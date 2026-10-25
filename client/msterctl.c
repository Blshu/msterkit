#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "ipc_protocol.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("사용법:\n");
        printf("  %s ping\n", argv[0]);
        printf("  %s inject <PID> <.so 경로>\n", argv[0]);
        return 1;
    }

    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("소켓 생성 실패");
        return 1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    memcpy(addr.sun_path, MSTER_SOCKET_NAME, sizeof(MSTER_SOCKET_NAME));

    socklen_t addr_len = sizeof(sa_family_t) + sizeof(MSTER_SOCKET_NAME) - 1;
    if (connect(sock, (struct sockaddr *)&addr, addr_len) < 0) {
        perror("데몬 연결 실패 (데몬이 실행 중인지 확인)");
        close(sock);
        return 1;
    }

    mster_request_t req;
    memset(&req, 0, sizeof(req));

    if (strcmp(argv[1], "ping") == 0) {
        req.cmd = CMD_PING;
    } else if (strcmp(argv[1], "inject") == 0 && argc >= 4) {
        req.cmd = CMD_INJECT_LIB;
        req.target_pid = atoi(argv[2]);
        strncpy(req.payload_path, argv[3], sizeof(req.payload_path) - 1);
    } else {
        printf("잘못된 명령어입니다.\n");
        close(sock);
        return 1;
    }

    write(sock, &req, sizeof(req));

    mster_response_t res;
    read(sock, &res, sizeof(res));
    printf("[응답 %d] %s\n", res.status, res.message);

    close(sock);
    return 0;
}