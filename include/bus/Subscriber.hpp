#pragma once
#include <zmq.hpp>
#include <chrono>
#include <functional>
#include <string>
#include <unordered_map>
#include <variant>

class Subscriber {
public:
    Subscriber(zmq::context_t& ctx, const std::string& endpoint);

    void onFloat(const std::string& signal, std::function<void(float)> cb);
    void onBool (const std::string& signal, std::function<void(bool)>  cb);

    // timeout 동안 대기 후 도착한 메시지를 콜백으로 전달
    void poll(std::chrono::milliseconds timeout);

private:
    using Handler = std::variant<std::function<void(float)>,
                                 std::function<void(bool)>>;

    void dispatch(const std::string& signal, const std::string& payload);

    zmq::socket_t sock_;
    std::unordered_map<std::string, Handler> handlers_;
};
