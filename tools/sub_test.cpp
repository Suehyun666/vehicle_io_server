#include <zmq.hpp>
#include <iostream>
#include <string>

// 사용법:
//   ./cmake-build-debug/sub_test [topic_prefix]
//   ./cmake-build-debug/sub_test sensor/tca/stick
//   ./cmake-build-debug/sub_test              ← 전체 토픽 수신
int main(int argc, char* argv[]) {
    const std::string endpoint = "ipc:///tmp/sdv_sensor.ipc";
    const std::string filter   = (argc > 1) ? argv[1] : "";  // 빈 문자열 = 전체 구독

    zmq::context_t ctx{1};
    zmq::socket_t  sub(ctx, zmq::socket_type::sub);

    sub.connect(endpoint);
    sub.set(zmq::sockopt::subscribe, filter);

    std::cout << "[sub_test] connected: " << endpoint << "\n"
              << "[sub_test] filter   : \""
              << (filter.empty() ? "(all)" : filter) << "\"\n"
              << "[sub_test] waiting for messages...\n\n";

    while (true) {
        // Frame 0: topic
        zmq::message_t topic_msg;
        auto res = sub.recv(topic_msg, zmq::recv_flags::none);
        if (!res.has_value()) continue;

        // Frame 1: payload
        zmq::message_t payload_msg;
        auto res2 = sub.recv(payload_msg, zmq::recv_flags::none);
        if (!res2.has_value()) continue;

        std::string topic  (static_cast<char*>(topic_msg.data()),   topic_msg.size());
        std::string payload(static_cast<char*>(payload_msg.data()), payload_msg.size());

        std::cout << "[" << topic << "]\n" << payload << "\n\n";
    }
}
