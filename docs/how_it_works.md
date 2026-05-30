# 시스템 동작 방식

## 한 줄 요약

하드웨어 → HAL → Signal Bus → Feature → Signal Bus → HAL → 하드웨어

---

## 컴포넌트

```
vehicle_io_server  (HAL 노드, 서버)
manual_control     (피처, 클라이언트)
sub_test           (디버그 도구)
```

셋 다 별도 프로세스입니다. ZMQ IPC로 연결합니다.

---

## Signal Bus

ZMQ PUB/SUB 소켓 두 개가 버스 역할을 합니다.

```
sdv_hal.ipc   — HAL이 바인드 (Publisher)
sdv_feat.ipc  — Feature가 바인드 (Publisher)
```

누구든 PUB 쪽에 바인드하고 SUB 쪽을 connect하면 버스에 참여합니다.

---

## 신호 이름 체계

`include/signals/names.hpp`에 정의되어 있습니다.

```
/sensor/front_distance   float   HAL 발행  — HC-SR04 실측 거리 (cm)
/sensor/twist            float   HAL 발행  — TCA 스틱 twist 축 [-1.0, +1.0]
/sensor/throttle         float   HAL 발행  — TCA 스로틀 정규화 [0.0, 1.0]

/request/steering        float   Feature 발행 — 원하는 조향 [-1.0 좌, +1.0 우]
/request/speed           float   Feature 발행 — 원하는 속도 [-1.0, +1.0]
/request/emergency_brake bool    Feature 발행 — 긴급 제동 요청
```

`/sensor/*` 는 실측값, `/request/*` 는 피처가 원하는 값입니다.
Arbiter가 추가되면 `/command/*` 가 중재된 최종 명령이 됩니다.

---

## 센서 데이터 흐름

```
[하드웨어]
  TCA Sidestick  (/dev/input/js0)
  HC-SR04        (Arduino /dev/ttyACM0 경유)

[ISensorDriver]
  TcaSidestickDriver.read()   → SensorData { topic="tca/stick",    payload=JSON }
  ArduinoSerialDriver.read()  → SensorData { topic="arduino/dist", payload=JSON }

[HalNode::sensorLoop — 백그라운드 스레드]
  driver.isDataAvailable() 확인 (논블로킹)
  driver.read() 호출
  handler map 조회:
    "tca/stick"    → JSON 파싱 → Publisher.publish("/sensor/twist",    twist)
                               → Publisher.publish("/sensor/throttle", throttle)
    "arduino/dist" → JSON 파싱 → Publisher.publish("/sensor/front_distance", dist_cm)

[ZMQ sdv_hal.ipc]
  멀티파트 메시지: ["/sensor/twist"] [{"v":0.3}]

[Subscriber.poll()]
  topic prefix 매칭 → onFloat("/sensor/twist", callback) 실행
```

---

## 액추에이터 명령 흐름

```
[Feature — manual_control]
  Subscriber.onFloat("/sensor/twist", [](float twist) {
      Publisher.publish("/request/steering", twist)
  })

[ZMQ sdv_feat.ipc]
  멀티파트 메시지: ["/request/steering"] [{"v":0.3}]

[HalNode::sensorLoop — cmd_sub_.poll(0ms)]
  onFloat("/request/steering", [](float v) {
      각도 변환: (v + 1) / 2 * 180
      driver->write({"actuator/servo1", angle, "hal"})
  })

[IActuatorDriver]
  ArduinoSerialDriver.write() → 시리얼: {"target":"servo1","value":90}\n

[Arduino]
  handleCommand() → servo.write(90)
```

---

## 메시지 포맷

모든 신호는 ZMQ 멀티파트 2프레임입니다.

```
Frame 0 : 신호 이름 (UTF-8 string)   e.g. "/sensor/twist"
Frame 1 : JSON 값   {"v": <value>}    e.g. {"v": 0.3}
```

float과 bool 모두 같은 포맷입니다. JSON이라 sub_test로 바로 읽을 수 있습니다.

---

## 스레드 구조

```
vehicle_io_server 프로세스

main thread
  └── while(running) sleep(1s)   ← 종료 신호 대기만

sensorLoop thread  (HalNode 소유)
  ├── driver 폴링 (논블로킹)
  ├── sensor signal 발행
  └── cmd_sub_.poll(0ms)  ← 명령 수신도 같이 처리

manual_control 프로세스

main thread
  └── while(running) sensor.poll(10ms)  ← 이벤트 루프
```

---

## 실행 방법

```bash
# Arduino에 vehicle_node.ino 업로드 후

# 터미널 1 — HAL 노드
./cmake-build-debug/vehicle_io_server

# 터미널 2 — 피처
./cmake-build-debug/manual_control

# 터미널 3 — 디버그 (선택)
./cmake-build-debug/sub_test
# sub_test는 sdv_sensor.ipc를 구독하므로 아직 sdv_hal.ipc와 맞지 않음
# endpoints.hpp의 kHalPub으로 수정 필요
```

---

## 새 피처 추가

`features/` 에 새 파일을 만들고 CMakeLists.txt에 타겟 추가합니다.

```cpp
// features/obstacle_stop.cpp
#include "bus/Publisher.hpp"
#include "bus/Subscriber.hpp"
#include "signals/names.hpp"
#include "endpoints.hpp"

int main() {
    zmq::context_t ctx{1};
    Subscriber sensor (ctx, endpoints::kHalPub);
    Publisher  request(ctx, endpoints::kFeatPub);

    sensor.onFloat(signals::kFrontDistance, [&](float dist) {
        request.publish(signals::kReqEmergency, dist > 0.0f && dist < 15.0f);
    });

    while (true) sensor.poll(std::chrono::milliseconds(10));
}
```

```cmake
# CMakeLists.txt에 추가
add_executable(obstacle_stop features/obstacle_stop.cpp)
target_include_directories(obstacle_stop PRIVATE ${COMMON_INCLUDES})
target_link_libraries(obstacle_stop PRIVATE sdv_bus ${COMMON_LIBS})
target_compile_options(obstacle_stop PRIVATE ${COMMON_OPTS})
```

---

## 새 센서 추가

드라이버를 만들고 HalNode에 handler를 등록합니다.

```cpp
// 1. DeviceManager에 드라이버 등록 (main.cpp)
dm.registerSensorDriver(std::make_shared<ImuDriver>("/dev/imu0"));

// 2. HalNode::sensorLoop handler map에 추가 (HalNode.cpp)
handlers["imu/accel"] = [&](const SensorData& d) {
    auto j = json::parse(d.payload);
    sensor_pub_.publish("/sensor/accel_x", j.value("ax", 0.0f));
    sensor_pub_.publish("/sensor/accel_y", j.value("ay", 0.0f));
};

// 3. signals/names.hpp에 신호 이름 추가
inline constexpr const char* kAccelX = "/sensor/accel_x";
inline constexpr const char* kAccelY = "/sensor/accel_y";
```

---

## 현재 없는 것 (추가 예정)

| 기능 | 설명 |
|---|---|
| Arbiter | `/request/*` 중재 → `/command/*` 발행, 다중 피처 충돌 해소 |
| FlagProvider | 피처 on/off 런타임 제어 |
| Feature base class | `initialize / run / onKill` SDK |
| sub_test 업데이트 | sdv_hal.ipc 구독으로 변경 |
| L293D 모터 드라이버 | ArduinoSerialDriver + firmware 확장 |
