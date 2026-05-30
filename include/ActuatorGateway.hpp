#pragma once
#include "DeviceManager.hpp"
#include "services/ActuatorServiceSkeleton.hpp"

// ── ActuatorGateway ──────────────────────────────────────────────
//  ActuatorServiceSkeleton 의 HAL 구현체
//  - onSetServo / onSetRelay 만 구현하면 됨
//  - ZMQ, JSON, REQ/REP 프로토콜은 Skeleton 이 처리
// ────────────────────────────────────────────────────────────────
class ActuatorGateway : public ActuatorServiceSkeleton {
public:
    ActuatorGateway(DeviceManager&     dm,
                    zmq::context_t&    zmq_ctx,
                    const std::string& endpoint);

protected:
    ActuatorResult onSetSteering(float normalized) override;
    ActuatorResult onSetEmergencyBrake(bool on)    override;

private:
    DeviceManager& dm_;
};
