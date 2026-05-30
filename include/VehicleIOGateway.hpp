#pragma once
#include "DeviceManager.hpp"
#include "SensorGateway.hpp"
#include "ActuatorGateway.hpp"
#include "services/SensorServiceSkeleton.hpp"
#include <zmq.hpp>
#include <memory>

class VehicleIOGateway {
public:
    explicit VehicleIOGateway(DeviceManager& dm);

    bool setup();
    void run();

private:
    DeviceManager&                         dm_;
    zmq::context_t                         zmq_ctx_{1};
    std::unique_ptr<SensorServiceSkeleton> sensor_skeleton_;
    std::unique_ptr<SensorGateway>         sensor_gw_;
    std::unique_ptr<ActuatorGateway>       actuator_gw_;
};
