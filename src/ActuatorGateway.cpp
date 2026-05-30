#include "ActuatorGateway.hpp"
#include <iostream>

ActuatorGateway::ActuatorGateway(DeviceManager&     dm,
                                  zmq::context_t&    zmq_ctx,
                                  const std::string& endpoint)
    : ActuatorServiceSkeleton(zmq_ctx, endpoint)
    , dm_(dm)
{}

ActuatorResult ActuatorGateway::onSetServo(double angle) {
    auto driver = dm_.findActuatorDriver("actuator/servo1");
    if (!driver) return {false, "No driver for actuator/servo1"};
    std::cout << "[ActuatorGateway] servo → " << angle << "deg\n";
    return driver->write({"actuator/servo1", angle, "gateway"});
}

ActuatorResult ActuatorGateway::onSetRelay(bool on) {
    auto driver = dm_.findActuatorDriver("actuator/relay");
    if (!driver) return {false, "No driver for actuator/relay"};
    std::cout << "[ActuatorGateway] relay → " << (on ? "ON" : "OFF") << "\n";
    return driver->write({"actuator/relay", on ? 1.0 : 0.0, "gateway"});
}
