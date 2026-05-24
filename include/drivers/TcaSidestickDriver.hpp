#pragma once
#include "IDriver.hpp"
#include <array>
#include <string>
#include <linux/joystick.h>

// ────────────────────────────────────────────────────────────────
//  TcaSidestickDriver
//  - Thrustmaster TCA Sidestick Airbus Edition
//  - Linux joystick API (/dev/input/js*) 사용
//  - 드라이버 없이 USB HID 로 자동 인식됨 (Windows .exe 는 설정 유틸)
//
//  발행 토픽: "sensor/tca/stick"
//  실측 축 매핑 (TCA Sidestick Airbus Edition 기준):
//    axis[0] → x        : 좌=-1, 우=+1
//    axis[1] → y        : 스틱 당기면(위)=-1, 밀면(아래)=+1
//    axis[2] → twist    : 스틱 회전(yaw/rudder)  좌(반시계)=-1, 우(시계)=+1
//    axis[3] → throttle : 레버 위(최대)=-1, 아래(idle)=+1  ← 하드웨어 반전
//
//  payload 예시:
//    {
//      "x":            0.52,   // 좌우 기울기     [-1.0 ~ 1.0]  (좌: 음수)
//      "y":           -0.31,   // 전후 기울기     [-1.0 ~ 1.0]  (당기면: 음수)
//      "throttle_pct": 0.75,   // 스로틀 정규화   [0.0 ~ 1.0]   (1.0 = 최대)
//      "throttle_raw":-0.51,   // 스로틀 raw      [-1.0 ~ 1.0]  (위=-1, 하드웨어 반전값)
//      "axes_raw": [...],      // 전체 축 원시값  (디버깅용)
//      "buttons": [2, 5],      // 눌린 버튼 번호 목록
//      "ts": 1234567890
//    }
// ────────────────────────────────────────────────────────────────
class TcaSidestickDriver : public ISensorDriver {
public:
    static constexpr const char* kTopic      = "sensor/tca/stick";
    static constexpr int         kMaxAxes    = 8;
    static constexpr int         kMaxButtons = 32;

    // ── 실측 확인된 축 번호 ────────────────────────────────────────
    static constexpr int kAxisX        = 0;  // 좌우 (roll)     좌=-1, 우=+1
    static constexpr int kAxisY        = 1;  // 전후 (pitch)    당기면=-1, 밀면=+1
    static constexpr int kAxisTwist    = 2;  // 스틱 회전(yaw/rudder)  좌(반시계)=-1, 우(시계)=+1
    static constexpr int kAxisThrottle = 3;  // 스로틀 raw      위=-1, 아래=+1 (하드웨어 반전)

    // 스로틀 정규화 공식: throttle_pct = (-raw + 1) / 2
    //   raw=-1 (레버 위/최대) → pct=1.0
    //   raw= 0 (중간)         → pct=0.5
    //   raw=+1 (레버 아래/idle) → pct=0.0
    static float normalizeThrottle(float raw) { return (-raw + 1.0f) / 2.0f; }

    explicit TcaSidestickDriver(const std::string& device_path = "/dev/input/js0");
    ~TcaSidestickDriver();

    // ── IDriver ───────────────────────────────────────────────────
    bool        connect()         override;
    void        disconnect()      override;
    std::string getId()     const override;

    // ── ISensorDriver ─────────────────────────────────────────────
    bool       isDataAvailable()  override;  // poll() 0ms timeout
    SensorData read()             override;  // js_event 읽고 상태 갱신 후 발행

private:
    static float normalize(int16_t raw);  // [-32767,32767] → [-1.0, 1.0]
    std::string  buildPayload() const;

    std::string device_path_;
    int         fd_{-1};
    char        device_name_[256]{"Unknown"};

    std::array<float, kMaxAxes>    axes_{};    // 현재 축 상태 (정규화)
    std::array<int,   kMaxButtons> buttons_{}; // 현재 버튼 상태
};
