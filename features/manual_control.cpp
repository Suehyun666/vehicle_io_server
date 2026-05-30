#include "bus/Publisher.hpp"
#include "bus/Subscriber.hpp"
#include "signals/names.hpp"
#include "endpoints.hpp"
#include <zmq.hpp>
#include <atomic>
#include <cmath>
#include <csignal>
#include <iostream>

static std::atomic<bool> g_running{true};
static void sigHandler(int) { g_running = false; }

int main() {
    std::signal(SIGINT,  sigHandler);
    std::signal(SIGTERM, sigHandler);

    zmq::context_t ctx{1};

    Subscriber sensor (ctx, endpoints::kHalPub);
    Publisher  request(ctx, endpoints::kFeatPub);

    float last_twist = 999.0f;
    bool  brake_on   = false;

    sensor.onFloat(signals::kTwist, [&](float twist) {
        if (std::fabs(twist - last_twist) < 0.02f) return;
        last_twist = twist;
        request.publish(signals::kReqSteering, twist);
        std::cout << "[manual] steering request=" << twist << "\n";
    });

    sensor.onFloat(signals::kFrontDistance, [&](float dist) {
        const bool should = dist > 0.0f && dist < 20.0f;
        if (should == brake_on) return;
        brake_on = should;
        request.publish(signals::kReqEmergency, brake_on);
        std::cout << "[manual] emergency_brake=" << (brake_on ? "ON" : "OFF")
                  << " (dist=" << dist << "cm)\n";
    });

    std::cout << "[manual_control] Running\n";

    while (g_running)
        sensor.poll(std::chrono::milliseconds(10));

    request.publish(signals::kReqSteering, 0.0f);
    request.publish(signals::kReqEmergency, false);
    std::cout << "[manual_control] Stopped\n";
}
