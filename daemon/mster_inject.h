#ifndef MSTER_INJECT_H
#define MSTER_INJECT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <elf.h>
#include <linux/elf.h>
#include <sys/mman.h>
#include <dlfcn.h>

// ARM64 레지스터 구조체
struct pt_regs_arm64 {
    uint64_t uregs[31]; // x0 - x30 (x30 = lr)
    uint64_t sp;
    uint64_t pc;
    uint64_t pstate;
};

// 특정 PID의 특정 모듈 베이스 주소 가져오기
static uintptr_t get_module_base(pid_t pid, const char *module_name) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/maps", pid);
    FILE *fp = fopen(path, "r");
    if (!fp) return 0;

    char line[512];
    uintptr_t base = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, module_name)) {
            base = strtoull(line, NULL, 16);
            break;
        }
    }
    fclose(fp);
    return base;
}

// 타겟 프로세스 내 원격 함수 주소 계산
static void *get_remote_func_addr(pid_t target_pid, const char *module_name, void *local_func_addr) {
    uintptr_t local_base = get_module_base(getpid(), module_name);
    uintptr_t target_base = get_module_base(target_pid, module_name);
    if (!local_base || !target_base) return NULL;

    return (void *)(target_base + ((uintptr_t)local_func_addr - local_base));
}

// ARM64 레지스터 읽기
static int ptrace_getregs(pid_t pid, struct pt_regs_arm64 *regs) {
    struct iovec io;
    io.iov_base = regs;
    io.iov_len = sizeof(*regs);
    return ptrace(PTRACE_GETREGSET, pid, (void *)NT_PRSTATUS, &io);
}

// ARM64 레지스터 쓰기
static int ptrace_setregs(pid_t pid, struct pt_regs_arm64 *regs) {
    struct iovec io;
    io.iov_base = regs;
    io.iov_len = sizeof(*regs);
    return ptrace(PTRACE_SETREGSET, pid, (void *)NT_PRSTATUS, &io);
}

// 메모리 쓰기 (문자열 또는 쉘코드 주입)
static int ptrace_write(pid_t pid, uintptr_t addr, const void *data, size_t size) {
    size_t words = (size + sizeof(long) - 1) / sizeof(long);
    long *src = (long *)data;
    for (size_t i = 0; i < words; i++) {
        if (ptrace(PTRACE_POKETEXT, pid, (void *)(addr + i * sizeof(long)), (void *)src[i]) < 0) {
            return -1;
        }
    }
    return 0;
}

#endif // MSTER_INJECT_H