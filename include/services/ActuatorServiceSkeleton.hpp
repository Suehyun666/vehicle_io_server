#pragma once
#include "common.hpp"
#include <zmq.hpp>
#include <atomic>
#include <string>

// ── ActuatorServiceSkeleton ──────────────────────────────────────
//  HAL 서비스 쪽에서 상속해서 구현하는 추상 클래스
//  - ZMQ REP 소켓 수신, JSON 파싱, target 분기를 처리
//  - 파생 클래스(ActuatorGateway)는 typed 핸들러만 구현하면 됨
// ────────────────────────────────────────────────────────────────
class ActuatorServiceSkeleton {
public:
    ActuatorServiceSkeleton(zmq::context_t& ctx, const std::string& endpoint);
    virtual ~ActuatorServiceSkeleton() = default;

    bool bind();
    void listenLoop();  // 메인 스레드 블로킹
    void stop();

protected:
    // 파생 클래스가 구현 — 드라이버 호출 등 HAL 로직
    virtual ActuatorResult onSetServo(double angle) = 0;
    virtual ActuatorResult onSetRelay(bool   on)    = 0;

private:
    zmq::socket_t     responder_;
    std::string       endpoint_;
    std::atomic<bool> running_{false};
};
