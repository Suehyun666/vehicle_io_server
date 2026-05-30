#pragma once
#include "services/sensor_types.hpp"
#include <nlohmann/json.hpp>
#include <string>

// parse / serialize 를 한 곳에서 관리
// SensorGateway(Skeleton 쪽)와 SensorServiceProxy(Proxy 쪽) 양쪽이 공유

namespace sdv::serde {

inline StickData parseStick(const std::string& jstr) {
    StickData d;
    try {
        auto j           = nlohmann::json::parse(jstr);
        d.x              = j.value("x",             0.0f);
        d.y              = j.value("y",             0.0f);
        d.twist          = j.value("twist",         0.0f);
        d.throttle_pct   = j.value("throttle_pct",  0.0f);
        d.throttle_raw   = j.value("throttle_raw",  0.0f);
        d.reverse_thrust = j.value("reverse_thrust", false);
        d.ts             = j.value("ts",             int64_t{0});
    } catch (...) {}
    return d;
}

inline DistData parseDist(const std::string& jstr) {
    DistData d;
    try {
        auto j        = nlohmann::json::parse(jstr);
        d.distance_cm = j.value("distance_cm", 0.0f);
        d.ts          = j.value("ts",          int64_t{0});
    } catch (...) {}
    return d;
}

inline std::string serializeStick(const StickData& d) {
    return nlohmann::json{
        {"x",              d.x},
        {"y",              d.y},
        {"twist",          d.twist},
        {"throttle_pct",   d.throttle_pct},
        {"throttle_raw",   d.throttle_raw},
        {"reverse_thrust", d.reverse_thrust},
        {"ts",             d.ts}
    }.dump();
}

inline std::string serializeDist(const DistData& d) {
    return nlohmann::json{{"distance_cm", d.distance_cm}, {"ts", d.ts}}.dump();
}

} // namespace sdv::serde
