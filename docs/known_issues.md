# 알려진 미비점 및 개선 항목

## 우선순위 분류

### 🔴 즉시 수정 — 버그

#### 1. Baud rate 파라미터 무시

**위치:** `src/drivers/ArduinoSerialDriver.cpp:39-40`

```cpp
// 현재 — baud_ 무시, 항상 115200
cfsetispeed(&tty, B115200);
cfsetospeed(&tty, B115200);
```

생성자에 `baud=9600`을 넘겨도 silently 115200으로 열립니다. `baud_` 멤버는 로그 출력에만 쓰이고 실제 termios 설정에는 반영되지 않습니다.

**수정 방향:**
```cpp
speed_t speed = baudToSpeed(baud_);  // B9600, B115200 등으로 변환
cfsetispeed(&tty, speed);
cfsetospeed(&tty, speed);
```

---

#### 2. `g_shutdown` 원자 변수가 미사용 (dead code)

**위치:** `main.cpp:9,13`

```cpp
static std::atomic<bool> g_shutdown{false};
static void signalHandler(int) {
    g_shutdown = true;
    // TODO: gateway.stop() 호출 경로 추가
}
```

`g_shutdown`은 세팅만 되고 아무 데도 읽히지 않습니다. 실제 종료는 `listenLoop`가 `EINTR`을 받아 우연히 동작하는 것입니다.

**수정 방향:** signal handler에서 `gateway.stop()` → `sensor_gw_->stop()` 호출 경로를 연결하거나, `VehicleIOGateway`에 `stop()` 메서드를 추가합니다.

---

#### 3. `line_buf_` 무한 성장

**위치:** `src/drivers/ArduinoSerialDriver.cpp:114-130`

Arduino가 `\n` 없이 계속 데이터를 보내면 `line_buf_`가 메모리를 무한 점유합니다. 최대 길이 제한이 없습니다.

**수정 방향:**
```cpp
} else if (c != '\r') {
    if (line_buf_.size() < 256)  // 최대 256바이트 제한
        line_buf_ += c;
    else
        line_buf_.clear();  // 오염된 프레임 버림
}
```

---

#### 4. `tcsetattr()` 반환값 무시

**위치:** `src/drivers/ArduinoSerialDriver.cpp:55`

```cpp
tcsetattr(fd_, TCSANOW, &tty);  // 반환값 미확인
```

시리얼 설정 실패 시 감지가 안 됩니다.

**수정 방향:**
```cpp
if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
    std::cerr << "[ArduinoSerialDriver] tcsetattr failed: " << std::strerror(errno) << "\n";
    ::close(fd_); fd_ = -1; return false;
}
```

---

### 🟠 설계 문제 — 중요도 높음

#### 5. `getSensorDrivers()`가 내부 벡터를 non-const 레퍼런스로 반환

**위치:** `include/DeviceManager.hpp:32`, `src/DeviceManager.cpp:33`

```cpp
// 현재 — 외부에서 sensor_drivers_ 수정 가능
std::vector<std::shared_ptr<ISensorDriver>>& getSensorDrivers();
```

**수정 방향:**
```cpp
const std::vector<std::shared_ptr<ISensorDriver>>& getSensorDrivers() const;
```

`SensorGateway::pollLoop()`의 호출부도 `const auto&`로 바꿔야 합니다.

---

#### 6. `VehicleIOGateway::run()` — 센서 스레드 명시적 종료 없음

**위치:** `src/VehicleIOGateway.cpp:26-29`

```cpp
void VehicleIOGateway::run() {
    sensor_gw_->start();
    actuator_gw_->listenLoop();  // 반환 후 sensor_gw_ 스레드가 여전히 살아있음
    // sensor_gw_->stop() 없음 — 소멸자에 의존
}
```

소멸자 체인이 정상 작동하긴 하나, run() 반환과 스레드 종료 사이 시간차가 있습니다. 로그도 출력되지 않아 종료 시 상태를 알기 어렵습니다.

**수정 방향:**
```cpp
void VehicleIOGateway::run() {
    sensor_gw_->start();
    actuator_gw_->listenLoop();
    sensor_gw_->stop();  // 명시적 종료
}
```

---

#### 7. IPC 엔드포인트 주소가 3곳에 중복 하드코딩

| 파일 | 위치 |
|---|---|
| `include/VehicleIOGateway.hpp:23-24` | kSensorPubEndpoint, kActuatorRepEndpoint |
| `tools/sub_test.cpp:7` | `"ipc:///tmp/sdv_sensor.ipc"` 리터럴 |
| `tools/demo_feature.cpp:27-28` | SENSOR_EP, ACTUATOR_EP |

**수정 방향:** `include/sdv_endpoints.hpp` 공통 헤더로 분리.

```cpp
// include/sdv_endpoints.hpp
#pragma once
namespace sdv {
    inline constexpr const char* kSensorPub   = "ipc:///tmp/sdv_sensor.ipc";
    inline constexpr const char* kActuatorRep = "ipc:///tmp/sdv_actuator.ipc";
}
```

---

### 🟡 개선 사항 — 중요도 중간

#### 8. `write()`에서 value를 int로 강제 캐스팅

**위치:** `src/drivers/ArduinoSerialDriver.cpp:101`

```cpp
json j = {{"target", tgt}, {"value", static_cast<int>(cmd.value)}};
```

서보 각도는 정수가 맞지만, 다른 액추에이터에서 float precision이 필요할 경우 손실이 생깁니다.

---

#### 9. 기기 경로가 main.cpp에 하드코딩

**위치:** `main.cpp:29,33`

`/dev/input/js0`, `/dev/ttyACM0` — 다른 시스템에서 실행하려면 재컴파일이 필요합니다. `loadFromConfig()`가 TODO로 방치되어 있습니다.

**수정 방향:** 최소한 CLI 인자나 환경 변수로 경로를 받거나, `loadFromConfig()`를 구현합니다.

---

#### 10. Arduino — `pulseIn()`이 블로킹

**위치:** `arduino/vehicle_node/vehicle_node.ino:92`

```cpp
long us = pulseIn(PIN_ECHO, HIGH, TIMEOUT_US);  // 최대 25ms 블로킹
```

이 25ms 동안 Arduino는 Serial 명령을 처리하지 못합니다. 명령이 빠르게 들어오면 버퍼 오버플로우가 발생할 수 있습니다.

**수정 방향:** 인터럽트 + 타이머 캡처 방식으로 교체.

---

#### 11. 아두이노 `StaticJsonDocument<32>` 사이즈 여유 없음

**위치:** `arduino/vehicle_node/vehicle_node.ino:69`

`{"dist":45.2}` = 최대 18바이트. 32는 아슬아슬합니다. `{"dist":400.0}` = 14바이트, float 정밀도에 따라 달라집니다.

**수정 방향:** 64로 올립니다.

---

### 🔵 누락 항목

#### 12. `.gitignore` 없음

`cmake-build-debug/` 전체가 git에 추적됩니다. 빌드 아티팩트 커밋 위험.

**수정 방향:**
```gitignore
cmake-build-debug/
cmake-build-release/
.venv/
*.o
*.a
```

#### 13. 단위 테스트 없음

`sub_test.cpp`, `demo_feature.cpp`는 수동 통합 도구입니다. 드라이버 파싱 로직(`parseLine`, `buildPayload`) 등의 자동화 테스트가 없습니다.

#### 14. `VehicleIOGateway`에 `stop()` 퍼블릭 메서드 없음

외부(main)에서 명시적으로 종료를 요청하는 경로가 없습니다. signal handler에서 호출할 수 없습니다.

#### 15. 센서 확장 시 폴링 루프 병목

드라이버가 늘어나면 단일 pollLoop의 순회 시간이 길어져 레이턴시가 증가합니다. 상세 내용은 `design_decisions.md` 섹션 1 참조.

---

## 수정 우선순위 요약

| 우선순위 | 항목 | 난이도 |
|---|---|---|
| 즉시 | 1 baud rate 버그 | 낮음 |
| 즉시 | 3 line_buf_ 무한 성장 | 낮음 |
| 즉시 | 4 tcsetattr 반환값 | 낮음 |
| 조만간 | 2 g_shutdown 미사용 + stop() 경로 | 중간 |
| 조만간 | 5 getSensorDrivers const | 낮음 |
| 조만간 | 6 run()에서 sensor stop 명시 | 낮음 |
| 조만간 | 7 IPC 엔드포인트 중복 | 낮음 |
| 조만간 | 10 pulseIn 블로킹 (Arduino) | 중간 |
| 여유 시 | 9 config 로딩 구현 | 높음 |
| 여유 시 | 12 .gitignore 추가 | 낮음 |
| 여유 시 | 13 단위 테스트 추가 | 높음 |
| 여유 시 | 15 폴링 루프 epoll 전환 | 높음 |
