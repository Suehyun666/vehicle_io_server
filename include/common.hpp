#pragma once
#include <string>
#include <cstdint>
#include <chrono>

// ────────────────────────────────────────────────────────────────
//  SensorGateway → 피처 앱으로 전파되는 센서 메시지
// ────────────────────────────────────────────────────────────────
struct SensorData {
    std::string topic;      // e.g. "sensor/tca/stick", "sensor/arduino/accel"
    std::string payload;    // JSON string, e.g. {"x":0.5,"y":-0.3,"ts":1234567890}
    int64_t     timestamp_ms;

    static int64_t now_ms() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }
};

// ────────────────────────────────────────────────────────────────
//  피처 앱 → ActuatorGateway로 전달하는 제어 명령
// ────────────────────────────────────────────────────────────────
struct ActuatorCommand {
    std::string target;       // e.g. "actuator/steering", "actuator/relay"
    double      value;        // 정규화 값 (의미는 드라이버가 해석)
    std::string feature_id;  // 명령을 보낸 피처 이름 (로깅용)
};

// ────────────────────────────────────────────────────────────────
//  ActuatorGateway → 피처 앱으로 돌아오는 실행 결과
// ────────────────────────────────────────────────────────────────
struct ActuatorResult {
    bool        success;
    std::string message;
};
