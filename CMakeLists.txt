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
find_package(gRPC CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)

set(ADPP_PROTO
    ${CMAKE_CURRENT_SOURCE_DIR}/external/anolis/spec/device-provider/protocol.proto
)

protobuf_generate_cpp(
    ADPP_PROTO_SRCS
    ADPP_PROTO_HDRS
    ${ADPP_PROTO}
)

# Generate FluxGraph protobuf + gRPC stubs from sibling repository.
# Allow override via -DFLUXGRAPH_DIR=path/to/fluxgraph
if(NOT DEFINED FLUXGRAPH_DIR)
    set(FLUXGRAPH_DIR "${CMAKE_SOURCE_DIR}/../fluxgraph" CACHE PATH "Path to FluxGraph repository")
endif()

# Normalize to absolute path
get_filename_component(FLUXGRAPH_DIR_ABS "${FLUXGRAPH_DIR}" REALPATH)
set(FLUXGRAPH_PROTO_DIR "${FLUXGRAPH_DIR_ABS}/proto")
set(FLUXGRAPH_PROTO "${FLUXGRAPH_PROTO_DIR}/fluxgraph.proto")

# Validate FluxGraph is available
if(NOT EXISTS "${FLUXGRAPH_PROTO}")
    message(FATAL_ERROR 
        "FluxGraph proto not found at: ${FLUXGRAPH_PROTO}\n"
        "Please ensure FluxGraph repository is cloned:\n"
        "  Expected location: ${FLUXGRAPH_DIR_ABS}\n"
        "  Or set custom path: cmake -DFLUXGRAPH_DIR=/path/to/fluxgraph ...\n"
        "  Clone command: git clone https://github.com/FEASTorg/fluxgraph.git ../fluxgraph"
    )
endif()

message(STATUS "FluxGraph proto: ${FLUXGRAPH_PROTO}")
message(STATUS "FluxGraph proto dir: ${FLUXGRAPH_PROTO_DIR}")

set(FLUXGRAPH_PROTO_OUT "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${FLUXGRAPH_PROTO_OUT}")

set(FLUXGRAPH_GEN_SRCS
    "${FLUXGRAPH_PROTO_OUT}/fluxgraph.pb.cc"
    "${FLUXGRAPH_PROTO_OUT}/fluxgraph.grpc.pb.cc"
)
set(FLUXGRAPH_GEN_HDRS
    "${FLUXGRAPH_PROTO_OUT}/fluxgraph.pb.h"
    "${FLUXGRAPH_PROTO_OUT}/fluxgraph.grpc.pb.h"
)

# Use FluxGraph's proven pattern for proto generation
add_custom_command(
    OUTPUT ${FLUXGRAPH_GEN_SRCS} ${FLUXGRAPH_GEN_HDRS}
    COMMAND protobuf::protoc
    ARGS --grpc_out=${FLUXGRAPH_PROTO_OUT}
         --cpp_out=${FLUXGRAPH_PROTO_OUT}
         -I${FLUXGRAPH_PROTO_DIR}
         --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
         ${FLUXGRAPH_PROTO}
    DEPENDS ${FLUXGRAPH_PROTO}
    COMMENT "Generating FluxGraph gRPC code"
)

add_custom_target(fluxgraph_proto_gen DEPENDS ${FLUXGRAPH_GEN_SRCS} ${FLUXGRAPH_GEN_HDRS})

add_executable(anolis-provider-sim
    src/main.cpp
    src/handlers.cpp
    src/fault_injection.cpp
    src/config.cpp
    src/flux_client.cpp
    src/config_translator.cpp
    src/devices/device_manager.cpp
    src/devices/device_factory.cpp
    src/devices/signal_registry.cpp
    src/devices/tempctl_device.cpp
    src/devices/motorctl_device.cpp
    src/devices/relayio_device.cpp
    src/devices/analogsensor_device.cpp
    src/devices/sim_control_device.cpp
    src/transport/framed_stdio.cpp
    ${ADPP_PROTO_SRCS}
    ${FLUXGRAPH_GEN_SRCS}
)

add_dependencies(anolis-provider-sim fluxgraph_proto_gen)

target_include_directories(anolis-provider-sim PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}   # generated protocol.pb.h
    ${FLUXGRAPH_PROTO_OUT}        # generated fluxgraph.pb.h / fluxgraph.grpc.pb.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(anolis-provider-sim PRIVATE
    gRPC::grpc++
    protobuf::libprotobuf
    yaml-cpp::yaml-cpp
)

if (WIN32)
    target_compile_definitions(anolis-provider-sim PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
