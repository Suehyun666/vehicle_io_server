# Signal Store 기반 아키텍처 설계

## 현재 구조의 한계

현재 피처는 `ActuatorServiceProxy`를 직접 들고 있습니다.

```
Feature A → ActuatorServiceProxy.setSteering(0.5) → ZMQ → Gateway → 하드웨어
Feature B → ActuatorServiceProxy.setSteering(0.0) → ZMQ → Gateway → 하드웨어
```

피처가 Gateway를 직접 호출하는 구조라 두 가지 문제가 있습니다.

첫째, 피처가 actuator 서비스의 존재를 알아야 합니다.
둘째, 여러 피처가 동시에 명령을 보내면 마지막이 이기는데 이 시점을 제어할 수 없습니다.

---

## Signal Store 개념

차량의 모든 상태를 **이름 붙인 값(Signal)의 테이블**로 관리합니다.

```
Signal Store

"Vehicle.Sensors.FrontDistance"     → 45.2    (HAL이 씀)
"Vehicle.Chassis.SteeringAngle"     → 5.2     (HAL이 씀, 현재 실제값)
"Vehicle.Speed"                     → 0.0     (HAL이 씀)

"Vehicle.ADAS.TargetSteering"       → 0.3     (피처가 씀, 원하는 조향)
"Vehicle.ADAS.TargetSpeed"          → 0.5     (피처가 씀, 원하는 속도)
"Vehicle.Safety.EmergencyBrake"     → false   (피처가 씀)
```

피처는 Signal Store에 값을 쓸 뿐입니다. Gateway는 Signal Store에서 읽을 뿐입니다.
둘은 서로의 존재를 모릅니다.

---

## 목표 구조

```
[HAL - SensorGateway]
  HC-SR04 읽기 → signal.write("Vehicle.Sensors.FrontDistance", 45.2)
  TCA 읽기     → signal.write("Vehicle.Input.Twist", 0.3)

[Feature - ObstacleStop]
  signal.subscribe("Vehicle.Sensors.FrontDistance", [](float dist) {
      if (!flags.getBool("obstacle_stop.enabled")) return;
      if (dist < 20.0f)
          signal.write("Vehicle.Safety.EmergencyBrake", true);
  });

[Feature - ManualControl]
  signal.subscribe("Vehicle.Input.Twist", [](float twist) {
      if (!flags.getBool("manual.enabled")) return;
      signal.write("Vehicle.ADAS.TargetSteering", twist);
  });

[HAL - ActuatorGateway]
  signal.subscribe("Vehicle.ADAS.TargetSteering", [](float v) {
      driver.setSteering(v);
  });
  signal.subscribe("Vehicle.Safety.EmergencyBrake", [](bool on) {
      driver.setEmergencyBrake(on);
  });
```

피처와 Gateway 사이에 메서드 호출이 없습니다. Signal Store만 있습니다.

---

## Arbiter가 자연스러워지는 이유

Signal Store에 우선순위 기반 쓰기 규칙을 추가하면 됩니다.

```
signal.write("Vehicle.ADAS.TargetSteering", 0.3, priority=10)   // ManualControl
signal.write("Vehicle.ADAS.TargetSteering", 0.0, priority=100)  // ObstacleStop

Signal Store: "Vehicle.ADAS.TargetSteering" = 0.0  (priority 100이 이김)
```

별도의 Arbiter 클래스 없이 Signal Store의 write 정책으로 해결됩니다.

---

## Shadow Mode가 자연스러워지는 이유

Shadow 피처는 별도의 Shadow Signal Store에 씁니다. 코드 변경 없습니다.

```
Active Feature  → Real Signal Store    → HAL → 하드웨어
Shadow Feature  → Shadow Signal Store  → 로그만 (하드웨어 미전달)
```

피처 코드는 동일합니다. 플랫폼이 어느 Store를 주입하는지만 다릅니다.

---

## Observability

언제든 Signal Store 전체를 읽으면 차량 상태를 볼 수 있습니다.

```bash
# 디버그 도구
signal_monitor --watch "Vehicle.ADAS.*"

# 출력
Vehicle.ADAS.TargetSteering  = 0.30  (writer: manual_control, priority: 10)
Vehicle.ADAS.TargetSpeed     = 0.50  (writer: manual_control, priority: 10)
```

---

## VSS (Vehicle Signal Specification)

COVESA(구 GENIVI)가 정의한 차량 신호 이름 표준입니다.
신호를 트리 구조로 정의하고 단위, 타입, 허용 범위를 명시합니다.

```yaml
# VSS 정의 예시
Vehicle.ADAS.TargetSteering:
  type: float
  unit: normalized   # [-1.0, 1.0]
  description: Desired steering angle normalized

Vehicle.Sensors.FrontDistance:
  type: float
  unit: cm
  min: 0
  max: 400
```

이 프로젝트에서 VSS를 전부 따를 필요는 없지만 **이름 규칙**은 참고합니다.
`"Vehicle.Domain.SignalName"` 형식으로 계층을 만들면 나중에 표준과 맞추기 쉽습니다.

---

## 마이그레이션 계획

현재 구조에서 Signal Store로 전환하는 순서입니다.

### 1단계 — Signal Store 구현

```
include/platform/
  ISignalStore.hpp       — interface: read / write / subscribe
  InMemorySignalStore.hpp — in-memory 구현체
```

### 2단계 — FeatureContext에 Signal Store 추가

```cpp
struct FeatureContext {
    ISignalStore&    signals;   // 추가
    IFlagProvider&   flags;
    // ActuatorServiceProxy 제거
    // SensorServiceProxy 제거 (signals로 대체)
};
```

### 3단계 — HAL이 Signal Store를 읽고 씀

SensorGateway: `SensorData` → `signals.write("Vehicle.Sensors.FrontDistance", dist)`
ActuatorGateway: `signals.subscribe("Vehicle.ADAS.TargetSteering", ...)` → 드라이버 호출

### 4단계 — 피처 코드 전환

```cpp
// 현재
ctx().sensor.subscribeDist([&](const DistData& d) {
    actuator.setEmergencyBrake(d.distance_cm < 20.0f);
});

// Signal Store 방식
ctx().signals.subscribe("Vehicle.Sensors.FrontDistance", [&](float dist) {
    if (!ctx().flags.getBool("obstacle_stop.enabled")) return;
    ctx().signals.write("Vehicle.Safety.EmergencyBrake", dist < 20.0f, priority_);
});
```

---

## 현재 유지되는 것

| 항목 | 유지 여부 | 이유 |
|---|---|---|
| ISensorDriver / IActuatorDriver | 유지 | 드라이버 인터페이스는 HAL 내부 |
| SensorServiceSkeleton | 유지 (내부에서만 사용) | HAL→Signal Store 브리지로 전환 |
| ActuatorServiceSkeleton | 유지 (내부에서만 사용) | Signal Store→HAL 브리지로 전환 |
| sdv_topics / sdv_endpoints | 유지 (내부에서만 사용) | 피처에는 노출 안 됨 |
| Proxy/Skeleton 패턴 | HAL 내부에 유지 | 피처 레벨에서는 제거 |
