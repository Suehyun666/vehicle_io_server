#include "drivers/ArduinoSerialDriver.hpp"
#include <nlohmann/json.hpp>

#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <cerrno>
#include <cstring>
#include <iostream>

using json = nlohmann::json;

ArduinoSerialDriver::ArduinoSerialDriver(const std::string& port, int baud)
    : port_(port), baud_(baud)
{}

ArduinoSerialDriver::~ArduinoSerialDriver() { disconnect(); }

// ── IDriver ────────────────────────────────────────────────────────

bool ArduinoSerialDriver::connect() {
    if (fd_ >= 0) return true;  // 이미 연결됨 (중복 등록 시 방어)

    fd_ = ::open(port_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) {
        std::cerr << "[ArduinoSerialDriver] open(" << port_ << ") failed: "
                  << std::strerror(errno) << "\n"
                  << "  → ls /dev/ttyACM* /dev/ttyUSB* 로 포트 확인\n";
        return false;
    }

    // ── termios 설정 (8N1, 115200) ──────────────────────────────
    struct termios tty{};
    if (tcgetattr(fd_, &tty) != 0) {
        std::cerr << "[ArduinoSerialDriver] tcgetattr failed\n";
        ::close(fd_); fd_ = -1; return false;
    }

    cfsetispeed(&tty, B115200);
    cfsetospeed(&tty, B115200);

    tty.c_cflag  = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_cflag |=  CLOCAL | CREAD;
    tty.c_cflag &= ~(PARENB | PARODD | CSTOPB | CRTSCTS);

    tty.c_iflag &= ~(IXON | IXOFF | IXANY |
                     IGNBRK | BRKINT | PARMRK |
                     ISTRIP | INLCR | IGNCR | ICRNL);
    tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tty.c_oflag &= ~OPOST;

    tty.c_cc[VMIN]  = 0;  // non-blocking read
    tty.c_cc[VTIME] = 0;

    tcsetattr(fd_, TCSANOW, &tty);
    tcflush(fd_, TCIOFLUSH);  // 연결 직후 쓰레기 데이터 버림

    std::cout << "[ArduinoSerialDriver] Connected: " << port_
              << " @ " << baud_ << " baud\n";
    return true;
}

void ArduinoSerialDriver::disconnect() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
        std::cout << "[ArduinoSerialDriver] Disconnected: " << port_ << "\n";
    }
}

std::string ArduinoSerialDriver::getId() const {
    return "ArduinoSerial:" + port_;
}

// ── ISensorDriver ──────────────────────────────────────────────────

// 시리얼에서 읽어 큐에 쌓은 뒤, 큐가 비어있지 않으면 true 반환
bool ArduinoSerialDriver::isDataAvailable() {
    if (fd_ < 0) return false;
    tryReadLines();
    return !data_queue_.empty();
}

SensorData ArduinoSerialDriver::read() {
    SensorData d = data_queue_.front();
    data_queue_.pop();
    return d;
}

// ── IActuatorDriver ────────────────────────────────────────────────

// "actuator/servo1" → "servo1", "actuator/relay" → "relay" 로 추출해서 전송
ActuatorResult ArduinoSerialDriver::write(const ActuatorCommand& cmd) {
    if (fd_ < 0) return {false, "Not connected"};

    // target 마지막 세그먼트 추출
    std::string tgt = cmd.target;
    auto pos = tgt.rfind('/');
    if (pos != std::string::npos) tgt = tgt.substr(pos + 1);

    json j = {{"target", tgt}, {"value", static_cast<int>(cmd.value)}};
    std::string msg = j.dump() + "\n";

    ssize_t written = ::write(fd_, msg.c_str(), msg.size());
    if (written < 0) {
        return {false, std::string("write failed: ") + std::strerror(errno)};
    }
    return {true, "OK"};
}

// ── private ────────────────────────────────────────────────────────

// 시리얼 버퍼에서 읽어 줄 단위로 파싱
bool ArduinoSerialDriver::tryReadLines() {
    char buf[128];
    ssize_t n = ::read(fd_, buf, sizeof(buf));
    if (n <= 0) return false;

    for (ssize_t i = 0; i < n; ++i) {
        char c = buf[i];
        if (c == '\n') {
            if (!line_buf_.empty()) {
                parseLine(line_buf_);
                line_buf_.clear();
            }
        } else if (c != '\r') {
            line_buf_ += c;
        }
    }
    return true;
}

// {"dist":45.2} → SensorData 생성 후 큐에 push
bool ArduinoSerialDriver::parseLine(const std::string& line) {
    try {
        auto j  = json::parse(line);
        int64_t ts = SensorData::now_ms();

        if (j.contains("dist")) {
            float dist = j["dist"].get<float>();
            json payload = {{"distance_cm", dist}, {"ts", ts}};
            data_queue_.push({kTopicDist, payload.dump(), ts});
            return true;
        }
    } catch (const json::exception&) {
        // 불완전한 패킷 등 → 무시
    }
    return false;
}
