#pragma once
#include <cstdint>

// ── 서비스 인터페이스 타입 — 피처와 HAL 서비스가 공유하는 계약 ──
// ZMQ, topic 이름, JSON 필드명은 이 파일 바깥에서 알 필요 없음

struct StickData {
    float   x             = 0.0f;  // 좌=-1, 우=+1
    float   y             = 0.0f;  // 당기면=-1, 밀면=+1
    float   twist         = 0.0f;  // 좌(반시계)=-1, 우(시계)=+1
    float   throttle_pct  = 0.0f;  // 0.0(idle) ~ 1.0(최대)
    float   throttle_raw  = 0.0f;  // 하드웨어 raw: 위=-1
    bool    reverse_thrust = false;
    int64_t ts            = 0;
};

struct DistData {
    float   distance_cm = 0.0f;
    int64_t ts          = 0;
};
