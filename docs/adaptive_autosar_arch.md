# Adaptive AUTOSAR 기준 아키텍처 설계

## 1. Adaptive AUTOSAR 핵심 개념

Adaptive AUTOSAR는 SDV의 고성능 중앙 컴퓨터(Central Vehicle Computer)를 위한
소프트웨어 아키텍처 표준이다. Linux/QNX 위에서 동작하며, 각 소프트웨어 컴포넌트는
독립적인 POSIX 프로세스로 실행된다.

```
┌──────────────────────────────────────────────────┐
│  Adaptive Applications                           │
│  (feature_lka, feature_acc, hal_service, ...)   │
│  각각 독립 프로세스 — 격리, 독립 배포 가능         │
├──────────────────────────────────────────────────┤
│  ARA (AUTOSAR Runtime for Adaptive Applications) │
│                                                  │
│  ara::com   — 서비스 통신 (Proxy / Skeleton)     │
│  ara::exec  — 실행 생명주기 (시작/종료/상태)     │
│  ara::diag  — UDS 진단                          │
│  ara::log   — 구조화 로깅                        │
│  ara::phm   — 플랫폼 헬스 모니터링               │
├──────────────────────────────────────────────────┤
│  POSIX OS  (Linux, QNX 등)                       │
└──────────────────────────────────────────────────┘
```

이 프로젝트에서 Adaptive AUTOSAR의 각 요소에 대응되는 것:

| Adaptive AUTOSAR 개념 | 이 프로젝트 |
|---|---|
| Adaptive Application | vehicle_io_server, demo_feature |
| ara::com (Proxy/Skeleton) | ZMQ PUB/SUB + REQ/REP (단순화 버전) |
| ara::exec | main() + SIGTERM 핸들링 |
| HAL / Platform Driver | ISensorDriver / IActuatorDriver |
| Functional Cluster | SensorGateway, ActuatorGateway |

---

## 2. 서비스 인터페이스 — Proxy / Skeleton 패턴

Adaptive AUTOSAR의 핵심 통신 모델이다. 피처(소비자)와 서비스(제공자)는
**서비스 인터페이스 정의**만 공유하고, transport가 무엇인지 서로 모른다.

```
[Skeleton = 서비스 제공자]           [Proxy = 서비스 소비자]
vehicle_io_server 프로세스           demo_feature 프로세스

SensorService::Skeleton              SensorService::Proxy
  → StickData 이벤트 발행               ← StickData 이벤트 수신
  → DistData 이벤트 발행                ← DistData 이벤트 수신

ActuatorService::Skeleton            ActuatorService::Proxy
  ← SetServo(angle) 메서드 수신         → SetServo(angle) 메서드 호출
  ← SetRelay(on)    메서드 수신         → SetRelay(on)    메서드 호출
```

피처는 Proxy를 통해서만 서비스와 통신한다. ZMQ 엔드포인트, topic 이름, JSON 형식을
피처가 알 필요가 없다 — 이것들은 모두 Proxy/Skeleton 구현 내부에 숨겨진다.

### 현재 코드의 문제

demo_feature가 driver interface를 쓰지 않는 것은 올바르다.
그러나 ZMQ 엔드포인트와 JSON topic을 직접 알고 있는 것은 잘못이다.

```cpp
// demo_feature.cpp 현재 — transport에 직접 의존
static constexpr const char* SENSOR_EP = "ipc:///tmp/sdv_sensor.ipc";  // 알면 안 됨
sub.set(zmq::sockopt::subscribe, "sensor/tca/stick");                   // 알면 안 됨
float twist = j.value("twist", 0.0f);                                   // raw JSON 파싱
```

올바른 구조에서 피처 코드:

```cpp
// 목표 구조 — 피처는 서비스 인터페이스만 안다
SensorService::Proxy sensor_proxy;
sensor_proxy.StickData.Subscribe([](const StickData& data) {
    float twist = data.twist;           // 타입화된 구조체
    int angle = twistToAngle(twist);
    actuator_proxy.SetServo(angle);     // 메서드 호출
});
```

---

## 3. 전체 목표 아키텍처

```
┌─────────────────────────────────────────────────────────────────┐
│  Adaptive Applications (별도 프로세스)                          │
│                                                                 │
│  ┌──────────────────┐  ┌──────────────────┐                     │
│  │  demo_feature    │  │  future_feature  │  ...                │
│  │                  │  │                  │                     │
│  │  SensorProxy     │  │  SensorProxy     │                     │
│  │  ActuatorProxy   │  │                  │                     │
│  └──────────────────┘  └──────────────────┘                     │
└────────────────────────────┬────────────────────────────────────┘
                             │
              Service Interface (ara::com)
              현재 구현: ZMQ PUB/SUB + REQ/REP
              미래 확장: SOME-IP / DDS / vsomeip
                             │
┌────────────────────────────┼────────────────────────────────────┐
│  HAL Service  (vehicle_io_server — 하나의 Adaptive Application) │
│                                                                 │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │  Service Layer  (SensorService + ActuatorService)        │   │
│  │  Skeleton 역할 — ZMQ transport 소유                      │   │
│  │  드라이버를 직접 모름                                     │   │
│  └────────────────────────────┬─────────────────────────────┘   │
│                               │ thread-safe queue               │
│  ┌────────────────────────────┼─────────────────────────────┐   │
│  │  DriverManager             │                             │   │
│  │  드라이버 스레드/epoll 소유  │                             │   │
│  │  ZMQ를 모름                │                             │   │
│  │                            ↓                             │   │
│  │  ISensorDriver[] ←── epoll/poll dispatch                │   │
│  │  IActuatorDriver{} ←── 명령 큐에서 소비                  │   │
│  └──────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
                             │
              Driver Interface (ISensorDriver / IActuatorDriver)
                             │
┌────────────────────────────┼────────────────────────────────────┐
│  Drivers                   │                                    │
│  TcaSidestickDriver        │  ArduinoSerialDriver               │
└────────────────────────────┼────────────────────────────────────┘
                             │  OS API (fd, termios, HID, SocketCAN)
                          Hardware
```

---

## 4. 드라이버 스레드 관리

### 원칙

| 레이어 | 스레드 | 책임 |
|---|---|---|
| Service Layer | ZMQ 스레드 (PUB / REP 각 1개) | transport I/O만 담당 |
| DriverManager | epoll 스레드 or per-driver 스레드 | 드라이버 이벤트 감시 |
| Blocking Driver 내부 | 드라이버 자체 스레드 | ISensorDriver 인터페이스 뒤에 숨김 |

### 스레드 구조

```
vehicle_io_server 프로세스

main thread
  ├── DriverManager 초기화
  ├── ServiceLayer 초기화
  └── ActuatorService::listenLoop()    ← 블로킹 (main thread)

sensor_dispatch thread (DriverManager 소유)
  ├── epoll_wait(all sensor fds)
  ├── TcaSidestick fd ready → driver.read() → sensor_queue.push()
  └── ArduinoSerial fd ready → driver.read() → sensor_queue.push()

sensor_publish thread (SensorService 소유)
  └── sensor_queue.pop() → ZMQ PUB publish()

[블로킹 드라이버 추가 시 — 드라이버 내부]
CameraDriver::capture_thread (driver 내부 스레드)
  └── blocking camera.read() → internal_queue
        ↑ DriverManager의 isDataAvailable()이 non-blocking 확인
```

### 스레드 간 데이터 흐름

```
Hardware
  → [sensor fd 이벤트]
  → sensor_dispatch thread: driver.read()
  → thread-safe SensorQueue (lock-free ring buffer or mutex queue)
  → sensor_publish thread: queue.pop() → ZMQ PUB
  → Feature SUB socket
```

이 구조에서:
- DriverManager는 ZMQ를 모른다
- SensorService는 fd와 드라이버를 모른다
- 두 레이어는 queue로만 연결된다

### non-blocking 계약

DriverManager의 epoll 스레드가 driver.read()를 호출하므로,
드라이버의 `isDataAvailable()`과 `read()`는 절대 블로킹하지 않아야 한다.
블로킹 I/O가 필요한 드라이버(카메라, 무선 GPS 등)는 내부 스레드를 두고
인터페이스는 non-blocking으로 노출한다.

```cpp
// 블로킹 드라이버의 올바른 구조
class CameraDriver : public ISensorDriver {
    std::thread            capture_thread_;
    std::queue<SensorData> queue_;
    std::mutex             mutex_;

    void captureLoop() {          // 내부 스레드
        while (running_) {
            auto frame = camera_api_.read();  // 블로킹 — 여기서만
            std::lock_guard lk(mutex_);
            queue_.push(buildSensorData(frame));
        }
    }
public:
    // DriverManager epoll 스레드에서 호출 — non-blocking 보장
    bool isDataAvailable() override {
        std::lock_guard lk(mutex_);
        return !queue_.empty();
    }
    SensorData read() override {
        std::lock_guard lk(mutex_);
        auto d = queue_.front(); queue_.pop();
        return d;
    }
};
```

---

## 5. HAL이란 무엇인가

HAL (Hardware Abstraction Layer)은 두 경계에 존재한다.

```
[경계 A — 드라이버 인터페이스]
  피처 → HAL 서비스 (vehicle_io_server)
  ISensorDriver / IActuatorDriver

[경계 B — 서비스 인터페이스]
  HAL 서비스 내부 → Hardware
  피처는 경계 A만 보고, 경계 B를 모른다
```

현재 vehicle_io_server는 HAL 서비스 역할을 한다.
`ISensorDriver` / `IActuatorDriver`는 경계 B (HAL 내부, 드라이버 추상화) 인터페이스다.

demo_feature는 경계 A (ZMQ)를 통해서만 HAL과 통신하며,
ISensorDriver를 쓰지 않는 것이 올바르다.

---

## 6. 현재 구조와 목표 구조의 차이

| 항목 | 현재 | 목표 |
|---|---|---|
| 피처-HAL 통신 | ZMQ 직접 사용 | Proxy 클래스로 추상화 |
| 피처의 transport 의존 | 엔드포인트 하드코딩 | Proxy 내부에 숨김 |
| 피처의 데이터 형식 의존 | 직접 JSON 파싱 | 타입화된 구조체 |
| DriverManager-ServiceLayer | 직접 호출 | thread-safe queue 연결 |
| 드라이버 스레드 관리 | 단일 pollLoop | epoll + 블로킹 드라이버 내부화 |
| 서비스 생명주기 | main() 단순 시작 | ara::exec 스타일 상태 관리 |
| 설정 | 하드코딩 | config 파일 기반 |

---

## 7. 단계별 개선 로드맵

### Phase 1 — 서비스 인터페이스 분리 (즉시)

```cpp
// include/services/SensorServiceSkeleton.hpp
// include/services/SensorServiceProxy.hpp
// include/services/ActuatorServiceSkeleton.hpp
// include/services/ActuatorServiceProxy.hpp
```

피처가 ZMQ를 직접 보는 대신 Proxy 클래스를 사용하도록 변경.
ZMQ 코드는 Proxy/Skeleton 구현 내부에만 존재.

### Phase 2 — DriverManager / ServiceLayer 분리

DriverManager에서 ZMQ 코드를 제거하고,
SensorService가 DriverManager의 큐에서 데이터를 꺼내 발행하도록 변경.

### Phase 3 — 설정 파일 기반 드라이버 로드

`hw_config.json`을 읽어 드라이버 인스턴스를 동적 생성.
device path, baud rate 등을 코드 변경 없이 설정.

### Phase 4 — 드라이버 스레드 epoll 전환

단일 pollLoop를 epoll 기반으로 교체.
센서가 늘어나도 레이턴시가 일정하게 유지됨.

### Phase 5 — ara::exec 스타일 생명주기

상태 머신(Init → Running → Shutdown)을 명시적으로 구현.
SIGTERM → Shutdown 상태 전이 → 모든 컴포넌트 순서대로 종료.
