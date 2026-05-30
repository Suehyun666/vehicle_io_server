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

# ── 공통 설정 ─────────────────────────────────────────────────────
set(COMMON_INCLUDES include ${ZMQ_INCLUDE_DIRS} ${CPPZMQ_INCLUDE_DIR})
set(COMMON_LIBS     ${ZMQ_LIBRARIES} nlohmann_json::nlohmann_json pthread)
set(COMMON_OPTS     -Wall -Wextra ${ZMQ_CFLAGS_OTHER})

# ── 서비스 레이어 라이브러리 (Proxy / Skeleton) ───────────────────
# vehicle_io_server 와 피처 앱(demo_feature) 이 공유
add_library(sdv_services STATIC
    src/services/SensorServiceSkeleton.cpp
    src/services/SensorServiceProxy.cpp
    src/services/ActuatorServiceSkeleton.cpp
    src/services/ActuatorServiceProxy.cpp
)
target_include_directories(sdv_services PUBLIC ${COMMON_INCLUDES})
target_link_libraries(sdv_services PUBLIC ${ZMQ_LIBRARIES} nlohmann_json::nlohmann_json)
target_compile_options(sdv_services PRIVATE ${COMMON_OPTS})

# ── 메인 서버 ─────────────────────────────────────────────────────
add_executable(${PROJECT_NAME}
    main.cpp
    src/DeviceManager.cpp
    src/SensorGateway.cpp
    src/ActuatorGateway.cpp
    src/VehicleIOGateway.cpp
    src/drivers/TcaSidestickDriver.cpp
    src/drivers/ArduinoSerialDriver.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE ${COMMON_INCLUDES})
target_link_libraries(${PROJECT_NAME} PRIVATE sdv_services ${COMMON_LIBS})
target_compile_options(${PROJECT_NAME} PRIVATE ${COMMON_OPTS})

# ── 개발 도구 ─────────────────────────────────────────────────────
add_executable(sub_test tools/sub_test.cpp)
target_include_directories(sub_test PRIVATE ${COMMON_INCLUDES})
target_link_libraries(sub_test PRIVATE ${ZMQ_LIBRARIES})
target_compile_options(sub_test PRIVATE ${COMMON_OPTS})

# demo_feature 는 sdv_services 만 링크 — 드라이버 코드 불필요
add_executable(demo_feature tools/demo_feature.cpp)
target_include_directories(demo_feature PRIVATE ${COMMON_INCLUDES})
target_link_libraries(demo_feature PRIVATE sdv_services ${COMMON_LIBS})
target_compile_options(demo_feature PRIVATE ${COMMON_OPTS})
