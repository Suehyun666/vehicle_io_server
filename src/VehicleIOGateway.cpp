#include "VehicleIOGateway.hpp"
#include <iostream>

VehicleIOGateway::VehicleIOGateway(DeviceManager& dm)
    : dm_(dm)
{}

bool VehicleIOGateway::setup() {
    // ── 자식 게이트웨이 생성 (공유 ZMQ 컨텍스트 전달) ────────────
    sensor_gw_   = std::make_unique<SensorGateway>  (dm_, zmq_ctx_, kSensorPubEndpoint);
    actuator_gw_ = std::make_unique<ActuatorGateway>(dm_, zmq_ctx_, kActuatorRepEndpoint);

    if (!sensor_gw_->initialize()) {
        std::cerr << "[VehicleIOGateway] SensorGateway init failed\n";
        return false;
    }
    if (!actuator_gw_->initialize()) {
        std::cerr << "[VehicleIOGateway] ActuatorGateway init failed\n";
        return false;
    }

    std::cout << "[VehicleIOGateway] Setup complete\n";
    return true;
}

void VehicleIOGateway::run() {
    sensor_gw_->start();         // 백그라운드 폴링 스레드 기동
    actuator_gw_->listenLoop();  // 메인 스레드 블로킹 — Ctrl+C 까지
}
