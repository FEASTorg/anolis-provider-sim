cmake_minimum_required(VERSION 3.20)

project(anolis_provider_sim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ThreadSanitizer (TSAN) - Data Race Detection
option(ENABLE_TSAN "Enable ThreadSanitizer for data race detection" OFF)

if(ENABLE_TSAN)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        if(NOT WIN32)
            add_compile_options(-fsanitize=thread -g -fno-omit-frame-pointer)
            add_link_options(-fsanitize=thread)
            message(STATUS "ThreadSanitizer (TSAN) enabled for data race detection")
        else()
            message(WARNING "ThreadSanitizer not supported on Windows (Linux/macOS only)")
        endif()
    else()
        message(WARNING "ThreadSanitizer requested but compiler not supported: ${CMAKE_CXX_COMPILER_ID}")
    endif()
# Fallback: detect TSAN triplet automatically
elseif(DEFINED VCPKG_TARGET_TRIPLET AND VCPKG_TARGET_TRIPLET MATCHES "tsan")
    if(NOT WIN32)
        message(STATUS "ThreadSanitizer auto-detected via triplet: ${VCPKG_TARGET_TRIPLET}")
        add_compile_options(-fsanitize=thread -g -fno-omit-frame-pointer)
        add_link_options(-fsanitize=thread)
    endif()
endif()

find_package(Protobuf REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)

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
    src/fault_injection.cpp
    src/config.cpp
    src/devices/device_manager.cpp
    src/devices/device_factory.cpp
    src/devices/tempctl_device.cpp
    src/devices/motorctl_device.cpp
    src/devices/relayio_device.cpp
    src/devices/analogsensor_device.cpp
    src/devices/sim_control_device.cpp
    src/transport/framed_stdio.cpp
    ${ADPP_PROTO_SRCS}
)

target_include_directories(anolis-provider-sim PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}   # generated protocol.pb.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(anolis-provider-sim PRIVATE
    protobuf::libprotobuf
    yaml-cpp::yaml-cpp
)

if (WIN32)
    target_compile_definitions(anolis-provider-sim PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
