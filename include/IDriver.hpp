#pragma once
#include <string>
#include "common.hpp"

// ────────────────────────────────────────────────────────────────
//  IDriver : 모든 드라이버 공통 기반 인터페이스
// ────────────────────────────────────────────────────────────────
class IDriver {
public:
    virtual ~IDriver() = default;
    virtual bool        connect()     = 0;
    virtual void        disconnect()  = 0;
    virtual std::string getId() const = 0;
};

// ────────────────────────────────────────────────────────────────
//  ISensorDriver : 읽기 전용 장치
//  virtual 상속: ArduinoSerialDriver 처럼 두 인터페이스를 동시에
//  구현할 때 IDriver 가 중복 인스턴스화되는 다이아몬드 문제 방지
// ────────────────────────────────────────────────────────────────
class ISensorDriver : public virtual IDriver {
public:
    virtual bool       isDataAvailable() = 0;
    virtual SensorData read()            = 0;
};

// ────────────────────────────────────────────────────────────────
//  IActuatorDriver : 쓰기 전용 장치
// ────────────────────────────────────────────────────────────────
class IActuatorDriver : public virtual IDriver {
public:
    virtual ActuatorResult write(const ActuatorCommand& cmd) = 0;
};
