/**
 * vehicle_node.ino  —  SDV Arduino I/O Node
 *
 * [센서]  HC-SR04 → JSON → PC
 * [액추에이터] PC → JSON → 서보모터 / 릴레이
 *
 * 전송 형식 (Arduino → PC, 20Hz):
 *   {"dist":45.2}\n
 *
 * 수신 형식 (PC → Arduino):
 *   {"target":"servo1","value":90}\n
 *   {"target":"relay","value":1}\n
 */

#include <ArduinoJson.h>
#include <Servo.h>

// ── 핀 ────────────────────────────────────────────────────────────
constexpr int PIN_TRIG  = 7;
constexpr int PIN_ECHO  = 8;
constexpr int PIN_SERVO = 9;
constexpr int PIN_RELAY = 11;

constexpr bool RELAY_ACTIVE_HIGH   = true;
constexpr unsigned long SEND_MS    = 50;      // 20Hz
constexpr unsigned long TIMEOUT_US = 25000;   // ~4.2m

// ── 전역 ──────────────────────────────────────────────────────────
Servo servo;
char  cmdBuf[64];
int   cmdLen    = 0;
unsigned long lastSendMs = 0;

// ──────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    servo.attach(PIN_SERVO);
    servo.write(90);

    pinMode(PIN_TRIG,  OUTPUT);
    pinMode(PIN_ECHO,  INPUT);
    pinMode(PIN_RELAY, OUTPUT);
    relayWrite(false);
}

void loop() {
    // PC → Arduino 명령 수신 (non-blocking)
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n') {
            cmdBuf[cmdLen] = '\0';
            handleCommand(cmdBuf);
            cmdLen = 0;
        } else if (cmdLen < (int)sizeof(cmdBuf) - 1) {
            cmdBuf[cmdLen++] = c;
        }
    }

    // Arduino → PC 센서 전송 (20Hz)
    if (millis() - lastSendMs >= SEND_MS) {
        lastSendMs = millis();
        sendSensor();
    }
}

// ── 센서 전송 ─────────────────────────────────────────────────────
void sendSensor() {
    StaticJsonDocument<32> doc;
    doc["dist"] = readDistCm();
    serializeJson(doc, Serial);
    Serial.print('\n');
}

// ── 명령 처리 ─────────────────────────────────────────────────────
void handleCommand(const char* raw) {
    StaticJsonDocument<64> doc;
    if (deserializeJson(doc, raw) != DeserializationError::Ok) return;

    const char* target = doc["target"] | "";
    int         value  = doc["value"]  | 0;

    if      (strcmp(target, "servo1") == 0) servo.write(constrain(value, 0, 180));
    else if (strcmp(target, "relay")  == 0) relayWrite(value != 0);
}

// ── HC-SR04 ───────────────────────────────────────────────────────
float readDistCm() {
    digitalWrite(PIN_TRIG, LOW);  delayMicroseconds(2);
    digitalWrite(PIN_TRIG, HIGH); delayMicroseconds(10);
    digitalWrite(PIN_TRIG, LOW);
    long us = pulseIn(PIN_ECHO, HIGH, TIMEOUT_US);
    return (us == 0) ? 0.0f : us * 0.017f;
}

// ── 릴레이 ───────────────────────────────────────────────────────
void relayWrite(bool on) {
    digitalWrite(PIN_RELAY, (RELAY_ACTIVE_HIGH ? on : !on) ? HIGH : LOW);
}
