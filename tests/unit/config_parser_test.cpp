#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include "config.hpp"
#include "config_schema.hpp"

namespace anolis_provider_sim {
namespace {

class TempConfigFile {
public:
    explicit TempConfigFile(const std::string &yaml_body) {
        static std::atomic<unsigned long long> counter{0ULL};
        const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto id = counter.fetch_add(1ULL, std::memory_order_relaxed);

        path_ = std::filesystem::temp_directory_path() /
                ("anolis_provider_sim_config_test_" + std::to_string(nonce) + "_" + std::to_string(id) + ".yaml");

        std::ofstream out(path_);
        if (!out.is_open()) {
            throw std::runtime_error("failed to create temp config: " + path_.string());
        }
        out << yaml_body;
        out.flush();
        if (!out.good()) {
            throw std::runtime_error("failed to write temp config: " + path_.string());
        }
    }

    ~TempConfigFile() {
        std::error_code ec;
        std::filesystem::remove(path_, ec);
    }

    const std::filesystem::path &path() const { return path_; }

private:
    std::filesystem::path path_;
};

void expect_config_error(const std::string &yaml_body, const std::string &expected_token) {
    const TempConfigFile config(yaml_body);
    try {
        (void)load_config(config.path().string());
        FAIL() << "Expected load_config() to fail";
    } catch (const std::runtime_error &e) {
        const std::string message = e.what();
        EXPECT_NE(message.find(expected_token), std::string::npos)
            << "expected token: " << expected_token << "\nactual message: " << message;
    }
}

}  // namespace

TEST(ConfigParserTest, StartupPolicyDefaultsToStrict) {
    const TempConfigFile config(R"(
devices:
  - id: tempctl0
    type: tempctl
simulation:
  mode: non_interacting
  tick_rate_hz: 10.0
)");

    const ProviderConfig parsed = load_config(config.path().string());
    EXPECT_EQ(parsed.startup_policy, StartupPolicy::Strict);
}

TEST(ConfigParserTest, StartupPolicyAcceptsDegraded) {
    const TempConfigFile config(R"(
startup_policy: degraded
devices:
  - id: tempctl0
    type: tempctl
simulation:
  mode: non_interacting
  tick_rate_hz: 10.0
)");

    const ProviderConfig parsed = load_config(config.path().string());
    EXPECT_EQ(parsed.startup_policy, StartupPolicy::Degraded);
}

TEST(ConfigParserTest, RejectsDuplicateDeviceId) {
    expect_config_error(R"(
devices:
  - id: tempctl0
    type: tempctl
  - id: tempctl0
    type: motorctl
simulation:
  mode: non_interacting
  tick_rate_hz: 10.0
)",
                        "duplicate id");
}

TEST(ConfigParserTest, RejectsDeprecatedSimulationKey) {
    expect_config_error(R"(
devices:
  - id: tempctl0
    type: tempctl
simulation:
  mode: non_interacting
  tick_rate_hz: 10.0
  update_rate_hz: 20.0
)",
                        "simulation.update_rate_hz is no longer supported");
}

TEST(ConfigParserTest, RejectsUnknownSimulationKey) {
    expect_config_error(R"(
devices:
  - id: tempctl0
    type: tempctl
simulation:
  mode: non_interacting
  tick_rate_hz: 10.0
  unknown_key: 123
)",
                        "unknown key");
}

TEST(ConfigParserTest, RejectsAmbientSignalPathWithoutAmbientTemperature) {
    expect_config_error(
        R"(
devices:
  - id: tempctl0
    type: tempctl
simulation:
  mode: sim
  tick_rate_hz: 5.0
  physics_config: physics.yaml
  ambient_signal_path: environment/ambient_temp
)",
        "ambient_temp_c: is required when ambient_signal_path is present");
}

TEST(ConfigParserTest, RejectsInvalidStartupPolicyValue) {
    expect_config_error(R"(
startup_policy: permissive
devices:
  - id: tempctl0
    type: tempctl
simulation:
  mode: non_interacting
  tick_rate_hz: 10.0
)",
                        "invalid value");
}

TEST(ConfigParserTest, SchemaDefaultsAgreeWithBinaryDefaults) {
    // The schema's advertised startup_policy default must be what the binary
    // does when the key is absent (a workbench form renders from the schema).
    const TempConfigFile config(R"(
simulation:
  mode: inert
)");
    const ProviderConfig parsed = load_config(config.path().string());
    EXPECT_EQ(parsed.startup_policy, StartupPolicy::Strict);

    bool checked_policy = false;
    for (const auto &member : provider_schema().root().spec().members) {
        if (member.key == "startup_policy" && member.field.has_value()) {
            ASSERT_TRUE(member.field->spec().default_string.has_value());
            EXPECT_EQ(parse_startup_policy(*member.field->spec().default_string), parsed.startup_policy);
            checked_policy = true;
        }
    }
    EXPECT_TRUE(checked_policy);
}

}  // namespace anolis_provider_sim
