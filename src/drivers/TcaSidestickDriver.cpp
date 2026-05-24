#include "drivers/TcaSidestickDriver.hpp"
#include <nlohmann/json.hpp>

#include <fcntl.h>      // open, O_RDONLY, O_NONBLOCK
#include <unistd.h>     // read, close
#include <sys/ioctl.h>  // ioctl
#include <poll.h>       // poll, pollfd
#include <cerrno>
#include <cstring>
#include <iostream>

using json = nlohmann::json;

TcaSidestickDriver::TcaSidestickDriver(const std::string& device_path)
    : device_path_(device_path)
{
    axes_.fill(0.0f);
    buttons_.fill(0);
}

TcaSidestickDriver::~TcaSidestickDriver() {
    disconnect();
}

// ── IDriver ────────────────────────────────────────────────────────

bool TcaSidestickDriver::connect() {
    // O_NONBLOCK: isDataAvailable() 에서 poll() 과 함께 사용
    fd_ = ::open(device_path_.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd_ < 0) {
        std::cerr << "[TcaSidestickDriver] open(" << device_path_ << ") failed: "
                  << std::strerror(errno) << "\n"
                  << "  → lsusb | grep -i thrustmaster  로 USB 인식 확인\n"
                  << "  → ls /dev/input/js*              로 장치 번호 확인\n";
        return false;
    }

    // 장치 이름 읽기 (로깅용)
    ::ioctl(fd_, JSIOCGNAME(sizeof(device_name_)), device_name_);

    // 축/버튼 개수 확인
    uint8_t num_axes = 0, num_buttons = 0;
    ::ioctl(fd_, JSIOCGAXES,    &num_axes);
    ::ioctl(fd_, JSIOCGBUTTONS, &num_buttons);

    std::cout << "[TcaSidestickDriver] Connected: \"" << device_name_ << "\"\n"
              << "  path=" << device_path_
              << "  axes=" << (int)num_axes
              << "  buttons=" << (int)num_buttons << "\n"
              << "  topic=" << kTopic << "\n";

    // 초기 상태 이벤트(JS_EVENT_INIT) 를 소비해서 현재 위치로 axes_ 초기화
    // poll 없이 반복 read 하다가 EAGAIN 이 오면 초기화 완료
    struct js_event ev{};
    while (::read(fd_, &ev, sizeof(ev)) == sizeof(ev)) {
        uint8_t type = ev.type & ~JS_EVENT_INIT;
        if (type == JS_EVENT_AXIS   && ev.number < kMaxAxes)
            axes_[ev.number]    = normalize(ev.value);
        else if (type == JS_EVENT_BUTTON && ev.number < kMaxButtons)
            buttons_[ev.number] = ev.value;
    }

    return true;
}

void TcaSidestickDriver::disconnect() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
        std::cout << "[TcaSidestickDriver] Disconnected: " << device_path_ << "\n";
    }
}

std::string TcaSidestickDriver::getId() const {
    return std::string("TcaSidestick:") + device_path_;
}

// ── ISensorDriver ──────────────────────────────────────────────────

// poll() 0ms timeout → 블로킹 없이 데이터 유무만 확인
bool TcaSidestickDriver::isDataAvailable() {
    if (fd_ < 0) return false;
    struct pollfd pfd = {fd_, POLLIN, 0};
    return ::poll(&pfd, 1, 0) > 0;
}

// js_event 하나를 읽고 상태를 갱신한 뒤 전체 상태를 담은 SensorData 반환
// 호출 전제: isDataAvailable() == true
SensorData TcaSidestickDriver::read() {
    struct js_event ev{};
    ssize_t bytes = ::read(fd_, &ev, sizeof(ev));

    if (bytes != static_cast<ssize_t>(sizeof(ev))) {
        // EAGAIN 이거나 읽기 실패 — 빈 payload 반환
        return {kTopic, buildPayload(), SensorData::now_ms()};
    }

    // JS_EVENT_INIT 플래그 제거 후 타입 판별
    uint8_t type = ev.type & ~JS_EVENT_INIT;

    if (type == JS_EVENT_AXIS && ev.number < kMaxAxes) {
        axes_[ev.number] = normalize(ev.value);
        std::cout << "[TcaSidestickDriver] axis[" << (int)ev.number
                  << "] = " << axes_[ev.number] << "\n";
    } else if (type == JS_EVENT_BUTTON && ev.number < kMaxButtons) {
        buttons_[ev.number] = ev.value;
        std::cout << "[TcaSidestickDriver] button[" << (int)ev.number
                  << "] = " << ev.value << "\n";
    }

    return {kTopic, buildPayload(), SensorData::now_ms()};
}

// ── private ────────────────────────────────────────────────────────

float TcaSidestickDriver::normalize(int16_t raw) {
    // 32767 은 int16_t 최대값 (32768 이 아님에 주의)
    return static_cast<float>(raw) / 32767.0f;
}

std::string TcaSidestickDriver::buildPayload() const {
    const float throttle_raw = axes_[kAxisThrottle];
    const float throttle_pct = normalizeThrottle(throttle_raw);
    // y 축: 스틱 당기면(위) = -1 이 raw 값.
    // 피처가 "당기면 전진"으로 해석하려면 -y 를 쓰면 됨.
    // 여기서는 raw 그대로 노출하고 해석은 피처에 위임.
    // 버튼 16 = TCA 가상 버튼 (역추력 구간 진입 시 활성화)
    const bool reverse_thrust = (buttons_[16] == 1);

    json j = {
        {"x",             axes_[kAxisX]},     // 좌=-1, 우=+1
        {"y",             axes_[kAxisY]},     // 당기면=-1, 밀면=+1
        {"twist",         axes_[kAxisTwist]}, // 좌(반시계)=-1, 우(시계)=+1
        {"throttle_pct",  throttle_pct},      // 0.0(idle) ~ 1.0(최대)
        {"throttle_raw",  throttle_raw},      // 하드웨어 raw: 위=-1, 아래=+1
        {"reverse_thrust",reverse_thrust},    // true = 역추력 구간
        {"ts",            SensorData::now_ms()},
    };

    // 전체 축 원시값 (디버깅 · 미매핑 축 확인용)
    json raw_axes = json::array();
    for (int i = 0; i < kMaxAxes; ++i) raw_axes.push_back(axes_[i]);
    j["axes_raw"] = raw_axes;

    // 눌린 버튼 번호 목록
    json btns = json::array();
    for (int i = 0; i < kMaxButtons; ++i)
        if (buttons_[i]) btns.push_back(i);
    j["buttons"] = btns;

    return j.dump();
}
