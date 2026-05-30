#pragma once
#include "common.hpp"
#include <zmq.hpp>
#include <string>

// ── ActuatorServiceProxy ─────────────────────────────────────────
//  피처 앱이 사용하는 클라이언트 SDK
//  - ZMQ REQ 소켓, JSON 직렬화/역직렬화가 이 클래스 안에 숨겨짐
//  - 피처는 typed 메서드 호출만 함
// ────────────────────────────────────────────────────────────────
class ActuatorServiceProxy {
public:
    explicit ActuatorServiceProxy(zmq::context_t& ctx,
                                  const std::string& endpoint,
                                  const std::string& feature_id = "unknown");

    ActuatorResult setSteering(float normalized);  // [-1.0 좌 ~ +1.0 우]
    ActuatorResult setEmergencyBrake(bool on);

private:
    ActuatorResult sendCommand(const std::string& target, double value);

    zmq::socket_t requester_;
    std::string   feature_id_;
};
