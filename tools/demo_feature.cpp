/**
 * demo_feature — Proxy/Skeleton 패턴 데모
 *
 * ZMQ, topic 문자열, JSON 파싱이 완전히 제거됨
 * SensorServiceProxy / ActuatorServiceProxy 만 사용
 */

#include "services/SensorServiceProxy.hpp"
#include "services/ActuatorServiceProxy.hpp"
#include "sdv_endpoints.hpp"
#include <zmq.hpp>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <csignal>
#include <iostream>

static std::atomic<bool> g_running{true};
static void sigHandler(int) { g_running = false; }

static constexpr float RELAY_ON_CM    = 20.0f;
static constexpr float RELAY_OFF_CM   = 25.0f;
static constexpr float TWIST_DEADZONE = 0.02f;

int main() {
    std::signal(SIGINT,  sigHandler);
    std::signal(SIGTERM, sigHandler);

    zmq::context_t ctx{1};

    SensorServiceProxy   sensor  (ctx, sdv::kSensorEndpoint);
    ActuatorServiceProxy actuator(ctx, sdv::kActuatorEndpoint, "demo_feature");

    bool  relay_on   = false;
    float last_twist = 999.0f;

    // ── TCA twist → 서보 각도 ─────────────────────────────────────
    sensor.subscribeStick([&](const StickData& d) {
        if (std::fabs(d.twist - last_twist) < TWIST_DEADZONE) return;
        last_twist = d.twist;

        int angle = static_cast<int>((d.twist + 1.0f) / 2.0f * 180.0f);
        angle = std::clamp(angle, 0, 180);

        actuator.setServo(static_cast<double>(angle));
        std::cout << "[demo_feature] twist=" << d.twist
                  << " → servo=" << angle << "deg\n";
    });

    // ── HC-SR04 dist → 릴레이 (히스테리시스) ─────────────────────
    sensor.subscribeDist([&](const DistData& d) {
        bool should_on;
        if      (d.distance_cm > 0.0f && d.distance_cm < RELAY_ON_CM)   should_on = true;
        else if (d.distance_cm == 0.0f || d.distance_cm > RELAY_OFF_CM) should_on = false;
        else    return;  // 히스테리시스 구간 → 유지

        if (should_on == relay_on) return;
        relay_on = should_on;

        actuator.setRelay(relay_on);
        std::cout << "[demo_feature] dist=" << d.distance_cm
                  << "cm → relay " << (relay_on ? "ON" : "OFF") << "\n";
    });

    std::cout << "[demo_feature] Running\n";

    while (g_running) {
        sensor.poll(std::chrono::milliseconds(10));
    }

    // 종료 시 안전 상태
    actuator.setRelay(false);
    actuator.setServo(90.0);
    std::cout << "[demo_feature] Stopped. Safe state applied.\n";
}
