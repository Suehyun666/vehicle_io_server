#pragma once
#include "services/sensor_types.hpp"
#include <zmq.hpp>
#include <string>

// ── SensorServiceSkeleton ────────────────────────────────────────
//  HAL 서비스(vehicle_io_server) 쪽 서버 SDK
//  - ZMQ PUB, JSON 직렬화가 이 클래스 안에 완전히 숨겨짐
//  - dispatch 로직 없음 — SensorGateway 의 handler map 이 분기를 담당
// ────────────────────────────────────────────────────────────────
class SensorServiceSkeleton {
public:
    SensorServiceSkeleton(zmq::context_t& ctx, const std::string& endpoint);

    bool bind();
    void publishStick(const StickData& d);
    void publishDist (const DistData&  d);

private:
    void publishRaw(const std::string& topic, const std::string& payload);

    zmq::socket_t publisher_;
    std::string   endpoint_;
};
