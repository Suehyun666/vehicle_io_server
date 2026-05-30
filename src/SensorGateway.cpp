#include "SensorGateway.hpp"
#include "services/sensor_serialization.hpp"
#include "drivers/TcaSidestickDriver.hpp"
#include "drivers/ArduinoSerialDriver.hpp"
#include <iostream>
#include <chrono>

SensorGateway::SensorGateway(DeviceManager& dm, SensorServiceSkeleton& skeleton)
    : dm_(dm), skeleton_(skeleton)
{
    setupHandlers();
}

SensorGateway::~SensorGateway() { stop(); }

// ── topic → publish 함수 매핑 — 새 센서는 여기에만 추가 ──────────
void SensorGateway::setupHandlers() {
    handlers_[TcaSidestickDriver::kTopic] = [&](const std::string& p) {
        skeleton_.publishStick(sdv::serde::parseStick(p));
    };
    handlers_[ArduinoSerialDriver::kTopicDist] = [&](const std::string& p) {
        skeleton_.publishDist(sdv::serde::parseDist(p));
    };
}

void SensorGateway::start() {
    running_ = true;
    worker_  = std::thread(&SensorGateway::pollLoop, this);
    std::cout << "[SensorGateway] Poll thread started\n";
}

void SensorGateway::stop() {
    running_ = false;
    if (worker_.joinable()) worker_.join();
}

void SensorGateway::pollLoop() {
    const auto& drivers = dm_.getSensorDrivers();

    while (running_) {
        bool any = false;
        for (const auto& driver : drivers) {
            if (!driver->isDataAvailable()) continue;

            const auto data = driver->read();
            const auto it   = handlers_.find(data.topic);
            if (it != handlers_.end()) it->second(data.payload);
            any = true;
        }
        if (!any) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    std::cout << "[SensorGateway] Stopped\n";
}
