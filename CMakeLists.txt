cmake_minimum_required(VERSION 3.16)
project(vehicle_io_server CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(ZMQ REQUIRED libzmq)
find_path(CPPZMQ_INCLUDE_DIR NAMES zmq.hpp
    PATHS /usr/include /usr/local/include
)
if(NOT CPPZMQ_INCLUDE_DIR)
    message(FATAL_ERROR "cppzmq (zmq.hpp) not found. Run: sudo apt install libcppzmq-dev")
endif()

find_package(nlohmann_json 3.2 REQUIRED)

# src
set(SOURCES
    src/DeviceManager.cpp
    src/SensorGateway.cpp
    src/ActuatorGateway.cpp
    src/VehicleIOGateway.cpp
    # drivers
    src/drivers/TcaSidestickDriver.cpp
    src/drivers/ArduinoSerialDriver.cpp
)

add_executable(${PROJECT_NAME} main.cpp ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    include
    ${ZMQ_INCLUDE_DIRS}
    ${CPPZMQ_INCLUDE_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${ZMQ_LIBRARIES}
    nlohmann_json::nlohmann_json
    pthread
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall -Wextra
    ${ZMQ_CFLAGS_OTHER}
)

# ── 개발 도구 ────────────────────────────────────────────────────
add_executable(sub_test tools/sub_test.cpp)
target_include_directories(sub_test PRIVATE ${ZMQ_INCLUDE_DIRS} ${CPPZMQ_INCLUDE_DIR})
target_link_libraries(sub_test PRIVATE ${ZMQ_LIBRARIES})
target_compile_options(sub_test PRIVATE -Wall -Wextra ${ZMQ_CFLAGS_OTHER})

add_executable(demo_feature tools/demo_feature.cpp)
target_include_directories(demo_feature PRIVATE ${ZMQ_INCLUDE_DIRS} ${CPPZMQ_INCLUDE_DIR})
target_link_libraries(demo_feature PRIVATE ${ZMQ_LIBRARIES} nlohmann_json::nlohmann_json)
target_compile_options(demo_feature PRIVATE -Wall -Wextra ${ZMQ_CFLAGS_OTHER})
