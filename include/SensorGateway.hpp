#pragma once
#include "DeviceManager.hpp"
#include "services/SensorServiceSkeleton.hpp"
#include <atomic>
#include <functional>
#include <string>
#include <thread>
#include <unordered_map>

// ── SensorGateway ────────────────────────────────────────────────
//  드라이버 폴링 스레드와 SensorServiceSkeleton 사이의 브리지
//  - topic → publish_fn 맵을 한 곳에서만 관리
//  - 새 센서 추가 = setupHandlers() 에 한 줄
// ────────────────────────────────────────────────────────────────
class SensorGateway {
public:
    SensorGateway(DeviceManager& dm, SensorServiceSkeleton& skeleton);
    ~SensorGateway();

    void start();
    void stop();

private:
    void setupHandlers();
    void pollLoop();

    using Handler = std::function<void(const std::string& payload)>;

    DeviceManager&         dm_;
    SensorServiceSkeleton& skeleton_;
    std::unordered_map<std::string, Handler> handlers_;
    std::thread            worker_;
    std::atomic<bool>      running_{false};
};
