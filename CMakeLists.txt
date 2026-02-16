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

# ===========================
# Simulation Engine Library
# ===========================
# This is the protocol-agnostic simulation engine.
# CRITICAL: This library target MUST NOT depend on ADPP protocol or device implementations.
# It depends ONLY on yaml-cpp for config parsing and ISignalSource interface.
# This enforces architectural boundaries and enables future extraction to standalone repo.

add_library(anolis_sim_engine STATIC
    src/physics/sim_physics.cpp
    src/physics/model_registry.cpp
    src/physics/thermal_mass_model.cpp
    src/physics/model_interface.hpp
    src/physics/sim_physics.hpp
    src/config.hpp
    src/config.cpp
)

target_include_directories(anolis_sim_engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(anolis_sim_engine PUBLIC
    yaml-cpp::yaml-cpp
)

# Enforce zero ADPP dependencies: do NOT link protobuf to sim_engine
# If compilation fails with protocol.pb.h not found, that's GOOD - it means
# we're enforcing separation. Fix by removing the offending include.

# ===========================
# Provider-Sim Executable
# ===========================
# This is the ADPP provider implementation that uses the sim engine library.

add_executable(anolis-provider-sim
    src/main.cpp
    src/handlers.cpp
    src/fault_injection.cpp
    src/devices/device_manager.cpp
    src/devices/device_factory.cpp
    src/devices/signal_registry.cpp
    src/devices/rule_engine.cpp
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
    anolis_sim_engine     # Simulation engine library (protocol-agnostic)
    protobuf::libprotobuf # ADPP protocol
    yaml-cpp::yaml-cpp    # Config parsing
)

if (WIN32)
    target_compile_definitions(anolis-provider-sim PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
