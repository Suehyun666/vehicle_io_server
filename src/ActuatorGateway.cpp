#include "ActuatorGateway.hpp"
#include <algorithm>
#include <iostream>

ActuatorGateway::ActuatorGateway(DeviceManager&     dm,
                                  zmq::context_t&    zmq_ctx,
                                  const std::string& endpoint)
    : ActuatorServiceSkeleton(zmq_ctx, endpoint)
    , dm_(dm)
{}

// normalized [-1, 1] → servo angle [0, 180]
ActuatorResult ActuatorGateway::onSetSteering(float normalized) {
    auto driver = dm_.findActuatorDriver("actuator/servo1");
    if (!driver) return {false, "No driver for steering"};

    const float clamped = std::clamp(normalized, -1.0f, 1.0f);
    const double angle  = (clamped + 1.0f) / 2.0f * 180.0f;

    std::cout << "[ActuatorGateway] steering=" << normalized << " → servo=" << angle << "deg\n";
    return driver->write({"actuator/servo1", angle, "gateway"});
}

ActuatorResult ActuatorGateway::onSetEmergencyBrake(bool on) {
    auto driver = dm_.findActuatorDriver("actuator/relay");
    if (!driver) return {false, "No driver for emergency brake"};

    std::cout << "[ActuatorGateway] emergencyBrake=" << (on ? "ON" : "OFF") << "\n";
    return driver->write({"actuator/relay", on ? 1.0 : 0.0, "gateway"});
}
