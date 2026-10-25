#ifndef IPC_PROTOCOL_H
#define IPC_PROTOCOL_H

#include <stdint.h>

#define MSTER_SOCKET_NAME "\0msterd_ipc" // 추상 소켓 (첫 바이트가 NULL)

// 명령 타입
typedef enum {
    CMD_PING = 1,          // 데몬 생존 확인
    CMD_INJECT_LIB,        // 특정 PID에 .so 라이브러리 주입
    CMD_UNLOAD_LIB         // 라이브러리 언로드
} mster_cmd_t;

// 요청 패킷
typedef struct {
    uint32_t cmd;          // mster_cmd_t
    int32_t target_pid;    // 주입할 대상 프로세스 PID
    char payload_path[256];// 주입할 .so 파일의 절대 경로
} mster_request_t;

// 응답 패킷
typedef struct {
    int32_t status;        // 0: 성공, 음수: 에러 코드
    char message[128];     // 상태 메시지
} mster_response_t;

#endif // IPC_PROTOCOL_H