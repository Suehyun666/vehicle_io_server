#pragma once
#include <zmq.hpp>
#include <string>

class Publisher {
public:
    Publisher(zmq::context_t& ctx, const std::string& endpoint);

    void publish(const std::string& signal, float value);
    void publish(const std::string& signal, bool  value);

private:
    void send(const std::string& signal, const std::string& payload);
    zmq::socket_t sock_;
};
