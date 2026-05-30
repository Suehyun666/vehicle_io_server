#include "services/SensorServiceSkeleton.hpp"
#include "services/sensor_serialization.hpp"
#include "sdv_topics.hpp"
#include <iostream>

SensorServiceSkeleton::SensorServiceSkeleton(zmq::context_t& ctx,
                                             const std::string& endpoint)
    : publisher_(ctx, zmq::socket_type::pub)
    , endpoint_(endpoint)
{}

bool SensorServiceSkeleton::bind() {
    try {
        publisher_.bind(endpoint_);
        std::cout << "[SensorSkeleton] PUB bound: " << endpoint_ << "\n";
        return true;
    } catch (const zmq::error_t& e) {
        std::cerr << "[SensorSkeleton] bind failed: " << e.what() << "\n";
        return false;
    }
}

void SensorServiceSkeleton::publishStick(const StickData& d) {
    publishRaw(sdv::topics::kStick, sdv::serde::serializeStick(d));
}

void SensorServiceSkeleton::publishDist(const DistData& d) {
    publishRaw(sdv::topics::kDist, sdv::serde::serializeDist(d));
}

void SensorServiceSkeleton::publishRaw(const std::string& topic,
                                       const std::string& payload) {
    try {
        publisher_.send(zmq::message_t(topic.data(),   topic.size()),
                        zmq::send_flags::sndmore);
        publisher_.send(zmq::message_t(payload.data(), payload.size()),
                        zmq::send_flags::none);
    } catch (const zmq::error_t& e) {
        std::cerr << "[SensorSkeleton] publish error: " << e.what() << "\n";
    }
}
