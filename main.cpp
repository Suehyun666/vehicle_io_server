#include "DeviceManager.hpp"
#include "VehicleIOGateway.hpp"
#include "drivers/TcaSidestickDriver.hpp"
#include <iostream>
#include <csignal>
#include <atomic>

static std::atomic<bool> g_shutdown{false};

static void signalHandler(int) {
    g_shutdown = true;
    // TODO: gateway.stop() 호출 경로 추가
}

int main() {
    std::signal(SIGINT,  signalHandler);
    std::signal(SIGTERM, signalHandler);

    // ── 1. 드라이버 로드 ──────────────────────────────────────────
    DeviceManager dm;

    // 설정 파일 기반 로드 (TODO: 드라이버 factory 구현 후 활성화)
    // dm.loadFromConfig("/etc/sdv/hw_config.json");

    // ── TCA Sidestick (USB HID → /dev/input/js0) ────────────────
    // 연결 안 됐으면 connect() 실패 로그 뜨고 등록 안 됨 (서버는 계속 실행)
    dm.registerSensorDriver(
        std::make_shared<TcaSidestickDriver>("/dev/input/js0"));

    // TODO: 추가 드라이버 (구현 후 주석 해제)
    // dm.registerSensorDriver(
    //     std::make_shared<ArduinoSerialDriver>("/dev/ttyUSB0", 115200));
    // dm.registerActuatorDriver("actuator/steering",
    //     std::make_shared<ServoDriver>("/dev/ttyUSB0", 115200));
    // dm.registerActuatorDriver("actuator/relay",
    //     std::make_shared<RelayDriver>("/dev/ttyUSB1", 115200));

    // ── 2. 게이트웨이 기동 ────────────────────────────────────────
    VehicleIOGateway gateway(dm);

    if (!gateway.setup()) {
        std::cerr << "[main] Setup failed. Exiting.\n";
        return EXIT_FAILURE;
    }

    std::cout << "[main] VehicleIoServer running. Press Ctrl+C to exit.\n";
    gateway.run();  // 블로킹 — actuator listenLoop 이 반환될 때까지 대기

    return EXIT_SUCCESS;
}
