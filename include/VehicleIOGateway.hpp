#pragma once
#include "DeviceManager.hpp"
#include "SensorGateway.hpp"
#include "ActuatorGateway.hpp"
#include <zmq.hpp>
#include <memory>

// ────────────────────────────────────────────────────────────────
//  VehicleIOGateway  (HAL 내부 오케스트레이터)
//  - main() 에서 DeviceManager 참조를 주입받아 생성
//  - SensorGateway, ActuatorGateway 를 소유·조립
//  - ZMQ 컨텍스트를 하나만 유지하고 자식들에게 공유
// ────────────────────────────────────────────────────────────────
class VehicleIOGateway {
public:
    explicit VehicleIOGateway(DeviceManager& dm);

    bool setup();  // 자식 게이트웨이 초기화
    void run();    // SensorGW 스레드 기동 → ActuatorGW 블로킹 루프

private:
    // ipc:// → 같은 호스트 전용, tcp:// 로 바꾸면 원격 피처도 연결 가능
    static constexpr const char* kSensorPubEndpoint   = "ipc:///tmp/sdv_sensor.ipc";
    static constexpr const char* kActuatorRepEndpoint = "ipc:///tmp/sdv_actuator.ipc";

    DeviceManager&                    dm_;
    zmq::context_t                    zmq_ctx_{1};  // I/O 스레드 1개
    std::unique_ptr<SensorGateway>    sensor_gw_;
    std::unique_ptr<ActuatorGateway>  actuator_gw_;
};
