#include "SensorGateway.hpp"
#include <iostream>
#include <chrono>

SensorGateway::SensorGateway(DeviceManager&     dm,
                             zmq::context_t&    zmq_ctx,
                             const std::string& pub_endpoint)
    : dm_(dm)
    , zmq_ctx_(zmq_ctx)
    , pub_endpoint_(pub_endpoint)
    , publisher_(zmq_ctx_, zmq::socket_type::pub)
{}

SensorGateway::~SensorGateway() {
    stop();
}

bool SensorGateway::initialize() {
    try {
        publisher_.bind(pub_endpoint_);
        std::cout << "[SensorGateway] PUB bound: " << pub_endpoint_ << "\n";
        return true;
    } catch (const zmq::error_t& e) {
        std::cerr << "[SensorGateway] bind failed: " << e.what() << "\n";
        return false;
    }
}

void SensorGateway::start() {
    running_ = true;
    worker_  = std::thread(&SensorGateway::pollLoop, this);
    std::cout << "[SensorGateway] Poll thread started\n";
}

void SensorGateway::stop() {
    running_ = false;
    if (worker_.joinable()) worker_.join();
}

// ── 센서 드라이버 폴링 루프 ─────────────────────────────────────
// 각 드라이버를 순회하며 데이터가 있으면 ZMQ PUB 으로 발행한다.
// 드라이버가 blocking read 를 쓴다면 별도 스레드 분리가 필요하다.
// (TODO: 필요 시 드라이버별 스레드로 분리)
void SensorGateway::pollLoop() {
    auto& drivers = dm_.getSensorDrivers();

    while (running_) {
        try {
            bool any_data = false;
            for (auto& driver : drivers) {
                if (driver->isDataAvailable()) {
                    publish(driver->read());
                    any_data = true;
                }
            }
            if (!any_data) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        } catch (const zmq::error_t& e) {
            if (!running_) break;  // stop() 호출 후 send 실패 → 정상
            std::cerr << "[SensorGateway] error: " << e.what() << "\n";
        }
    }
    std::cout << "[SensorGateway] Stopped\n";
}

// ── ZMQ PUB 멀티파트 전송 ────────────────────────────────────────
// 피처 앱에서 구독할 때: subscriber.setsockopt(ZMQ_SUBSCRIBE, "sensor/tca", 10)
void SensorGateway::publish(const SensorData& data) {
    try {
        zmq::message_t topic_msg  (data.topic.data(),   data.topic.size());
        zmq::message_t payload_msg(data.payload.data(), data.payload.size());

        publisher_.send(topic_msg,   zmq::send_flags::sndmore);
        publisher_.send(payload_msg, zmq::send_flags::none);
    } catch (const zmq::error_t& e) {
        std::cerr << "[SensorGateway] publish error: " << e.what() << "\n";
    }
}
