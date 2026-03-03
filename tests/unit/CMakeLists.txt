add_executable(provider_unit_tests
    config_parser_test.cpp
    ${CMAKE_SOURCE_DIR}/src/config.cpp
)

target_include_directories(provider_unit_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(provider_unit_tests PRIVATE
    GTest::gtest_main
    yaml-cpp::yaml-cpp
)

anolis_provider_sim_apply_warnings(provider_unit_tests)

if(WIN32)
    target_compile_definitions(provider_unit_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

include(GoogleTest)
gtest_discover_tests(provider_unit_tests
    TEST_PREFIX provider.unit.
    DISCOVERY_TIMEOUT 30
    PROPERTIES
        LABELS "provider;unit"
        TIMEOUT 30
)
