#include "bus/Publisher.hpp"
#include <nlohmann/json.hpp>
#include <iostream>

Publisher::Publisher(zmq::context_t& ctx, const std::string& endpoint)
    : sock_(ctx, zmq::socket_type::pub)
{
    sock_.bind(endpoint);
}

void Publisher::publish(const std::string& signal, float value) {
    send(signal, nlohmann::json{{"v", value}}.dump());
}

void Publisher::publish(const std::string& signal, bool value) {
    send(signal, nlohmann::json{{"v", value}}.dump());
}

void Publisher::send(const std::string& signal, const std::string& payload) {
    try {
        sock_.send(zmq::message_t(signal.data(),  signal.size()),
                   zmq::send_flags::sndmore);
        sock_.send(zmq::message_t(payload.data(), payload.size()),
                   zmq::send_flags::none);
    } catch (const zmq::error_t& e) {
        std::cerr << "[Publisher] " << e.what() << "\n";
    }
}
