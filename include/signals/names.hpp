#pragma once

namespace signals {
    // HAL 발행 — 실측값
    inline constexpr const char* kFrontDistance = "/sensor/front_distance";
    inline constexpr const char* kTwist         = "/sensor/twist";
    inline constexpr const char* kThrottle       = "/sensor/throttle";

    // Feature 발행 — 요청값 (Arbiter가 중재 후 command로 변환)
    inline constexpr const char* kReqSteering  = "/request/steering";
    inline constexpr const char* kReqSpeed      = "/request/speed";
    inline constexpr const char* kReqEmergency  = "/request/emergency_brake";
}
