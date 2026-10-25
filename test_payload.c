#include <stdio.h>
#include <unistd.h>

// 라이브러리가 dlopen으로 메모리에 적재될 때 자동 실행
__attribute__((constructor))
void on_inject(void) {
    printf("\n[Payload] *************************************\n");
    printf("[Payload] Msterkit 원격 주입 성공! (PID: %d)\n", getpid());
    printf("[Payload] *************************************\n\n");
}