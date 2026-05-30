#pragma once
#include "IDriver.hpp"
#include <queue>
#include <string>

// ────────────────────────────────────────────────────────────────
//  ArduinoSerialDriver
//  - ISensorDriver  : HC-SR04 거리값 수신 → sensor/arduino/dist 발행
//  - IActuatorDriver: servo1 / relay 명령 → 시리얼 JSON 전송
//
//  Arduino ↔ PC 프로토콜 (115200, newline 구분):
//    Arduino → PC : {"dist":45.2}\n
//    PC → Arduino : {"target":"servo1","value":90}\n
//                   {"target":"relay","value":1}\n
// ────────────────────────────────────────────────────────────────
class ArduinoSerialDriver : public ISensorDriver, public IActuatorDriver {
public:
    static constexpr const char* kTopicDist = "arduino/dist";

    explicit ArduinoSerialDriver(const std::string& port  = "/dev/ttyACM0",
                                 int                baud  = 115200);
    ~ArduinoSerialDriver();

    // ── IDriver ────────────────────────────────────────────────
    bool        connect()         override;
    void        disconnect()      override;
    std::string getId()     const override;

    // ── ISensorDriver ──────────────────────────────────────────
    bool       isDataAvailable()  override;  // 시리얼 읽기 + 파싱
    SensorData read()             override;  // 큐에서 pop

    // ── IActuatorDriver ────────────────────────────────────────
    // target: "actuator/servo1" → Arduino 에 "servo1" 전달
    // target: "actuator/relay"  → Arduino 에 "relay"  전달
    ActuatorResult write(const ActuatorCommand& cmd) override;

private:
    bool tryReadLines();                          // fd → line_buf_ → queue
    bool parseLine(const std::string& line);      // JSON 파싱 → data_queue_

    std::string port_;
    int         baud_;
    int         fd_{-1};

    std::string            line_buf_;   // 미완성 줄 누적
    std::queue<SensorData> data_queue_; // 파싱 완료된 SensorData
};
