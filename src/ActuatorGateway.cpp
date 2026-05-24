#include "ActuatorGateway.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <chrono>
#include <thread>

using json = nlohmann::json;

ActuatorGateway::ActuatorGateway(DeviceManager&     dm,
                                 zmq::context_t&    zmq_ctx,
                                 const std::string& rep_endpoint)
    : dm_(dm)
    , zmq_ctx_(zmq_ctx)
    , rep_endpoint_(rep_endpoint)
    , responder_(zmq_ctx_, zmq::socket_type::rep)
{}

ActuatorGateway::~ActuatorGateway() {
    stop();
}

bool ActuatorGateway::initialize() {
    try {
        responder_.bind(rep_endpoint_);
        std::cout << "[ActuatorGateway] REP bound: " << rep_endpoint_ << "\n";
        return true;
    } catch (const zmq::error_t& e) {
        std::cerr << "[ActuatorGateway] bind failed: " << e.what() << "\n";
        return false;
    }
}

// ── 메인 스레드 블로킹 루프 ──────────────────────────────────────
// zmq::poll 로 10ms 타임아웃을 주어 running_ 플래그를 주기적으로 확인한다.
void ActuatorGateway::listenLoop() {
    running_ = true;
    std::cout << "[ActuatorGateway] Listening for commands...\n";

    zmq::pollitem_t items[] = {
        {static_cast<void*>(responder_), 0, ZMQ_POLLIN, 0}
    };

    while (running_) {
        try {
            zmq::poll(items, 1, std::chrono::milliseconds(10));
        } catch (const zmq::error_t& e) {
            if (e.num() == EINTR) break;  // Ctrl+C → 정상 종료
            throw;
        }

        if (!(items[0].revents & ZMQ_POLLIN)) continue;

        // ── 수신 ──────────────────────────────────────────────────
        zmq::message_t msg;
        auto recv_res = responder_.recv(msg, zmq::recv_flags::none);
        if (!recv_res.has_value()) continue;  // poll 통과 후 실패는 이론상 없지만 방어
        std::string raw(static_cast<char*>(msg.data()), msg.size());

        // ── 파싱 & 처리 ───────────────────────────────────────────
        ActuatorCommand cmd;
        ActuatorResult  result;

        try {
            auto j        = json::parse(raw);
            cmd.target     = j.at("target").get<std::string>();
            cmd.value      = j.at("value").get<double>();
            cmd.feature_id = j.value("feature_id", "unknown");

            result = handleCommand(cmd);
        } catch (const json::exception& e) {
            result = {false, std::string("JSON parse error: ") + e.what()};
            std::cerr << "[ActuatorGateway] " << result.message << "\n";
        }

        // ── 응답 ──────────────────────────────────────────────────
        json reply = {{"success", result.success}, {"message", result.message}};
        std::string reply_str = reply.dump();
        zmq::message_t reply_msg(reply_str.data(), reply_str.size());
        responder_.send(reply_msg, zmq::send_flags::none);
    }
    std::cout << "[ActuatorGateway] Stopped\n";
}

void ActuatorGateway::stop() {
    running_ = false;
}

// ── 드라이버 조회 & 위임 ─────────────────────────────────────────
ActuatorResult ActuatorGateway::handleCommand(const ActuatorCommand& cmd) {
    auto driver = dm_.findActuatorDriver(cmd.target);

    if (!driver) {
        std::cerr << "[ActuatorGateway] No driver for target: " << cmd.target << "\n";
        return {false, "Driver not found: " + cmd.target};
    }

    std::cout << "[ActuatorGateway] cmd | target=" << cmd.target
              << " value=" << cmd.value
              << " from=" << cmd.feature_id << "\n";

    return driver->write(cmd);
}
