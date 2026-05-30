#pragma once
#include "DeviceManager.hpp"
#include "bus/Publisher.hpp"
#include "bus/Subscriber.hpp"
#include <atomic>
#include <thread>

// ── HalNode ──────────────────────────────────────────────────────
//  드라이버 ↔ Signal Bus 사이의 번역 참여자
//
//  발행: /sensor/* (실측값, 드라이버에서 읽음)
//  구독: /request/* (현재는 Arbiter 없이 요청을 명령으로 직접 사용)
// ────────────────────────────────────────────────────────────────
class HalNode {
public:
    HalNode(DeviceManager& dm, zmq::context_t& ctx);

    bool start();
    void stop();

private:
    void sensorLoop();

    DeviceManager& dm_;
    Publisher      sensor_pub_;  // HAL → bus
    Subscriber     cmd_sub_;     // bus → HAL

    std::thread       sensor_thread_;
    std::atomic<bool> running_{false};
};
