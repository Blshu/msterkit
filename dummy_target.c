#include <stdio.h>
#include <unistd.h>

int main(void) {
    // stdout 버퍼링 끄기 (또는 매 printf 뒤에 fflush)
    setbuf(stdout, NULL);

    printf("[Target] 시작됨. PID: %d\n", getpid());
    fflush(stdout);

    int count = 0;
    while (1) {
        printf("[Target] 실행 중... (%d초)\n", ++count);
        fflush(stdout);
        sleep(1);
    }

    return 0;
}