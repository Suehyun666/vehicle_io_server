#include "services/ActuatorServiceProxy.hpp"
#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;

ActuatorServiceProxy::ActuatorServiceProxy(zmq::context_t& ctx,
                                           const std::string& endpoint,
                                           const std::string& feature_id)
    : requester_(ctx, zmq::socket_type::req)
    , feature_id_(feature_id)
{
    requester_.connect(endpoint);
}

ActuatorResult ActuatorServiceProxy::setSteering(float normalized) {
    return sendCommand("vehicle/steering", static_cast<double>(normalized));
}

ActuatorResult ActuatorServiceProxy::setEmergencyBrake(bool on) {
    return sendCommand("vehicle/emergencyBrake", on ? 1.0 : 0.0);
}

ActuatorResult ActuatorServiceProxy::sendCommand(const std::string& target,
                                                  double value) {
    const auto s = json{{"target", target}, {"value", value},
                        {"feature_id", feature_id_}}.dump();
    try {
        requester_.send(zmq::message_t(s.data(), s.size()), zmq::send_flags::none);

        zmq::message_t rep;
        if (!requester_.recv(rep, zmq::recv_flags::none).has_value())
            return {false, "No reply"};

        const auto j = json::parse(std::string(static_cast<char*>(rep.data()), rep.size()));
        return {j.value("success", false), j.value("message", std::string{})};

    } catch (const zmq::error_t& e)  { return {false, "ZMQ: "  + std::string(e.what())}; }
    catch  (const json::exception& e) { return {false, "JSON: " + std::string(e.what())}; }
}
