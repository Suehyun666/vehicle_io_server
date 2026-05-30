#include "services/SensorServiceProxy.hpp"
#include "services/sensor_serialization.hpp"
#include "sdv_topics.hpp"
#include <iostream>

SensorServiceProxy::SensorServiceProxy(zmq::context_t& ctx,
                                       const std::string& endpoint)
    : subscriber_(ctx, zmq::socket_type::sub)
{
    subscriber_.connect(endpoint);
}

void SensorServiceProxy::subscribeStick(std::function<void(const StickData&)> cb) {
    subscriber_.set(zmq::sockopt::subscribe, sdv::topics::kStick);
    handlers_[sdv::topics::kStick] = [cb = std::move(cb)](const std::string& p) {
        cb(sdv::serde::parseStick(p));
    };
}

void SensorServiceProxy::subscribeDist(std::function<void(const DistData&)> cb) {
    subscriber_.set(zmq::sockopt::subscribe, sdv::topics::kDist);
    handlers_[sdv::topics::kDist] = [cb = std::move(cb)](const std::string& p) {
        cb(sdv::serde::parseDist(p));
    };
}

// ── handler map 기반 generic dispatch — 센서가 늘어나도 이 함수는 변경 없음 ──
void SensorServiceProxy::poll(std::chrono::milliseconds timeout) {
    zmq::pollitem_t items[] = {{static_cast<void*>(subscriber_), 0, ZMQ_POLLIN, 0}};

    try {
        zmq::poll(items, 1, timeout);
    } catch (const zmq::error_t& e) {
        if (e.num() == EINTR) return;
        throw;
    }
    if (!(items[0].revents & ZMQ_POLLIN)) return;

    zmq::message_t topic_msg, payload_msg;
    if (!subscriber_.recv(topic_msg,   zmq::recv_flags::none).has_value()) return;
    if (!subscriber_.recv(payload_msg, zmq::recv_flags::none).has_value()) return;

    const std::string topic  (static_cast<char*>(topic_msg.data()),   topic_msg.size());
    const std::string payload(static_cast<char*>(payload_msg.data()), payload_msg.size());

    const auto it = handlers_.find(topic);
    if (it != handlers_.end()) it->second(payload);
}
