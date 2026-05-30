#include "VehicleIOGateway.hpp"
#include "sdv_endpoints.hpp"
#include <iostream>

VehicleIOGateway::VehicleIOGateway(DeviceManager& dm) : dm_(dm) {}

bool VehicleIOGateway::setup() {
    sensor_skeleton_ = std::make_unique<SensorServiceSkeleton>(
        zmq_ctx_, sdv::kSensorEndpoint);

    if (!sensor_skeleton_->bind()) {
        std::cerr << "[VehicleIOGateway] SensorSkeleton bind failed\n";
        return false;
    }

    sensor_gw_   = std::make_unique<SensorGateway>(dm_, *sensor_skeleton_);
    actuator_gw_ = std::make_unique<ActuatorGateway>(
        dm_, zmq_ctx_, sdv::kActuatorEndpoint);

    if (!actuator_gw_->bind()) {
        std::cerr << "[VehicleIOGateway] ActuatorSkeleton bind failed\n";
        return false;
    }

    std::cout << "[VehicleIOGateway] Setup complete\n";
    return true;
}

void VehicleIOGateway::run() {
    sensor_gw_->start();
    actuator_gw_->listenLoop();
    sensor_gw_->stop();  // listenLoop 반환 후 명시적 종료
}
