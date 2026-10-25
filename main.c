#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>

// MsterHookFunction 함수 프로토타입 선언 추가!
bool MsterHookFunction(void *symbol_addr, void *new_func, void **old_func);

// 원본 함수 (타겟)
__attribute__((noinline))
int target_calculate(int a, int b) {
    asm volatile("nop; nop; nop; nop;");
    return a + b;
}

// 원본 함수를 호출하기 위한 함수 포인터 백업
static int (*orig_target_calculate)(int a, int b) = NULL;

// 우리가 대신 실행할 훅 함수
int hooked_calculate(int a, int b) {
    printf("[Msterkit] hooked_calculate 호출됨! (a=%d, b=%d)\n", a, b);

    int original_result = orig_target_calculate(a, b);
    printf("[Msterkit] 원본 결과: %d -> 조작된 결과 반환!\n", original_result);

    return original_result * 10;
}

int main() {
    printf("--- 후킹 전 실행 ---\n");
    printf("10 + 20 = %d\n", target_calculate(10, 20));

    printf("\n--- Msterkit 훅 설치 중... ---\n");
    if (MsterHookFunction((void *)target_calculate, (void *)hooked_calculate, (void **)&orig_target_calculate)) {
        printf("훅 설치 성공!\n");
    } else {
        printf("훅 설치 실패...\n");
        return 1;
    }

    printf("\n--- 후킹 후 실행 ---\n");
    int result = target_calculate(10, 20);
    printf("최종 리턴값: %d\n", result);

    return 0;
}