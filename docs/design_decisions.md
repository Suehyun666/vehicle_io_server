# 설계 결정 및 기술 비교

## 1. 센서가 많아지면 현재 구조가 문제인가?

### 현재 구조의 한계

```
[SensorGateway 단일 스레드]

driver_A.isDataAvailable() → read() → publish()
driver_B.isDataAvailable() → read() → publish()
driver_C.isDataAvailable() → read() → publish()   ← 추가할수록 루프가 길어짐
...
sleep(1ms) if 아무 데이터 없을 때만
```

센서가 늘어날수록 생기는 문제:

| 문제 | 설명 |
|---|---|
| 레이턴시 증가 | driver_A 처리 시간만큼 driver_C 응답이 늦어짐 |
| 블로킹 I/O 전파 | 한 드라이버가 잠깐이라도 블로킹하면 전체 루프 지연 |
| CPU busy-loop | 항상 데이터가 있는 드라이버가 있으면 sleep(1ms) 안 들어감 |
| 비균등 폴링 주기 | 드라이버 수에 따라 각 드라이버의 실질 폴링 주기가 달라짐 |

현재 TCA와 Arduino 둘 정도는 문제없습니다. 카메라, GPS, CAN 버스, IMU 등이 추가되면 구조를 바꿔야 합니다.

---

### 개선 방안

#### 방안 A — 드라이버별 전용 스레드

```
[main]
  ├── SensorThread(TcaSidestickDriver) → publish()
  ├── SensorThread(ArduinoSerialDriver) → publish()
  ├── SensorThread(CameraDriver) → publish()      ← 블로킹 OK
  └── ActuatorGateway (main thread)
```

- 장점: 블로킹 I/O 드라이버가 다른 드라이버에 영향 없음, 각 드라이버 독립 동작
- 단점: ZMQ PUB 소켓은 스레드 안전하지 않음 → publish 호출에 mutex 필요
- 드라이버 10개면 스레드 10개. 대부분 I/O 대기 중이라 CPU는 괜찮지만 context switch 비용 존재

```cpp
// publish 호출 시 뮤텍스 필요
std::mutex pub_mutex_;
void publishSafe(const SensorData& d) {
    std::lock_guard<std::mutex> lk(pub_mutex_);
    publish(d);
}
```

#### 방안 B — epoll + 스레드 풀 (권장)

```
[epoll 스레드] — 모든 fd를 감시
    fd 읽기 가능 이벤트 발생
         ↓
    [스레드 풀] — 해당 드라이버의 read() + publish() 처리
```

- OS가 어떤 fd가 준비됐는지 알려줌 → sleep/busy-loop 없음
- 드라이버가 100개여도 이벤트가 있는 것만 처리
- 단, 드라이버가 fd 기반이어야 함 (UDP, 시리얼, HID 등은 가능, 일부 SDK는 불가)

#### 방안 C — 현재 구조 유지 + 계약 강제

드라이버가 적고 모두 non-blocking이라면 현재 구조를 유지하되 인터페이스에 명시:

```cpp
// ISensorDriver 계약: isDataAvailable()과 read()는 절대 블로킹하지 않는다
// 블로킹 I/O가 필요한 드라이버는 별도 내부 스레드를 가져야 한다
class ISensorDriver : public virtual IDriver {
public:
    // MUST be non-blocking. Max duration: ~100µs
    virtual bool       isDataAvailable() = 0;
    virtual SensorData read()            = 0;
};
```

이 경우 카메라 같은 드라이버는 내부에 스레드를 두고 큐로 연결하는 방식으로 구현.

---

## 2. ZMQ vs MQTT (Mosquitto)

### 개요

| 항목 | ZMQ | MQTT (Mosquitto) |
|---|---|---|
| 브로커 | 없음 (P2P, 직접 연결) | 있음 (Mosquitto 브로커 필수) |
| 레이턴시 | 매우 낮음 (IPC는 수 µs) | TCP + 브로커 경유 → ms 단위 |
| QoS | 없음 (fire-and-forget) | 0 / 1 / 2 (at-most/at-least/exactly-once) |
| 메시지 보존 | 없음 | retain 메시지 (마지막 값 캐시) |
| 재연결 복구 | 없음 | 세션 유지 → 오프라인 중 메시지 수신 가능 |
| 토픽 와일드카드 | 없음 (prefix 매칭만) | `sensor/+/dist`, `sensor/#` |
| 바이너리 페이로드 | 자유 | 자유 |
| 표준화 | 없음 (라이브러리) | OASIS 표준 (IoT 생태계) |
| 복잡도 | 낮음 | 브로커 관리 필요 |

### 언제 MQTT가 유리한가

- 피처 앱이 다른 머신이나 컨테이너에서 실행되는 경우
- 피처가 죽었다 살아날 때 놓친 메시지를 다시 받아야 하는 경우
- 새 구독자가 연결 즉시 마지막 센서값을 받아야 하는 경우 (retain)
- 외부 대시보드, 원격 모니터링과 연동하는 경우

### 언제 ZMQ가 유리한가

- 모든 컴포넌트가 같은 호스트에서 실행되는 경우 (IPC → 수 µs)
- 레이턴시가 중요한 실시간 제어인 경우
- 브로커 단일 장애점을 피하고 싶은 경우
- 단순한 구조를 원하는 경우

### 이 프로젝트에 Mosquitto를 쓴다면

```
ZMQ PUB/SUB (현재)          MQTT/Mosquitto (대안)

vehicle_io_server              vehicle_io_server
  └─ SensorGateway               └─ SensorGateway
       PUB bind                       publish("sensor/tca/stick", payload)
                                           │
                               [Mosquitto broker]
                                           │
feature_A: SUB connect        feature_A: subscribe("sensor/tca/+")
feature_B: SUB connect        feature_B: subscribe("sensor/#")
```

구조 복잡도는 올라가지만 `sensor/+/dist` 같은 와일드카드 구독이 가능해집니다.
같은 호스트에서만 쓴다면 현재 ZMQ IPC가 더 단순하고 빠릅니다.

---

## 3. CAN / AUTOSAR / ZCU — 어느 것이 전통 차량이고 어느 것이 SDV인가?

### CAN (Controller Area Network)

전통 차량과 현대 차량 모두에서 사용합니다. 버스 프로토콜(물리/링크 레이어)입니다.

```
[전통 차량 구조]

  Engine ECU ─┐
  ABS ECU    ─┤─── CAN bus ───── BCM (Body Control Module)
  TCU        ─┘                └── Cluster (계기판)

특징: ECU가 100개 이상, 각자 독립 MCU, CAN으로 연결
```

CAN의 특성:
- 최대 1Mbps (CAN FD는 8Mbps)
- 멀티마스터: 누구나 보낼 수 있음, 충돌 시 우선순위로 해결
- 메시지 ID + 데이터 8바이트
- 결정론적, 노이즈에 강함 (차동 신호)
- 모든 노드가 모든 메시지를 봄 → 필터링으로 관심 있는 것만 처리

### AUTOSAR (AUTomotive Open System ARchitecture)

**소프트웨어 아키텍처 표준**입니다. 버스 프로토콜이 아닙니다.

```
Classic AUTOSAR (전통 + SDV ECU 내부)       Adaptive AUTOSAR (SDV 고성능 컴퓨트)

┌─────────────────────────┐               ┌──────────────────────────┐
│  Application Layer       │               │  Adaptive Application    │
│  (SWC, Runnable)        │               │  (실행 파일, 서비스)      │
├─────────────────────────┤               ├──────────────────────────┤
│  RTE (Runtime Env)      │               │  ARA (AUTOSAR Runtime)   │
├─────────────────────────┤               ├──────────────────────────┤
│  BSW (Basic Software)   │               │  OS (Linux/QNX 등)       │
│  (CAN 드라이버, OS 등)   │               │                          │
├─────────────────────────┤               ├──────────────────────────┤
│  MCU (Renesas, ST...)   │               │  SoC (Qualcomm, NXP...)  │
└─────────────────────────┘               └──────────────────────────┘

  → 전통 차량 ECU에 사용                    → SDV Central Computer에 사용
    + SDV의 Zone ECU에도 사용                SOME-IP / DDS로 서비스 통신
```

**정리:**
- CAN → 전통/SDV 모두, ECU 간 버스
- Classic AUTOSAR → 전통/SDV 모두, ECU 내부 소프트웨어 표준
- Adaptive AUTOSAR → SDV 특화, 고성능 중앙 컴퓨터용

### 이 프로젝트의 Arduino ↔ PC 통신은 어떤 게 맞나?

현재: UART (USB CDC) + JSON

| 방식 | 특성 | 이 프로젝트 적합성 |
|---|---|---|
| UART/JSON (현재) | 단순, 낮은 속도, 텍스트 파싱 오버헤드 | 프로토타입에 적합 |
| CAN (MCP2515 쉴드) | 결정론적, 노이즈 강함, 자동차 표준 | ZCU 역할 강화 시 권장 |
| LIN | 단순 마스터-슬레이브, 저비용 | 단순 액추에이터 제어용 |
| CAN FD | 고속, 최대 64바이트 페이로드 | 센서가 많아지면 고려 |

프로토타입에서 실제 ZCU에 가까운 구조로 가려면 CAN을 권장합니다.
Arduino + MCP2515 모듈로 CAN에 연결하고, PC 쪽은 SocketCAN(`/dev/can0`)으로 읽으면 됩니다.

---

## 4. 폴링 vs 인터럽트 — 신뢰성 문제?

### Arduino 내부 (현재)

`vehicle_node.ino`에서 `pulseIn()`은 **블로킹 함수**입니다. 초음파 echo를 기다리는 동안(최대 25ms) Arduino는 다른 일을 못 합니다.

```cpp
// 현재 코드 — pulseIn이 블로킹
long us = pulseIn(PIN_ECHO, HIGH, TIMEOUT_US);  // 최대 25ms 블로킹
```

이 25ms 동안 PC에서 서보 명령이 와도 Serial 버퍼에서 기다립니다. Serial 버퍼(64바이트)가 가득 차면 명령이 유실됩니다.

인터럽트 기반으로 바꾸면:

```cpp
// 인터럽트 + Timer 방식
volatile unsigned long echo_start = 0;
volatile unsigned long echo_end   = 0;

void echoRise() { echo_start = micros(); }
void echoFall() { echo_end   = micros(); }

// setup에서
attachInterrupt(digitalPinToInterrupt(PIN_ECHO), echoRise, RISING);
attachInterrupt(digitalPinToInterrupt(PIN_ECHO), echoFall, FALLING);

// loop는 블로킹 없이 돌아감
// → Serial 명령 수신이 지연되지 않음
```

### PC 쪽 폴링 (현재)

PC의 `pollLoop`는 실시간 제약이 없으므로 폴링으로 충분합니다. OS 스케줄러가 100µs~수ms 레이턴시를 허용하는 환경이면 문제없습니다.

### 신뢰성 정리

| 계층 | 현재 방식 | 문제점 | 개선 |
|---|---|---|---|
| Arduino 센서 읽기 | pulseIn() 블로킹 | 명령 응답 최대 25ms 지연 | 인터럽트 + 타이머 |
| Arduino 명령 수신 | loop() 폴링 | pulseIn 중 명령 유실 가능 | 위와 동일 |
| PC 센서 폴링 | 단일 스레드 순회 | 드라이버 추가 시 레이턴시 증가 | epoll 또는 per-driver 스레드 |
| PC 액추에이터 | zmq::poll 10ms timeout | 10ms 응답 레이턴시 | timeout 줄이거나 별도 스레드 |

---

## 5. 드라이버별 스레드를 만들어야 하나?

드라이버 성격에 따라 다릅니다.

### Non-blocking 드라이버 (현재 TCA, Arduino)

내부 스레드 불필요. 현재 단일 pollLoop로 충분합니다.

### Blocking 드라이버 (카메라, GPS 등)

전용 스레드가 필요합니다. 공유 publisher 소켓을 mutex로 보호하거나,
드라이버 내부에서 스레드를 숨기고 ISensorDriver 인터페이스만 non-blocking으로 노출합니다.

```cpp
// 내부에 스레드를 숨기는 패턴
class CameraDriver : public ISensorDriver {
    std::thread         capture_thread_;
    std::queue<SensorData> queue_;
    std::mutex          mutex_;

    void captureLoop() {
        while (running_) {
            auto frame = camera_.read();  // 블로킹이지만 내부 스레드에서만
            std::lock_guard lk(mutex_);
            queue_.push(buildSensorData(frame));
        }
    }

public:
    bool isDataAvailable() override {
        std::lock_guard lk(mutex_);
        return !queue_.empty();          // pollLoop에서 호출 시 non-blocking
    }
    SensorData read() override {
        std::lock_guard lk(mutex_);
        auto d = queue_.front(); queue_.pop();
        return d;
    }
};
```

이 패턴을 쓰면 pollLoop의 "모든 드라이버는 non-blocking" 계약을 유지하면서 블로킹 드라이버도 수용할 수 있습니다.

---

## 6. Arduino 통신 — readline/parseline 외 대안은?

### 현재 방식: Newline-delimited JSON

```
Arduino → PC: {"dist":45.2}\n
PC → Arduino: {"target":"servo1","value":90}\n
```

장점: 디버깅 쉬움, 구현 단순  
단점: 파싱 오버헤드, `\n` 유실 시 프레임 깨짐, 텍스트 비효율

### 대안 1: COBS (Consistent Overhead Byte Stuffing)

```
[len][COBS-encoded payload][0x00]
```

- 0x00을 프레임 구분자로 사용
- 데이터에서 0x00을 없애는 인코딩
- 실제 임베디드에서 많이 씀
- 프레임 경계 명확, 깨진 패킷 복구 쉬움

### 대안 2: SLIP (Serial Line IP)

- 0xC0을 프레임 구분자로 사용
- 구현 극히 단순

### 대안 3: 길이 접두사 바이너리

```
[payload_len : 2 bytes][payload : N bytes][checksum : 1 byte]
```

- 길이를 먼저 읽고 정확히 N바이트 읽음
- 가장 효율적이지만 양쪽 구현 필요

### 대안 4: CAN (실제 ZCU로 발전 시)

```
Arduino + MCP2515         PC
  CAN 노드            SocketCAN (/dev/can0)
     │                      │
     └──── CAN bus ──────────┘
           11-bit ID + 8bytes
```

CAN으로 가면 readline/parseline 자체가 필요 없어지고 CAN 메시지 ID + 8바이트 데이터로 통신합니다.

### 이 프로젝트에서 readline/parseline을 쓰는 게 잘못된 건 아닙니다

UART + JSON은 프로토타입과 교육용으로 표준적입니다. 실제 자동차 양산 코드에서는 안 쓰지만, SDV 연구/개발 단계에서는 충분합니다. 다만 `line_buf_`의 최대 길이 제한은 반드시 추가해야 합니다.
