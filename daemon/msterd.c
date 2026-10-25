#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "ipc_protocol.h"

int mster_inject_library(pid_t target_pid, const char *so_path);
int mster_unload_library(pid_t target_pid, const char *so_path);
// 나중에 ptrace/remote dlopen 코드가 들어갈 자리
static int handle_injection(int pid, const char *path) {
    printf("[msterd] PID %d 에 라이브러리 주입 시작: %s\n", pid, path);
    return mster_inject_library(pid, path);
}

int main(void) {
    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("[msterd] 소켓 생성 실패");
        return 1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;

    // 추상 네임스페이스 소켓 설정 (첫 바이트 '\0')
    memcpy(addr.sun_path, MSTER_SOCKET_NAME, sizeof(MSTER_SOCKET_NAME));

    // 바인드 (sun_path 길이 계산 시 첫 바이트 null 포함)
    socklen_t addr_len = sizeof(sa_family_t) + sizeof(MSTER_SOCKET_NAME) - 1;
    if (bind(server_fd, (struct sockaddr *)&addr, addr_len) < 0) {
        perror("[msterd] 소켓 바인드 실패");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 5) < 0) {
        perror("[msterd] listen 실패");
        close(server_fd);
        return 1;
    }

    printf("[msterd] 온디맨드 IPC 데몬 대기 중...\n");

    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) continue;

        mster_request_t req;
        mster_response_t res;
        memset(&res, 0, sizeof(res));

        ssize_t bytes = read(client_fd, &req, sizeof(req));
        if (bytes == sizeof(req)) {
            if (req.cmd == CMD_PING) {
                res.status = 0;
                snprintf(res.message, sizeof(res.message), "Msterkit Daemon Alive!");
            } else if (req.cmd == CMD_INJECT_LIB) {
                int ret = handle_injection(req.target_pid, req.payload_path);
                res.status = ret;
                snprintf(res.message, sizeof(res.message), ret == 0 ? "Injection Triggered" : "Injection Failed");
            } else if (req.cmd == CMD_UNLOAD_LIB) {
                int ret = mster_unload_library(req.target_pid, req.payload_path);
                res.status = ret;
                snprintf(res.message, sizeof(res.message), ret == 0 ? "Unload Succeeded" : "Unload Failed");
            } else {
                res.status = -1;
                snprintf(res.message, sizeof(res.message), "Unknown Command");
            }
        } else {
            res.status = -2;
            snprintf(res.message, sizeof(res.message), "Malformed Packet");
        }

        write(client_fd, &res, sizeof(res));
        close(client_fd);
    }

    close(server_fd);
    return 0;
}