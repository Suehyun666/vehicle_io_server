#include "hal/HalNode.hpp"
#include "signals/names.hpp"
#include "endpoints.hpp"
#include "drivers/TcaSidestickDriver.hpp"
#include "drivers/ArduinoSerialDriver.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <functional>
#include <iostream>
#include <unordered_map>

using json = nlohmann::json;
using namespace std::chrono_literals;

HalNode::HalNode(DeviceManager& dm, zmq::context_t& ctx)
    : dm_(dm)
    , sensor_pub_(ctx, endpoints::kHalPub)
    , cmd_sub_   (ctx, endpoints::kFeatPub)
{
    // Arbiter 없이 request를 명령으로 직접 처리 — Arbiter 추가 시 여기 교체
    cmd_sub_.onFloat(signals::kReqSteering, [&](float v) {
        auto driver = dm_.findActuatorDriver("actuator/servo1");
        if (!driver) return;
        const double angle = (std::clamp(v, -1.0f, 1.0f) + 1.0f) / 2.0f * 180.0;
        std::cout << "[HAL] steering=" << v << " → " << angle << "deg\n";
        driver->write({"actuator/servo1", angle, "hal"});
    });

    cmd_sub_.onBool(signals::kReqEmergency, [&](bool on) {
        auto driver = dm_.findActuatorDriver("actuator/relay");
        if (!driver) return;
        std::cout << "[HAL] emergency_brake=" << (on ? "ON" : "OFF") << "\n";
        driver->write({"actuator/relay", on ? 1.0 : 0.0, "hal"});
    });
}

bool HalNode::start() {
    running_ = true;
    sensor_thread_ = std::thread(&HalNode::sensorLoop, this);
    std::cout << "[HAL] Started — pub=" << endpoints::kHalPub
              << " sub=" << endpoints::kFeatPub << "\n";
    return true;
}

void HalNode::stop() {
    running_ = false;
    if (sensor_thread_.joinable()) sensor_thread_.join();
    std::cout << "[HAL] Stopped\n";
}

void HalNode::sensorLoop() {
    using Handler = std::function<void(const SensorData&)>;
    std::unordered_map<std::string, Handler> handlers;

    handlers[TcaSidestickDriver::kTopic] = [&](const SensorData& d) {
        try {
            auto j = json::parse(d.payload);
            sensor_pub_.publish(signals::kTwist,    j.value("twist",        0.0f));
            sensor_pub_.publish(signals::kThrottle,  j.value("throttle_pct", 0.0f));
        } catch (...) {}
    };

    handlers[ArduinoSerialDriver::kTopicDist] = [&](const SensorData& d) {
        try {
            auto j = json::parse(d.payload);
            sensor_pub_.publish(signals::kFrontDistance, j.value("distance_cm", 0.0f));
        } catch (...) {}
    };

    const auto& drivers = dm_.getSensorDrivers();

    while (running_) {
        bool any = false;
        for (const auto& driver : drivers) {
            if (!driver->isDataAvailable()) continue;
            const auto data = driver->read();
            const auto it   = handlers.find(data.topic);
            if (it != handlers.end()) it->second(data);
            any = true;
        }
        cmd_sub_.poll(0ms);  // non-blocking — 명령 즉시 처리
        if (!any) std::this_thread::sleep_for(1ms);
    }
}
