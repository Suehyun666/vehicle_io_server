# vehicle_io_server — 아키텍처 문서

## 1. 전체 구조

```
┌─────────────────────────────────────────────────────────┐
│  Feature App  (demo_feature, 피처 N, ...)               │
│                                                         │
│   SUB socket ──→ 센서 데이터 수신                        │
│   REQ socket ──→ 액추에이터 명령 전송                    │
└──────────┬──────────────────────────────┬───────────────┘
           │ ipc:///tmp/sdv_sensor.ipc    │ ipc:///tmp/sdv_actuator.ipc
           ↓  (ZMQ PUB/SUB)              ↓  (ZMQ REQ/REP)
┌─────────────────────────────────────────────────────────┐
│  VehicleIOGateway  (ZMQ context 소유, 두 GW 조립)       │
│                                                         │
│  ┌──────────────────────┐  ┌──────────────────────────┐ │
│  │  SensorGateway       │  │  ActuatorGateway         │ │
│  │  [백그라운드 스레드]  │  │  [메인 스레드 블로킹]    │ │
│  │                      │  │                          │ │
│  │  PUB socket bind     │  │  REP socket bind         │ │
│  │  드라이버 폴링        │  │  JSON 파싱               │ │
│  │  multipart 발행       │  │  driver.write() 위임     │ │
│  └──────────┬───────────┘  └──────────────┬───────────┘ │
└─────────────┼─────────────────────────────┼─────────────┘
              │           DeviceManager      │
              ↓                             ↓
     sensor_drivers_[]       actuator_drivers_["actuator/servo1"]
                                            ["actuator/relay"]
              │                             │
              └──────────────┬──────────────┘
                             ↓
┌─────────────────────────────────────────────────────────┐
│  Drivers                                                │
│                                                         │
│  TcaSidestickDriver          ArduinoSerialDriver        │
│  (ISensorDriver)             (ISensorDriver +           │
│                               IActuatorDriver)          │
└──────────────┬───────────────────────────┬──────────────┘
               │ Linux joystick API        │ POSIX termios
               ↓                           ↓
         /dev/input/js0              /dev/ttyACM0
         TCA Sidestick               Arduino UNO
                                    ┌────────────────┐
                                    │  HC-SR04 (센서) │
                                    │  Servo   (액추) │
                                    │  Relay   (액추) │
                                    └────────────────┘
```

---

## 2. 데이터 흐름

### 센서 방향 (Hardware → Feature)

```
Hardware
  → Driver.isDataAvailable() / read()     [드라이버가 OS에서 읽음]
  → SensorGateway.pollLoop()              [백그라운드 스레드 순회]
  → SensorGateway.publish()              [ZMQ PUB 멀티파트 전송]
  → ZMQ IPC                              [topic + JSON payload]
  → Feature SUB socket                   [구독 필터 매칭]
```

### 명령 방향 (Feature → Hardware)

```
Feature REQ socket
  → ZMQ IPC
  → ActuatorGateway.listenLoop()         [메인 스레드, zmq::poll 10ms timeout]
  → JSON 파싱 → ActuatorCommand
  → DeviceManager.findActuatorDriver()   [target 문자열로 드라이버 조회]
  → Driver.write()                       [OS에 쓰기]
  → JSON 응답 (success / message)
  → Feature REP 수신
```

---

## 3. 인터페이스 계층과 다이아몬드 문제

```
          IDriver
         (connect / disconnect / getId)
         /                \
  ISensorDriver      IActuatorDriver
  (isDataAvailable   (write)
   read)
         \                /
      ArduinoSerialDriver
```

`ArduinoSerialDriver`는 같은 물리 장치(Arduino)에서 센서 읽기와 액추에이터 쓰기를 모두 담당합니다.
두 인터페이스를 동시에 상속하면 `IDriver`의 복사본이 두 개 생기는 **다이아몬드 문제**가 발생합니다.

`ISensorDriver`와 `IActuatorDriver`를 `public virtual IDriver`로 선언해 `IDriver` 인스턴스를 하나로 공유합니다.

`TcaSidestickDriver`는 센서 전용이므로 가상 상속 없이 `ISensorDriver`만 상속합니다.

---

## 4. SensorGateway 폴링 구조

```cpp
// 백그라운드 스레드 (SensorGateway::pollLoop)
while (running_) {
    bool any_data = false;
    for (auto& driver : sensor_drivers_) {
        if (driver->isDataAvailable()) {
            publish(driver->read());   // ZMQ PUB 발행
            any_data = true;
        }
    }
    if (!any_data) sleep(1ms);         // 데이터 없으면 CPU 양보
}
```

드라이버별 `isDataAvailable()` 구현이 다릅니다.

| 드라이버 | isDataAvailable() 방식 |
|---|---|
| TcaSidestickDriver | `poll(fd, 0ms)` — 커널 HID 이벤트 큐 확인 |
| ArduinoSerialDriver | `tryReadLines()` — 시리얼 버퍼 직접 소비 후 내부 큐 확인 |

TCA는 변화가 있을 때만 HID 이벤트가 생성됩니다 (edge-triggered).
Arduino는 20Hz 주기로 데이터를 전송합니다 (cyclic, 50ms 간격).

---

## 5. ZMQ PUB/SUB — 구독 등록/해제

```
SensorGateway          libzmq (내부)          Feature App
[PUB bind]                                    [SUB connect]

publish("sensor/tca/stick", payload)          sub.set(ZMQ_SUBSCRIBE, "sensor/tca")
     │                                              │
     └──── 전송 요청 ────→ 구독 필터 매칭 ────→ 전달
                            prefix 비교만 수행

                                              sub.set(ZMQ_UNSUBSCRIBE, "sensor/tca")
                                                   → 이후 메시지 차단
```

**서버(publisher)에는 등록/해제 로직이 없습니다.**

ZMQ PUB/SUB에서 구독 관리는 subscriber 쪽과 libzmq 내부에서만 일어납니다.

- publisher는 항상 모든 메시지를 발행합니다.
- libzmq가 각 subscriber의 필터(`ZMQ_SUBSCRIBE` 값)와 topic을 prefix 비교해 전달 여부를 결정합니다.
- subscriber가 연결을 끊으면 자동으로 구독이 사라집니다.
- 서버 재시작 없이 피처 앱을 자유롭게 붙이고 떼는 것이 가능합니다.

멀티파트 메시지 형식:
```
Frame[0]: topic   e.g. "sensor/tca/stick"
Frame[1]: payload JSON string
```

---

## 6. DeviceManager 설계

| 메서드 | 반환 타입 | 이유 |
|---|---|---|
| `getSensorDrivers()` | `const vector&` | SensorGateway가 전체 순회 필요 |
| `findActuatorDriver(target)` | `shared_ptr` | 명령별로 target 하나를 O(1) 조회 |

센서는 "등록된 전부를 매 루프마다 폴링", 액추에이터는 "이름으로 하나를 빠르게 찾기"라는
용도 차이가 반영된 것입니다.

---

## 7. 알려진 미비점 (TODO)

| 항목 | 위치 | 내용 |
|---|---|---|
| baud rate 무시 버그 | `ArduinoSerialDriver.cpp:39` | `B115200` 하드코딩, `baud_` 미반영 |
| g_shutdown 미사용 | `main.cpp:9` | signal handler에서 세팅하나 아무도 읽지 않음 |
| line_buf_ 무한 성장 | `ArduinoSerialDriver.cpp:114` | 최대 길이 제한 없음 |
| config 로딩 미구현 | `DeviceManager.cpp:4` | loadFromConfig()가 stub |
| IPC 엔드포인트 중복 | GW 헤더, sub_test, demo_feature | 공통 헤더로 분리 필요 |
| VehicleIOGateway::stop() 없음 | `VehicleIOGateway.cpp:26` | run() 반환 시 센서 스레드 명시적 종료 없음 |
| 단위 테스트 없음 | — | 파싱 로직 등 자동화 테스트 부재 |
