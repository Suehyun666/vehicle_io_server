/**
 * demo_feature  —  기초 피처 앱
 *
 * [로직]
 *   TCA axis[2] (twist) [-1, 1]  →  서보 각도 [0, 180]
 *   HC-SR04 dist < 20cm          →  릴레이 ON
 *   HC-SR04 dist > 25cm          →  릴레이 OFF  (히스테리시스)
 *
 * [ZMQ]
 *   SUB  ipc:///tmp/sdv_sensor.ipc    ← sensor/tca/stick, sensor/arduino/dist
 *   REQ  ipc:///tmp/sdv_actuator.ipc  → actuator/servo1, actuator/relay
 */

#include <zmq.hpp>
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <csignal>
#include <atomic>
#include <cmath>

using json = nlohmann::json;

static std::atomic<bool> g_running{true};
static void sigHandler(int) { g_running = false; }

static constexpr const char* SENSOR_EP   = "ipc:///tmp/sdv_sensor.ipc";
static constexpr const char* ACTUATOR_EP = "ipc:///tmp/sdv_actuator.ipc";

static constexpr float RELAY_ON_CM  = 20.0f;
static constexpr float RELAY_OFF_CM = 25.0f;
static constexpr float TWIST_DEADZONE = 0.02f;  // 노이즈 무시 범위

// ── 액추에이터 명령 전송 (REQ/REP) ──────────────────────────────
bool sendCmd(zmq::socket_t& req,
             const std::string& target, double value) {
    json cmd = {{"target", target}, {"value", value}, {"feature_id", "demo_feature"}};
    std::string s = cmd.dump();
    zmq::message_t req_msg(s.data(), s.size());

    try {
        req.send(req_msg, zmq::send_flags::none);
        zmq::message_t rep_msg;
        auto r = req.recv(rep_msg, zmq::recv_flags::none);
        return r.has_value();
    } catch (const zmq::error_t& e) {
        std::cerr << "[demo_feature] sendCmd error: " << e.what() << "\n";
        return false;
    }
}

int main() {
    std::signal(SIGINT,  sigHandler);
    std::signal(SIGTERM, sigHandler);

    zmq::context_t ctx{1};

    // ── 센서 구독 ────────────────────────────────────────────────
    zmq::socket_t sub(ctx, zmq::socket_type::sub);
    sub.connect(SENSOR_EP);
    sub.set(zmq::sockopt::subscribe, "sensor/tca/stick");
    sub.set(zmq::sockopt::subscribe, "sensor/arduino/dist");

    // ── 액추에이터 요청 ──────────────────────────────────────────
    zmq::socket_t req(ctx, zmq::socket_type::req);
    req.connect(ACTUATOR_EP);

    zmq::pollitem_t items[] = {{static_cast<void*>(sub), 0, ZMQ_POLLIN, 0}};

    bool  relay_on   = false;
    float last_twist = 999.0f;  // 초기값: 반드시 첫 명령이 전송되도록

    std::cout << "[demo_feature] Running\n"
              << "  TCA twist → servo | HC-SR04 dist → relay\n";

    while (g_running) {
        // ── poll (10ms 타임아웃) ─────────────────────────────────
        try {
            zmq::poll(items, 1, std::chrono::milliseconds(10));
        } catch (const zmq::error_t& e) {
            if (e.num() == EINTR) break;
            throw;
        }
        if (!(items[0].revents & ZMQ_POLLIN)) continue;

        // ── 메시지 수신 ──────────────────────────────────────────
        zmq::message_t topic_msg, payload_msg;
        auto r1 = sub.recv(topic_msg,   zmq::recv_flags::none);
        auto r2 = sub.recv(payload_msg, zmq::recv_flags::none);
        if (!r1.has_value() || !r2.has_value()) continue;

        std::string topic  (static_cast<char*>(topic_msg.data()),   topic_msg.size());
        std::string payload(static_cast<char*>(payload_msg.data()), payload_msg.size());

        try {
            auto j = json::parse(payload);

            // ── TCA twist → 서보 각도 ────────────────────────────
            if (topic == "sensor/tca/stick") {
                float twist = j.value("twist", 0.0f);

                // 데드존: 이전 값과 차이가 TWIST_DEADZONE 미만이면 스킵
                if (std::fabs(twist - last_twist) < TWIST_DEADZONE) continue;
                last_twist = twist;

                // twist [-1, 1] → angle [0, 180]
                int angle = static_cast<int>((twist + 1.0f) / 2.0f * 180.0f);
                angle = std::max(0, std::min(180, angle));

                sendCmd(req, "actuator/servo1", angle);
                std::cout << "[demo_feature] twist=" << twist
                          << " → servo=" << angle << "deg\n";
            }

            // ── HC-SR04 dist → 릴레이 ────────────────────────────
            else if (topic == "sensor/arduino/dist") {
                float dist = j.value("distance_cm", 0.0f);

                bool should_on;
                if      (dist > 0.0f && dist < RELAY_ON_CM)  should_on = true;
                else if (dist == 0.0f || dist > RELAY_OFF_CM) should_on = false;
                else    continue;  // 히스테리시스 구간 → 유지

                if (should_on == relay_on) continue;  // 변화 없음 → 스킵

                relay_on = should_on;
                sendCmd(req, "actuator/relay", relay_on ? 1.0 : 0.0);
                std::cout << "[demo_feature] dist=" << dist
                          << "cm → relay " << (relay_on ? "ON" : "OFF") << "\n";
            }

        } catch (const json::exception& e) {
            std::cerr << "[demo_feature] JSON: " << e.what() << "\n";
        }
    }

    // 종료 시 안전 상태로
    sendCmd(req, "actuator/relay",  0.0);
    sendCmd(req, "actuator/servo1", 90.0);
    std::cout << "[demo_feature] Stopped. Safe state applied.\n";
    return 0;
}
