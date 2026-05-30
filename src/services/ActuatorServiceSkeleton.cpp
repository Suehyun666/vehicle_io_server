#include "services/ActuatorServiceSkeleton.hpp"
#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;

ActuatorServiceSkeleton::ActuatorServiceSkeleton(zmq::context_t& ctx,
                                                 const std::string& endpoint)
    : responder_(ctx, zmq::socket_type::rep)
    , endpoint_(endpoint)
{}

bool ActuatorServiceSkeleton::bind() {
    try {
        responder_.bind(endpoint_);
        std::cout << "[ActuatorSkeleton] REP bound: " << endpoint_ << "\n";
        return true;
    } catch (const zmq::error_t& e) {
        std::cerr << "[ActuatorSkeleton] bind failed: " << e.what() << "\n";
        return false;
    }
}

// ── JSON 수신 → target 분기 → 파생 클래스 typed 핸들러 위임 ─────
void ActuatorServiceSkeleton::listenLoop() {
    running_ = true;
    zmq::pollitem_t items[] = {{static_cast<void*>(responder_), 0, ZMQ_POLLIN, 0}};

    while (running_) {
        try {
            zmq::poll(items, 1, std::chrono::milliseconds(10));
        } catch (const zmq::error_t& e) {
            if (e.num() == EINTR) break;
            throw;
        }
        if (!(items[0].revents & ZMQ_POLLIN)) continue;

        zmq::message_t msg;
        if (!responder_.recv(msg, zmq::recv_flags::none).has_value()) continue;

        const std::string raw(static_cast<char*>(msg.data()), msg.size());
        ActuatorResult result;

        try {
            auto j   = json::parse(raw);
            auto tgt = j.at("target").get<std::string>();
            auto val = j.at("value").get<double>();

            // "vehicle/steering" → "steering"
            const auto pos = tgt.rfind('/');
            const auto cmd = (pos != std::string::npos) ? tgt.substr(pos + 1) : tgt;

            if      (cmd == "steering")       result = onSetSteering(static_cast<float>(val));
            else if (cmd == "emergencyBrake") result = onSetEmergencyBrake(val != 0.0);
            else                              result = {false, "Unknown target: " + tgt};

        } catch (const json::exception& e) {
            result = {false, std::string("JSON error: ") + e.what()};
            std::cerr << "[ActuatorSkeleton] " << result.message << "\n";
        }

        const auto rep = json{{"success", result.success}, {"message", result.message}}.dump();
        responder_.send(zmq::message_t(rep.data(), rep.size()), zmq::send_flags::none);
    }
    std::cout << "[ActuatorSkeleton] Stopped\n";
}

void ActuatorServiceSkeleton::stop() { running_ = false; }
