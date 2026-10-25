<div align="center">

<img src="./assets/icon.png" width="128" height="128" alt="Msterkit Icon" />

# Msterkit

**안드로이드 ARM64 온디맨드 인젝터 & 인라인 후킹 프레임워크**

![Architecture](https://img.shields.io/badge/Arch-ARM64-red?style=flat-square&logo=arm)
![Platform](https://img.shields.io/badge/Platform-Android%20NDK-3DDC84?style=flat-square&logo=android)
![Status](https://img.shields.io/badge/Status-Toy%20PoC-orange?style=flat-square)

</div>
안드로이드(ARM64) 환경에서 프로세스 메모리 조작과 인라인 후킹을 공부해보려고 만든 개인 토이 프로젝트입니다.

`ptrace`를 이용한 원격 `.so` 주입/언로드, ARM64 인라인 훅 엔진, 그리고 백그라운드에서 요청을 받아 처리하는 경량 IPC 데몬으로 구성되어 있습니다.

> **안내:** 순수 학습 및 실험용 PoC(Proof of Concept) 프로젝트입니다. 실제 상용 환경이나 보안 솔루션이 적용된 타겟에서는 예외 처리가 부족할 수 있습니다.

---

## 주요 구성 요소

* **Core Hook Engine (`libmsterkit.so`)**
  * ARM64 16바이트 트램펄린(`LDR X16, #8; BR X16`) 기반 인라인 후킹
  * 원본 함수 호출 백업 및 Instruction Cache 동기화(`__clear_cache`)
* **온디맨드 IPC 데몬 (`msterd`)**
  * 리눅스 추상 네임스페이스 유닉스 도메인 소켓(`\0msterd_ipc`) 기반 경량 서버
  * 클라이언트 요청에 따라 타겟 프로세스에 동적으로 라이브러리 주입 및 언로드 수행
* **원격 인젝터 / 언로더**
  * `ptrace`를 통해 타겟 프로세스 내 `mmap`, `dlopen`, `dlclose` 원격 호출
* **제어 도구 (`msterctl`)**
  * CLI 환경에서 데몬 상태 확인(`ping`), 라이브러리 주입(`inject`), 제거(`unload`) 명령 전송

---

## 🏗️ 빌드 방법

Android NDK(Clang, `aarch64-linux-android`) 환경에서 크로스 컴파일합니다.

### 1. NDK 툴체인 환경변수 설정

사용 중인 OS에 맞춰 터미널(bash/zsh)에서 경로를 지정합니다.

**Linux**
```bash
export ANDROID_NDK=~/Android/Sdk/ndk/<NDK버전>
export TOOLCHAIN=$ANDROID_NDK/toolchains/llvm/prebuilt/linux-x86_64
export CLANG=$TOOLCHAIN/bin/aarch64-linux-android34-clang
```
**macOS (Apple Silicon / Intel)**

```bash
export ANDROID_NDK=~/Library/Android/sdk/ndk/<NDK버전>
export TOOLCHAIN=$ANDROID_NDK/toolchains/llvm/prebuilt/darwin-x86_64
export CLANG=$TOOLCHAIN/bin/aarch64-linux-android34-clang
```

**Windows(PowerShell)**

```Powershell
$CLANG = "C:\Users\$env:USERNAME\AppData\Local\Android\Sdk\ndk\<NDK버전>\toolchains\llvm\prebuilt\windows-x86_64\bin\aarch64-linux-android34-clang.cmd"
```
> 💡 참고: 안드로이드 API 레벨은 필요에 따라 android30~android34 사이로 맞춰주면 됩니다.

### 2. 컴파일 명령
환경 변수를 잡았다면 Linux와 macOS 모두 동일한 명령어로 빌드됩니다.

```bash
# 1. 코어 인라인 후킹 공유 라이브러리 빌드
$CLANG -O2 -shared -fPIC -Iinclude src/mster_hook.c -o libmsterkit.so

# 2. 온디맨드 데몬 및 CLI 도구 빌드
$CLANG -O2 -fPIE -pie -I. daemon/msterd.c daemon/mster_inject.c -ldl -o msterd
$CLANG -O2 -fPIE -pie -I. client/msterctl.c -o msterctl

# 3. 테스트용 타겟 프로세스 및 트윅 빌드
$CLANG -O2 -fPIE -pie dummy_target.c -o dummy_target$CLANG -O2 -shared -fPIC -Iinclude test_tweak/my_tweak.c -L. -lmsterkit -o libmy_tweak.so
```

## 사용 예시
빌드한 바이너리와 라이브러리를 기기의 임시 디렉터리(/data/local/tmp/)에 올린 뒤 실행합니다.

```bash
# 1. 데몬 백그라운드 실행
/data/local/tmp/msterd &

# 2. 데몬 생존 확인
/data/local/tmp/msterctl ping

# 3. 타겟 프로세스에 트윅 주입
/data/local/tmp/msterctl inject <타겟_PID> /data/local/tmp/libmy_tweak.so

# 4. 트윅 언로드
/data/local/tmp/msterctl unload <타겟_PID> /data/local/tmp/libmy_tweak.so
```