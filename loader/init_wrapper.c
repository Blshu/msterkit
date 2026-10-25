#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/input.h>

#define REAL_INIT_PATH "/init.real"
#define KEY_VOLUMEDOWN 114

// 리눅스 input 이벤트를 검사하여 특정 키가 눌려있는지 확인
static bool is_key_pressed(int target_key) {
    DIR *dir = opendir("/dev/input");
    if (!dir) return false;

    struct dirent *entry;
    bool pressed = false;

    // /dev/input 디렉터리 내의 모든 event 노드 순회
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "event", 5) != 0) continue;

        char path[256];
        snprintf(path, sizeof(path), "/dev/input/%s", entry->d_name);

        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;

        // 키 상태 비트맵 조회 (EVIOCGKEY)
        uint8_t key_bitmask[KEY_MAX / 8 + 1] = {0};
        if (ioctl(fd, EVIOCGKEY(sizeof(key_bitmask)), key_bitmask) >= 0) {
            // 해당 키의 비트가 세팅되어 있는지 확인
            if (key_bitmask[target_key / 8] & (1 << (target_key % 8))) {
                pressed = true;
                close(fd);
                break;
            }
        }
        close(fd);
    }

    closedir(dir);
    return pressed;
}

int main(int argc, char *argv[], char *envp[]) {
    // 1. 하드웨어 볼륨 다운 키 입력 확인 (안전 모드 검사)
    if (is_key_pressed(KEY_VOLUMEDOWN)) {
        // 긴급 탈출: 아무 작업도 하지 않고 순정 /init.real 호출
        execve(REAL_INIT_PATH, argv, envp);
        _exit(1);
    }

    // 2. Msterkit 백그라운드 서비스 프로세스 분기
    pid_t pid = fork();
    if (pid == 0) {
        // 자식 프로세스: 온디맨드 데몬 구동 (추후 daemon 바이너리로 대체)
        setsid();
        // TODO: msterd 데몬 구동 로직 위치
        _exit(0);
    }

    // 3. 부모 프로세스: 정상 안드로이드 부팅 파이프라인으로 전환
    execve(REAL_INIT_PATH, argv, envp);

    // execve가 실패했을 경우 비상 종료
    _exit(1);
}