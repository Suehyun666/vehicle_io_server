#pragma once
#include "DeviceManager.hpp"
#include <zmq.hpp>
#include <atomic>
#include <string>
#include <thread>

// ────────────────────────────────────────────────────────────────
//  SensorGateway
//  - 모든 ISensorDriver를 주기적으로 폴링 (백그라운드 스레드)
//  - 새 데이터가 있으면 ZMQ PUB 소켓으로 멀티파트 발행
//    Frame[0]: topic   e.g. "sensor/tca/stick"
//    Frame[1]: payload JSON string
// ────────────────────────────────────────────────────────────────
class SensorGateway {
public:
    // zmq_ctx 는 VehicleIOGateway 가 소유하고 공유 컨텍스트로 전달
    SensorGateway(DeviceManager&     dm,
                  zmq::context_t&    zmq_ctx,
                  const std::string& pub_endpoint);
    ~SensorGateway();

    bool initialize();  // PUB 소켓 bind
    void start();       // 폴링 스레드 기동
    void stop();        // 스레드 안전 종료

private:
    void pollLoop();                    // 드라이버 순회 → 발행
    void publish(const SensorData& d); // ZMQ multipart 전송

    DeviceManager&    dm_;
    zmq::context_t&   zmq_ctx_;
    std::string       pub_endpoint_;
    zmq::socket_t     publisher_;
    std::thread       worker_;
    std::atomic<bool> running_{false};
};
