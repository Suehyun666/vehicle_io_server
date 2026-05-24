#pragma once
#include <string>
#include "common.hpp"

// ────────────────────────────────────────────────────────────────
//  IDriver : 모든 드라이버 공통 기반 인터페이스
//  - DeviceManager가 이 타입으로 드라이버를 관리한다
// ────────────────────────────────────────────────────────────────
class IDriver {
public:
    virtual ~IDriver() = default;

    virtual bool        connect()      = 0;  // 장치 파일 열기, 포트 초기화 등
    virtual void        disconnect()   = 0;  // 자원 해제
    virtual std::string getId() const  = 0;  // 드라이버 식별자 (로깅용)
};

// ────────────────────────────────────────────────────────────────
//  ISensorDriver : 읽기 전용 장치
//  구현 예) TcaSidestickDriver, ArduinoSerialDriver
// ────────────────────────────────────────────────────────────────
class ISensorDriver : public IDriver {
public:
    // non-blocking 확인 — 데이터 없으면 false 반환
    virtual bool       isDataAvailable() = 0;
    // 데이터 읽기 — isDataAvailable() == true 인 경우에만 호출
    virtual SensorData read()            = 0;
};

// ────────────────────────────────────────────────────────────────
//  IActuatorDriver : 쓰기 전용 장치
//  구현 예) ServoDriver, RelayDriver
// ────────────────────────────────────────────────────────────────
class IActuatorDriver : public IDriver {
public:
    virtual ActuatorResult write(const ActuatorCommand& cmd) = 0;
};
