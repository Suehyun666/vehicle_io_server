#include "bus/Subscriber.hpp"
#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;

Subscriber::Subscriber(zmq::context_t& ctx, const std::string& endpoint)
    : sock_(ctx, zmq::socket_type::sub)
{
    sock_.connect(endpoint);
}

void Subscriber::onFloat(const std::string& signal, std::function<void(float)> cb) {
    sock_.set(zmq::sockopt::subscribe, signal);
    handlers_[signal] = std::move(cb);
}

void Subscriber::onBool(const std::string& signal, std::function<void(bool)> cb) {
    sock_.set(zmq::sockopt::subscribe, signal);
    handlers_[signal] = std::move(cb);
}

void Subscriber::poll(std::chrono::milliseconds timeout) {
    zmq::pollitem_t items[] = {{static_cast<void*>(sock_), 0, ZMQ_POLLIN, 0}};
    try {
        zmq::poll(items, 1, timeout);
    } catch (const zmq::error_t& e) {
        if (e.num() == EINTR) return;
        throw;
    }
    if (!(items[0].revents & ZMQ_POLLIN)) return;

    zmq::message_t sig_msg, val_msg;
    if (!sock_.recv(sig_msg, zmq::recv_flags::none).has_value()) return;
    if (!sock_.recv(val_msg, zmq::recv_flags::none).has_value()) return;

    dispatch(std::string(static_cast<char*>(sig_msg.data()), sig_msg.size()),
             std::string(static_cast<char*>(val_msg.data()), val_msg.size()));
}

void Subscriber::dispatch(const std::string& signal, const std::string& payload) {
    const auto it = handlers_.find(signal);
    if (it == handlers_.end()) return;

    try {
        const auto  j = json::parse(payload);
        const auto& v = j.at("v");
        std::visit([&](const auto& cb) {
            using T = std::decay_t<decltype(cb)>;
            if constexpr (std::is_same_v<T, std::function<void(float)>>)
                cb(v.get<float>());
            else if constexpr (std::is_same_v<T, std::function<void(bool)>>)
                cb(v.get<bool>());
        }, it->second);
    } catch (const json::exception& e) {
        std::cerr << "[Subscriber] " << signal << ": " << e.what() << "\n";
    }
}
