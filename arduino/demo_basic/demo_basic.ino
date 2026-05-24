/**
 * demo_basic.ino
 * SDV 하드웨어 기초 데모
 *
 * [동작]
 *   가변저항 → 서보모터 각도 제어
 *   HC-SR04  → 일정 거리 이하 접근 시 릴레이 ON / 멀어지면 OFF
 *
 * [핀 배치]
 *   A0       : 가변저항 (중간 단자)
 *   D7       : HC-SR04 TRIG
 *   D8       : HC-SR04 ECHO
 *   D9       : 서보모터 신호선
 *   D11      : 릴레이 IN
 *
 * [릴레이 모듈 종류 확인]
 *   모듈에 "Active LOW" 표시 있으면 → RELAY_ACTIVE_HIGH = false
 *   없거나 모르면 → true 먼저 시도
 */

#include <Servo.h>

// ── 핀 ────────────────────────────────────────────────────────────
constexpr int PIN_ACCEL      = A0;
constexpr int PIN_TRIG       = 7;
constexpr int PIN_ECHO       = 8;
constexpr int PIN_SERVO      = 9;
constexpr int PIN_RELAY      = 11;

// ── 설정값 ────────────────────────────────────────────────────────
constexpr float RELAY_ON_CM  = 20.0f;  // 이 거리 이하 → 릴레이 ON
constexpr float RELAY_OFF_CM = 25.0f;  // 이 거리 이상 → 릴레이 OFF
                                        // (ON~OFF 사이 구간: 이전 상태 유지 = 히스테리시스)

constexpr bool  RELAY_ACTIVE_HIGH = true; // 릴레이 모듈이 active-HIGH면 true
                                           // active-LOW 모듈이면 false로 변경

constexpr unsigned long SEND_INTERVAL_MS  = 50;    // 루프 주기 (20Hz)
constexpr unsigned long ULTRA_TIMEOUT_US  = 25000; // 초음파 타임아웃 (~4.2m)

// ── 전역 ──────────────────────────────────────────────────────────
Servo servo;
bool  relayOn = false;

// ──────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    servo.attach(PIN_SERVO);
    servo.write(90);  // 초기 중립 위치

    pinMode(PIN_TRIG,  OUTPUT);
    pinMode(PIN_ECHO,  INPUT);
    pinMode(PIN_RELAY, OUTPUT);
    relayWrite(false);  // 초기 OFF

    Serial.println("=== SDV Demo Ready ===");
    Serial.println("accel | angle | dist_cm | relay");
}

// ──────────────────────────────────────────────────────────────────
void loop() {
    unsigned long start = millis();

    // ① 가변저항 → 서보 각도
    int   raw   = analogRead(PIN_ACCEL);
    int   angle = map(raw, 0, 1023, 0, 180);
    servo.write(angle);

    // ② 초음파 → 릴레이
    float dist = readDistanceCm();
    updateRelay(dist);

    // ③ 시리얼 모니터 디버그
    Serial.print(raw);   Serial.print(" | ");
    Serial.print(angle); Serial.print("deg | ");
    Serial.print(dist);  Serial.print("cm | ");
    Serial.println(relayOn ? "ON" : "OFF");

    // 루프 주기 맞추기
    long elapsed = millis() - start;
    if (elapsed < (long)SEND_INTERVAL_MS)
        delay(SEND_INTERVAL_MS - elapsed);
}

// ── 릴레이 상태 갱신 (히스테리시스 적용) ─────────────────────────
//
//   dist < ON_CM          → ON
//   dist > OFF_CM         → OFF
//   ON_CM ≤ dist ≤ OFF_CM → 이전 상태 유지 (채터링 방지)
//   dist == 0             → 범위 초과(=장애물 없음)으로 처리 → OFF
//
void updateRelay(float dist) {
    bool shouldOn;

    if (dist > 0.0f && dist < RELAY_ON_CM) {
        shouldOn = true;
    } else if (dist == 0.0f || dist > RELAY_OFF_CM) {
        shouldOn = false;
    } else {
        return;  // 히스테리시스 구간 → 변경 없음
    }

    if (shouldOn == relayOn) return;  // 상태 변화 없으면 스킵

    relayOn = shouldOn;
    relayWrite(relayOn);
    Serial.print("[RELAY] ");
    Serial.println(relayOn ? "ON  ← 장애물 감지!" : "OFF ← 장애물 없음");
}

// ── HC-SR04 거리 측정 ─────────────────────────────────────────────
float readDistanceCm() {
    // TRIG: 10μs 펄스
    digitalWrite(PIN_TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(PIN_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_TRIG, LOW);

    // ECHO: 왕복 시간 측정
    long us = pulseIn(PIN_ECHO, HIGH, ULTRA_TIMEOUT_US);
    if (us == 0) return 0.0f;  // 타임아웃 = 범위 초과

    return us * 0.017f;  // cm 변환 (340m/s ÷ 2 ÷ 10000)
}

// ── 릴레이 출력 (active-HIGH / active-LOW 모두 지원) ─────────────
void relayWrite(bool on) {
    digitalWrite(PIN_RELAY, (RELAY_ACTIVE_HIGH ? on : !on) ? HIGH : LOW);
}
