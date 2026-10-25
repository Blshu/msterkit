#include "mster_inject.h"

#ifndef RTLD_NOLOAD
#define RTLD_NOLOAD 0x00004
#endif

// 원격 함수 단일 호출 (ARM64 규약)
static uint64_t ptrace_call_arm64(pid_t pid, uintptr_t func_addr, uint64_t *args, int argc) {
    struct pt_regs_arm64 regs, backup_regs;
    if (ptrace_getregs(pid, &backup_regs) < 0) return 0;
    memcpy(&regs, &backup_regs, sizeof(regs));

    // 1. 인자 세팅 (x0 ~ x7)
    for (int i = 0; i < argc && i < 8; i++) {
        regs.uregs[i] = args[i];
    }

    // 2. 반환 감지용 트랩 주소 세팅 (LR에 0을 넣고 호출하거나 트랩 영역 활용)
    // 여기서는 안전하게 LR을 0으로 설정 후 SIGSEGV/SIGILL/SIGTRAP 감지
    regs.uregs[30] = 0; // LR = 0
    regs.pc = func_addr;

    if (ptrace_setregs(pid, &regs) < 0) return 0;

    // 3. 프로세스 재개 후 함수 종료 대기
    int status;
    while (1) {
        if (ptrace(PTRACE_CONT, pid, NULL, NULL) < 0) return 0;
        waitpid(pid, &status, WUNTRACED);

        if (WIFSTOPPED(status)) {
            // 함수가 리턴하면서 LR(0) 점프 시도 -> SIGSEGV 발생 시 정상 복귀로 간주
            if (WSTOPSIG(status) == SIGSEGV || WSTOPSIG(status) == SIGTRAP) {
                break;
            }
        }
    }

    // 4. 리턴값(X0) 획득 및 레지스터 복원
    ptrace_getregs(pid, &regs);
    uint64_t ret_val = regs.uregs[0];

    ptrace_setregs(pid, &backup_regs);
    return ret_val;
}

// 실제 .so 주입 진입 함수
int mster_inject_library(pid_t target_pid, const char *so_path) {
    int status;

    // 1. 타겟 프로세스 Attach
    if (ptrace(PTRACE_ATTACH, target_pid, NULL, NULL) < 0) {
        perror("[mster_inject] PTRACE_ATTACH 실패");
        return -1;
    }
    waitpid(target_pid, &status, WUNTRACED);

    // 2. 필요한 원격 함수 주소 획득 (libc.so의 mmap / munmap)
    void *remote_mmap = get_remote_func_addr(target_pid, "libc.so", (void *)mmap);
    void *remote_munmap = get_remote_func_addr(target_pid, "libc.so", (void *)munmap);

    // 안드로이드 링커의 dlopen 주소 (libdl.so)
    void *remote_dlopen = get_remote_func_addr(target_pid, "libdl.so", (void *)dlopen);

    if (!remote_mmap || !remote_dlopen) {
        fprintf(stderr, "[mster_inject] 원격 함수 주소 찾기 실패 (mmap: %p, dlopen: %p)\n", remote_mmap, remote_dlopen);
        ptrace(PTRACE_DETACH, target_pid, NULL, NULL);
        return -2;
    }

    // 3. 타겟 프로세스 내부에 임시 메모리 4KB 할당 (mmap 호출)
    // mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0)
    uint64_t mmap_args[] = {
        0, 
        4096, 
        PROT_READ | PROT_WRITE, 
        MAP_ANONYMOUS | MAP_PRIVATE, 
        (uint64_t)-1, 
        0
    };
    uint64_t remote_buf = ptrace_call_arm64(target_pid, (uintptr_t)remote_mmap, mmap_args, 6);
    if (!remote_buf || (void *)remote_buf == MAP_FAILED) {
        fprintf(stderr, "[mster_inject] 타겟 메모리 mmap 실패\n");
        ptrace(PTRACE_DETACH, target_pid, NULL, NULL);
        return -3;
    }

    // 4. 할당된 버퍼에 주입할 .so 절대 경로 문자열 쓰기
    ptrace_write(target_pid, remote_buf, so_path, strlen(so_path) + 1);

    // 5. 원격 dlopen 호출 (dlopen(remote_buf, RTLD_NOW))
    uint64_t dlopen_args[] = { remote_buf, RTLD_NOW };
    uint64_t handle = ptrace_call_arm64(target_pid, (uintptr_t)remote_dlopen, dlopen_args, 2);

    // 6. 임시 버퍼 해제 (munmap)
    uint64_t munmap_args[] = { remote_buf, 4096 };
    ptrace_call_arm64(target_pid, (uintptr_t)remote_munmap, munmap_args, 2);

    // 7. Detach로 원래 프로세스 정상 구동 복귀
    ptrace(PTRACE_DETACH, target_pid, NULL, NULL);

    if (!handle) {
        fprintf(stderr, "[mster_inject] 원격 dlopen 반환 핸들 NULL (주입 실패)\n");
        return -4;
    }

    printf("[mster_inject] 주입 성공! (so handle: 0x%lx)\n", handle);
    return 0;
}

int mster_unload_library(pid_t target_pid, const char *so_path) {
    int status;

    if (ptrace(PTRACE_ATTACH, target_pid, NULL, NULL) < 0) {
        perror("[mster_inject] PTRACE_ATTACH 실패");
        return -1;
    }
    waitpid(target_pid, &status, WUNTRACED);

    void *remote_mmap = get_remote_func_addr(target_pid, "libc.so", (void *)mmap);
    void *remote_munmap = get_remote_func_addr(target_pid, "libc.so", (void *)munmap);
    void *remote_dlopen = get_remote_func_addr(target_pid, "libdl.so", (void *)dlopen);
    void *remote_dlclose = get_remote_func_addr(target_pid, "libdl.so", (void *)dlclose);

    if (!remote_mmap || !remote_dlopen || !remote_dlclose) {
        fprintf(stderr, "[mster_inject] 필수 원격 함수 탐색 실패\n");
        ptrace(PTRACE_DETACH, target_pid, NULL, NULL);
        return -2;
    }

    // 1. 임시 메모리 4KB 할당 후 경로 쓰기
    uint64_t mmap_args[] = { 0, 4096, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, (uint64_t)-1, 0 };
    uint64_t remote_buf = ptrace_call_arm64(target_pid, (uintptr_t)remote_mmap, mmap_args, 6);
    if (!remote_buf || (void *)remote_buf == MAP_FAILED) {
        ptrace(PTRACE_DETACH, target_pid, NULL, NULL);
        return -3;
    }
    ptrace_write(target_pid, remote_buf, so_path, strlen(so_path) + 1);

    // 2. dlopen(path, RTLD_NOLOAD) 호출로 현재 로드된 핸들 획득
    uint64_t dlopen_args[] = { remote_buf, RTLD_NOLOAD };
    uint64_t handle = ptrace_call_arm64(target_pid, (uintptr_t)remote_dlopen, dlopen_args, 2);

    // 임시 버퍼 즉시 해제
    uint64_t munmap_args[] = { remote_buf, 4096 };
    ptrace_call_arm64(target_pid, (uintptr_t)remote_munmap, munmap_args, 2);

    if (!handle) {
        fprintf(stderr, "[mster_inject] 대상 라이브러리가 타겟 메모리에 로드되어 있지 않음\n");
        ptrace(PTRACE_DETACH, target_pid, NULL, NULL);
        return -4;
    }

    // 3. dlclose 호출 (RTLD_NOLOAD 참조 1회 + 원래 주입 참조 1회 = 총 2회 호출)
    uint64_t dlclose_args[] = { handle };
    ptrace_call_arm64(target_pid, (uintptr_t)remote_dlclose, dlclose_args, 1);
    ptrace_call_arm64(target_pid, (uintptr_t)remote_dlclose, dlclose_args, 1);

    ptrace(PTRACE_DETACH, target_pid, NULL, NULL);
    printf("[mster_inject] 언로드 완료 (handle: 0x%lx)\n", handle);
    return 0;
}