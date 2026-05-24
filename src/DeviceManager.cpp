#include "DeviceManager.hpp"
#include <iostream>

bool DeviceManager::loadFromConfig(const std::string& config_path) {
    std::cout << "[DeviceManager] Loading config: " << config_path << "\n";
    // TODO: nlohmann/json 으로 파싱 → 드라이버 factory 호출
    //       각 "type" 문자열을 보고 적절한 구현체를 new 해서 register
    return true;
}

void DeviceManager::registerSensorDriver(std::shared_ptr<ISensorDriver> driver) {
    if (!driver->connect()) {
        std::cerr << "[DeviceManager] Sensor driver connect failed: "
                  << driver->getId() << "\n";
        return;
    }
    std::cout << "[DeviceManager] Sensor  registered: " << driver->getId() << "\n";
    sensor_drivers_.push_back(std::move(driver));
}

void DeviceManager::registerActuatorDriver(const std::string& target,
                                           std::shared_ptr<IActuatorDriver> driver) {
    if (!driver->connect()) {
        std::cerr << "[DeviceManager] Actuator driver connect failed: "
                  << driver->getId() << "\n";
        return;
    }
    std::cout << "[DeviceManager] Actuator registered: "
              << driver->getId() << " → target: " << target << "\n";
    actuator_drivers_[target] = std::move(driver);
}

std::vector<std::shared_ptr<ISensorDriver>>& DeviceManager::getSensorDrivers() {
    return sensor_drivers_;
}

std::shared_ptr<IActuatorDriver> DeviceManager::findActuatorDriver(const std::string& target) {
    auto it = actuator_drivers_.find(target);
    return (it != actuator_drivers_.end()) ? it->second : nullptr;
}
