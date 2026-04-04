#pragma once

/**
 * @file config.hpp
 * @brief Provider configuration types and parsing entry points for anolis-provider-sim.
 */

#include <map>
#include <optional>
#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>

namespace anolis_provider_sim {

/**
 * @brief Runtime simulation mode for the provider process.
 */
enum class SimulationMode {
  NonInteracting, // Fixed-tick, device-only physics, no cross-device flow
  Inert,          // No automatic updates, function calls only
  Sim             // Fixed-tick, external simulation engine with signal routing
};

/**
 * @brief Startup behavior when one or more configured devices fail to initialize.
 */
enum class StartupPolicy {
  Strict,   // Abort startup on first init failure
  Degraded, // Continue with successfully initialized devices
};

/**
 * @brief One configured device entry from the provider YAML.
 *
 * `config` holds the device-type-specific subtree exactly as parsed so the
 * corresponding device implementation can validate and consume it.
 */
struct DeviceSpec {
  std::string id;
  std::string type;
  std::map<std::string, YAML::Node> config;
};

/**
 * @brief Fully resolved provider configuration after YAML parsing.
 *
 * The optional simulation fields are mode-dependent:
 * `tick_rate_hz` is required in `non_interacting` and `sim` modes, while
 * `physics_config_path` is required in `sim` mode only.
 */
struct ProviderConfig {
  std::string config_file_path;
  std::optional<std::string> provider_name;
  StartupPolicy startup_policy = StartupPolicy::Strict;
  std::vector<DeviceSpec> devices;
  SimulationMode simulation_mode;
  std::optional<double> tick_rate_hz;
  std::optional<std::string> physics_config_path;
  std::optional<double> ambient_temp_c;
  std::optional<std::string> ambient_signal_path;
};

/**
 * @brief Load and validate provider configuration from a YAML file.
 *
 * @return Resolved provider configuration
 * @throws std::runtime_error if the file cannot be read, parsed, or validated
 */
ProviderConfig load_config(const std::string &path);

/**
 * @brief Parse a config string into a simulation mode.
 *
 * @throws std::runtime_error if the mode is invalid
 */
SimulationMode parse_simulation_mode(const std::string &mode_str);

/**
 * @brief Parse a config string into a startup policy.
 *
 * @throws std::runtime_error if the policy is invalid
 */
StartupPolicy parse_startup_policy(const std::string &policy_str);

} // namespace anolis_provider_sim
