#pragma once
#include "DeviceManager.hpp"
#include <zmq.hpp>
#include <atomic>
#include <string>

// ────────────────────────────────────────────────────────────────
//  ActuatorGateway
//  - ZMQ REP 소켓으로 피처 앱의 제어 명령을 수신
//  - DeviceManager에서 target에 맞는 드라이버를 찾아 위임
//  - 실행 결과를 JSON으로 응답
//
//  수신 JSON 형식:
//    {"target": "actuator/steering", "value": 15.0, "feature_id": "lka"}
//  응답 JSON 형식:
//    {"success": true, "message": "OK"}
// ────────────────────────────────────────────────────────────────
class ActuatorGateway {
public:
    ActuatorGateway(DeviceManager&     dm,
                    zmq::context_t&    zmq_ctx,
                    const std::string& rep_endpoint);
    ~ActuatorGateway();

    bool initialize();   // REP 소켓 bind
    void listenLoop();   // 메인 스레드 블로킹 루프
    void stop();

private:
    ActuatorResult handleCommand(const ActuatorCommand& cmd);

    DeviceManager&    dm_;
    zmq::context_t&   zmq_ctx_;
    std::string       rep_endpoint_;
    zmq::socket_t     responder_;
    std::atomic<bool> running_{false};
};
