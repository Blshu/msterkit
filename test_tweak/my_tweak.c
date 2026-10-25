#include <stdio.h>
#include <unistd.h>
#include <msterkit.h>

// 타겟의 sleep 함수 원본 포인터
static unsigned int (*orig_sleep)(unsigned int seconds) = NULL;

// sleep을 가로챌 훅 함수
unsigned int hooked_sleep(unsigned int seconds) {
    printf("[Tweak] sleep(%u) 호출 가로챔! 대기 시간 조작 없음 :)\n", seconds);
    return orig_sleep(seconds);
}

// 라이브러리 로드 시 자동 실행
__attribute__((constructor))
void tweak_init(void) {
    printf("[Tweak] libmsterkit 기반 트윅 로드됨! 후킹 시도...\n");
    if (MsterHookFunction((void *)sleep, (void *)hooked_sleep, (void **)&orig_sleep)) {
        printf("[Tweak] sleep 함수 후킹 성공!\n");
    }
}

// 라이브러리 언로드 시 자동 실행
__attribute__((destructor))
void tweak_cleanup(void) {
    printf("[Tweak] 트윅 언로드 완료 및 정리됨!\n");
}