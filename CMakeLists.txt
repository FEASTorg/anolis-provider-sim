cmake_minimum_required(VERSION 3.20)

project(anolis_provider_sim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(Protobuf REQUIRED)

set(ADPP_PROTO
    ${CMAKE_CURRENT_SOURCE_DIR}/external/anolis/spec/device-provider/protocol.proto
)

protobuf_generate_cpp(
    ADPP_PROTO_SRCS
    ADPP_PROTO_HDRS
    ${ADPP_PROTO}
)

add_executable(anolis-provider-sim
    src/main.cpp
    src/handlers.cpp
    src/devices/device_manager.cpp
    src/devices/tempctl_device.cpp
    src/devices/motorctl_device.cpp
    src/transport/framed_stdio.cpp
    ${ADPP_PROTO_SRCS}
)

target_include_directories(anolis-provider-sim PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}   # generated protocol.pb.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(anolis-provider-sim PRIVATE
    protobuf::libprotobuf
)

if (WIN32)
    target_compile_definitions(anolis-provider-sim PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
