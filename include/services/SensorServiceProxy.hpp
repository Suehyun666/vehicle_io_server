#pragma once
#include "services/sensor_types.hpp"
#include <zmq.hpp>
#include <chrono>
#include <functional>
#include <string>
#include <unordered_map>

// ── SensorServiceProxy ───────────────────────────────────────────
//  피처 앱이 사용하는 클라이언트 SDK
//  - ZMQ SUB, JSON 역직렬화가 이 클래스 안에 완전히 숨겨짐
//  - subscribeXxx() 호출 시 내부 handler map 에 등록
//  - poll() 은 handler map 으로 generic dispatch — 센서가 늘어도 변경 없음
// ────────────────────────────────────────────────────────────────
class SensorServiceProxy {
public:
    SensorServiceProxy(zmq::context_t& ctx, const std::string& endpoint);

    void subscribeStick(std::function<void(const StickData&)> cb);
    void subscribeDist (std::function<void(const DistData&)>  cb);

    // 루프에서 주기적으로 호출 — 도착한 메시지를 handler map 으로 dispatch
    void poll(std::chrono::milliseconds timeout);

private:
    zmq::socket_t subscriber_;
    std::unordered_map<std::string, std::function<void(const std::string&)>> handlers_;
};
