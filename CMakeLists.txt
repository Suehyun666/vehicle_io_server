cmake_minimum_required(VERSION 3.16)
project(vehicle_io_server CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(ZMQ REQUIRED libzmq)
find_path(CPPZMQ_INCLUDE_DIR NAMES zmq.hpp PATHS /usr/include /usr/local/include)
if(NOT CPPZMQ_INCLUDE_DIR)
    message(FATAL_ERROR "cppzmq not found. Run: sudo apt install libcppzmq-dev")
endif()
find_package(nlohmann_json 3.2 REQUIRED)

# ── FTXUI ────────────────────────────────────────────────────────
# sudo apt install libftxui-dev
find_package(ftxui REQUIRED)

set(COMMON_INCLUDES include ${ZMQ_INCLUDE_DIRS} ${CPPZMQ_INCLUDE_DIR})
set(COMMON_LIBS     ${ZMQ_LIBRARIES} nlohmann_json::nlohmann_json pthread)
set(COMMON_OPTS     -Wall -Wextra ${ZMQ_CFLAGS_OTHER})

# ── Bus 라이브러리 — Publisher / Subscriber ───────────────────────
add_library(sdv_bus STATIC
    src/bus/Publisher.cpp
    src/bus/Subscriber.cpp
)
target_include_directories(sdv_bus PUBLIC ${COMMON_INCLUDES})
target_link_libraries(sdv_bus PUBLIC ${ZMQ_LIBRARIES} nlohmann_json::nlohmann_json)
target_compile_options(sdv_bus PRIVATE ${COMMON_OPTS})

# ── HAL 서버 ─────────────────────────────────────────────────────
add_executable(${PROJECT_NAME}
    main.cpp
    src/DeviceManager.cpp
    src/hal/HalNode.cpp
    src/drivers/TcaSidestickDriver.cpp
    src/drivers/ArduinoSerialDriver.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE ${COMMON_INCLUDES})
target_link_libraries(${PROJECT_NAME} PRIVATE sdv_bus ${COMMON_LIBS})
target_compile_options(${PROJECT_NAME} PRIVATE ${COMMON_OPTS})

# ── Feature: 수동 제어 ────────────────────────────────────────────
add_executable(manual_control features/manual_control.cpp)
target_include_directories(manual_control PRIVATE ${COMMON_INCLUDES})
target_link_libraries(manual_control PRIVATE sdv_bus ${COMMON_LIBS})
target_compile_options(manual_control PRIVATE ${COMMON_OPTS})

# ── 개발 도구 ─────────────────────────────────────────────────────
add_executable(sub_test tools/sub_test.cpp)
target_include_directories(sub_test PRIVATE ${COMMON_INCLUDES})
target_link_libraries(sub_test PRIVATE ${ZMQ_LIBRARIES})
target_compile_options(sub_test PRIVATE ${COMMON_OPTS})
