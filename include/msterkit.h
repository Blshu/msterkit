#ifndef MSTERKIT_H
#define MSTERKIT_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ARM64 인라인 후킹 API
// symbol_addr: 후킹할 대상 함수의 메모리 주소
// new_func: 대체 실행할 훅 함수 주소
// old_func: 원본 함수를 호출할 수 있는 트램펄린 백업 포인터 주소
bool MsterHookFunction(void *symbol_addr, void *new_func, void **old_func);

#ifdef __cplusplus
}
#endif

#endif // MSTERKIT_H