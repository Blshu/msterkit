#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#define JUMP_STUB_SIZE 16

// 메모리 페이지 권한을 쓰기 가능하도록 변경
static bool make_memory_writable(void *addr, size_t size) {
    long page_size = sysconf(_SC_PAGESIZE);
    uintptr_t start = (uintptr_t)addr & ~(page_size - 1);
    uintptr_t end = ((uintptr_t)addr + size + page_size - 1) & ~(page_size - 1);

    return mprotect((void *)start, end - start, PROT_READ | PROT_WRITE | PROT_EXEC) == 0;
}

// ARM64 점프 스텁 생성 (LDR X16, #8; BR X16; [64-bit Target])
static void write_jump_stub(void *dst, void *target) {
    uint32_t *code = (uint32_t *)dst;
    code[0] = 0x58000050; // ldr x16, #8
    code[1] = 0xd61f0200; // br x16
    *(uint64_t *)(&code[2]) = (uint64_t)target;

    // 캐시 동기화 (명령어 캐시와 데이터 캐시 불일치 방지)
    __builtin___clear_cache((char *)dst, (char *)dst + JUMP_STUB_SIZE);
}

// Msterkit 핵심 인라인 훅 함수
bool MsterHookFunction(void *symbol_addr, void *new_func, void **old_func) {
    if (!symbol_addr || !new_func) return false;

    // 1. 원본을 실행할 트램펄린 메모리 할당 (RWX 권한)
    // 원본 앞 16바이트 + 원래 함수 남은 부분으로 돌아가는 점프 스텁 16바이트 = 총 32바이트
    void *trampoline = mmap(NULL, JUMP_STUB_SIZE * 2, PROT_READ | PROT_WRITE | PROT_EXEC,
                            MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (trampoline == MAP_FAILED) {
        perror("mmap failed");
        return false;
    }

    // 2. 원본 함수의 첫 16바이트를 트램펄린으로 복사
    memcpy(trampoline, symbol_addr, JUMP_STUB_SIZE);

    // 3. 트램펄린 끝에 '원본 함수 + 16바이트 지점'으로 복귀하는 점프 스텁 작성
    void *return_addr = (void *)((uintptr_t)symbol_addr + JUMP_STUB_SIZE);
    write_jump_stub((void *)((uintptr_t)trampoline + JUMP_STUB_SIZE), return_addr);

    if (old_func) {
        *old_func = trampoline;
    }

    // 4. 원본 대상 함수 메모리 보호 해제
    if (!make_memory_writable(symbol_addr, JUMP_STUB_SIZE)) {
        perror("mprotect failed");
        munmap(trampoline, JUMP_STUB_SIZE * 2);
        return false;
    }

    // 5. 원본 대상 함수의 첫머리에 '훅 함수(new_func)'로 점프하는 코드 삽입
    write_jump_stub(symbol_addr, new_func);

    return true;
}