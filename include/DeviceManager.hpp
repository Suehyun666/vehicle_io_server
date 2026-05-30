#pragma once
#include "IDriver.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// ────────────────────────────────────────────────────────────────
//  DeviceManager
//  - 드라이버 인스턴스를 생성·보관하는 레지스트리
//  - VehicleIoServer::main() 이 소유하고, VehicleIOGateway에 참조로 전달
// ────────────────────────────────────────────────────────────────
class DeviceManager {
public:
    DeviceManager()  = default;
    ~DeviceManager() = default;

    // ── 설정 파일 기반 일괄 로드 ─────────────────────────────────
    // hw_config.json을 읽어 드라이버를 자동 인스턴스화한다
    // (TODO: 드라이버 factory 구현 후 활성화)
    bool loadFromConfig(const std::string& config_path);

    // ── 개발/테스트용 수동 등록 ───────────────────────────────────
    void registerSensorDriver(std::shared_ptr<ISensorDriver> driver);

    // target 은 ActuatorCommand::target 과 1:1 매핑
    // e.g. "actuator/steering", "actuator/relay"
    void registerActuatorDriver(const std::string& target,
                                std::shared_ptr<IActuatorDriver> driver);

    // ── 조회 ──────────────────────────────────────────────────────
    const std::vector<std::shared_ptr<ISensorDriver>>& getSensorDrivers() const;
    std::shared_ptr<IActuatorDriver> findActuatorDriver(const std::string& target);

private:
    std::vector<std::shared_ptr<ISensorDriver>>          sensor_drivers_;
    std::unordered_map<std::string,
                       std::shared_ptr<IActuatorDriver>> actuator_drivers_;
};
