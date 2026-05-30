#include "DeviceManager.hpp"
#include "hal/HalNode.hpp"
#include "drivers/TcaSidestickDriver.hpp"
#include "drivers/ArduinoSerialDriver.hpp"
#include <zmq.hpp>
#include <atomic>
#include <csignal>
#include <iostream>
#include <thread>

static std::atomic<bool> g_running{true};
static void sigHandler(int) { g_running = false; }

int main() {
    std::signal(SIGINT,  sigHandler);
    std::signal(SIGTERM, sigHandler);

    DeviceManager dm;
    dm.registerSensorDriver(std::make_shared<TcaSidestickDriver>("/dev/input/js0"));

    auto arduino = std::make_shared<ArduinoSerialDriver>("/dev/ttyACM0", 115200);
    dm.registerSensorDriver(arduino);
    dm.registerActuatorDriver("actuator/servo1", arduino);
    dm.registerActuatorDriver("actuator/relay",  arduino);

    zmq::context_t ctx{1};
    HalNode hal(dm, ctx);

    if (!hal.start()) {
        std::cerr << "[main] HAL start failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "[main] Running. Ctrl+C to stop.\n";
    while (g_running)
        std::this_thread::sleep_for(std::chrono::seconds(1));

    hal.stop();
    return EXIT_SUCCESS;
}
