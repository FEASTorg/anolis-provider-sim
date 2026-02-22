cmake_minimum_required(VERSION 3.20)

# Build-time feature switches (must be before project() for vcpkg)
option(ENABLE_TSAN "Enable ThreadSanitizer for data race detection" OFF)
option(ENABLE_FLUXGRAPH "Enable FluxGraph adapter for sim mode" OFF)

# Configure vcpkg features based on build options
if(ENABLE_FLUXGRAPH)
    list(APPEND VCPKG_MANIFEST_FEATURES "fluxgraph")
endif()

project(anolis_provider_sim VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(CTest)

# Guard against unsupported ABI/toolchain combinations on Windows.
# Default Windows vcpkg triplets are MSVC-based and cannot be mixed with
# MinGW/GNU unless an explicit MinGW triplet is selected.
if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(_provider_triplet "${VCPKG_TARGET_TRIPLET}")
    if(NOT _provider_triplet OR _provider_triplet MATCHES "windows")
        if(NOT _provider_triplet MATCHES "mingw")
            message(FATAL_ERROR
                "Unsupported Windows toolchain/triplet combination:\n"
                "  compiler: ${CMAKE_CXX_COMPILER_ID}\n"
                "  VCPKG_TARGET_TRIPLET: '${_provider_triplet}'\n"
                "Use MSVC presets (`dev-windows-release`, `dev-windows-debug`, `ci-windows-release`),\n"
                "or select an explicit MinGW triplet (e.g. x64-mingw-dynamic)."
            )
        endif()
    endif()
endif()

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

if(NOT DEFINED ANOLIS_PROTOCOL_DIR)
    set(ANOLIS_PROTOCOL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/anolis-protocol" CACHE PATH "Path to anolis-protocol repository")
endif()
get_filename_component(ANOLIS_PROTOCOL_DIR_ABS "${ANOLIS_PROTOCOL_DIR}" REALPATH)
set(ADPP_PROTO_DIR "${ANOLIS_PROTOCOL_DIR_ABS}/spec/device-provider")
set(ADPP_PROTO "${ADPP_PROTO_DIR}/protocol.proto")

if(NOT EXISTS "${ADPP_PROTO}")
    message(FATAL_ERROR
        "ADPP proto not found at: ${ADPP_PROTO}\n"
        "Expected anolis-protocol submodule at: ${ANOLIS_PROTOCOL_DIR_ABS}\n"
        "Run: git submodule update --init --recursive\n"
        "Or set custom path: cmake -DANOLIS_PROTOCOL_DIR=/path/to/anolis-protocol ..."
    )
endif()

add_library(adpp_proto OBJECT ${ADPP_PROTO})
target_link_libraries(adpp_proto PUBLIC protobuf::libprotobuf)
target_include_directories(adpp_proto PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

protobuf_generate(
    TARGET adpp_proto
    LANGUAGE cpp
    IMPORT_DIRS ${ADPP_PROTO_DIR}
    PROTOC_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}
)

set(SOURCES
    # Configuration
    src/config.cpp

    # Layer 1: Protocol core
    src/core/main.cpp
    src/core/handlers.cpp
    src/core/transport/framed_stdio.cpp

    # Layer 2: Devices
    src/devices/common/device_manager.cpp
    src/devices/common/device_factory.cpp
    src/devices/common/signal_registry.cpp
    src/devices/tempctl/tempctl_device.cpp
    src/devices/motorctl/motorctl_device.cpp
    src/devices/relayio/relayio_device.cpp
    src/devices/analogsensor/analogsensor_device.cpp

    # Layer 3: Simulation
    src/simulation/engines/null_engine.cpp
    src/simulation/engines/local_engine.cpp
    src/simulation/engines/remote_engine.cpp

    # Cross-cutting: Chaos engineering
    src/chaos/fault_injection.cpp
    src/chaos/chaos_control_device.cpp
)

if(ENABLE_FLUXGRAPH)
    find_package(gRPC CONFIG REQUIRED)

    # Generate FluxGraph protobuf + gRPC stubs from sibling repository.
    # Allow override via -DFLUXGRAPH_DIR=path/to/fluxgraph
    if(NOT DEFINED FLUXGRAPH_DIR)
        set(FLUXGRAPH_DIR "${CMAKE_SOURCE_DIR}/../fluxgraph" CACHE PATH "Path to FluxGraph repository")
    endif()

    # Normalize to absolute path
    get_filename_component(FLUXGRAPH_DIR_ABS "${FLUXGRAPH_DIR}" REALPATH)
    set(FLUXGRAPH_PROTO_DIR "${FLUXGRAPH_DIR_ABS}/proto")
    set(FLUXGRAPH_PROTO "${FLUXGRAPH_PROTO_DIR}/fluxgraph.proto")

    if(NOT EXISTS "${FLUXGRAPH_PROTO}")
        message(FATAL_ERROR
            "FluxGraph proto not found at: ${FLUXGRAPH_PROTO}\n"
            "Please ensure FluxGraph repository is cloned:\n"
            "  Expected location: ${FLUXGRAPH_DIR_ABS}\n"
            "  Or set custom path: cmake -DFLUXGRAPH_DIR=/path/to/fluxgraph ..."
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

    add_custom_command(
        OUTPUT ${FLUXGRAPH_GEN_SRCS} ${FLUXGRAPH_GEN_HDRS}
        COMMAND $<TARGET_FILE:protobuf::protoc>
        ARGS --grpc_out=${FLUXGRAPH_PROTO_OUT}
             --cpp_out=${FLUXGRAPH_PROTO_OUT}
             -I${FLUXGRAPH_PROTO_DIR}
             --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
             ${FLUXGRAPH_PROTO}
        DEPENDS ${FLUXGRAPH_PROTO}
        COMMENT "Generating FluxGraph gRPC code"
        VERBATIM
    )

    add_custom_target(fluxgraph_proto_gen DEPENDS ${FLUXGRAPH_GEN_SRCS} ${FLUXGRAPH_GEN_HDRS})

    list(APPEND SOURCES
        src/simulation/adapters/fluxgraph/fluxgraph_client.cpp
        src/simulation/adapters/fluxgraph/fluxgraph_adapter.cpp
        ${FLUXGRAPH_GEN_SRCS}
    )

    message(STATUS "FluxGraph adapter: ENABLED")
else()
    message(STATUS "FluxGraph adapter: DISABLED (inert + non_interacting only)")
endif()

add_executable(anolis-provider-sim
    ${SOURCES}
)

if(ENABLE_FLUXGRAPH)
    add_dependencies(anolis-provider-sim fluxgraph_proto_gen)
endif()

target_include_directories(anolis-provider-sim PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(ENABLE_FLUXGRAPH)
    target_include_directories(anolis-provider-sim PRIVATE
        ${FLUXGRAPH_PROTO_OUT}
    )
endif()

target_link_libraries(anolis-provider-sim PRIVATE
    adpp_proto
    protobuf::libprotobuf
    yaml-cpp::yaml-cpp
)

if(ENABLE_FLUXGRAPH)
    target_compile_definitions(anolis-provider-sim PRIVATE HAVE_FLUXGRAPH)
    target_link_libraries(anolis-provider-sim PRIVATE
        gRPC::grpc++
    )
endif()

if (WIN32)
    target_compile_definitions(anolis-provider-sim PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

if(BUILD_TESTING)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)

    if(WIN32)
        set(_python_gen_cmd
            powershell.exe -NoProfile -ExecutionPolicy Bypass
            -File ${CMAKE_SOURCE_DIR}/scripts/generate_proto_python.ps1
            -OutputDir ${CMAKE_BINARY_DIR}
        )
    else()
        set(_python_gen_cmd
            bash ${CMAKE_SOURCE_DIR}/scripts/generate_proto_python.sh ${CMAKE_BINARY_DIR}
        )
    endif()

    add_test(NAME provider.generate_proto_python COMMAND ${_python_gen_cmd})
    set_tests_properties(provider.generate_proto_python PROPERTIES
        LABELS "provider;integration"
        FIXTURES_SETUP proto_python
    )

    set(_provider_test_env
        "ANOLIS_PROVIDER_SIM_EXE=$<TARGET_FILE:anolis-provider-sim>"
        "ANOLIS_PROVIDER_SIM_BUILD_DIR=${CMAKE_BINARY_DIR}"
    )

    add_test(
        NAME provider.smoke
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_hello.py
    )
    set_tests_properties(provider.smoke PROPERTIES
        LABELS "provider;integration;smoke"
        FIXTURES_REQUIRED proto_python
        ENVIRONMENT "${_provider_test_env}"
    )

    add_test(
        NAME provider.adpp
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_adpp_integration.py --test all
    )
    set_tests_properties(provider.adpp PROPERTIES
        LABELS "provider;integration;adpp"
        FIXTURES_REQUIRED proto_python
        ENVIRONMENT "${_provider_test_env}"
    )

    add_test(
        NAME provider.multi
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_multi_instance.py --config ${CMAKE_SOURCE_DIR}/config/multi-tempctl.yaml
    )
    set_tests_properties(provider.multi PROPERTIES
        LABELS "provider;integration;multi"
        FIXTURES_REQUIRED proto_python
        ENVIRONMENT "${_provider_test_env}"
    )

    add_test(
        NAME provider.fault
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_fault_injection.py --test all
    )
    set_tests_properties(provider.fault PROPERTIES
        LABELS "provider;integration;fault"
        FIXTURES_REQUIRED proto_python
        ENVIRONMENT "${_provider_test_env}"
    )

    if(ENABLE_FLUXGRAPH)
        add_test(
            NAME provider.fluxgraph.integration
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_fluxgraph_integration.py
        )
        set_tests_properties(provider.fluxgraph.integration PROPERTIES
            LABELS "integration;fluxgraph"
            FIXTURES_REQUIRED proto_python
            ENVIRONMENT "${_provider_test_env}"
        )

        add_test(
            NAME provider.fluxgraph.multi_provider
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_multi_provider_scenario.py
        )
        set_tests_properties(provider.fluxgraph.multi_provider PROPERTIES
            LABELS "integration;fluxgraph"
            FIXTURES_REQUIRED proto_python
            ENVIRONMENT "${_provider_test_env}"
        )
    endif()
endif()
