#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>

namespace anolis_provider_sim {

// Simulation mode
enum class SimulationMode {
  NonInteracting, // Fixed-tick, device-only physics, no cross-device flow
  Inert,          // No automatic updates, function calls only
  Sim             // Fixed-tick, external simulation engine with signal routing
};

// Startup policy controls behavior when one or more configured devices fail to
// initialize.
enum class StartupPolicy {
  Strict,   // Abort startup on first init failure
  Degraded, // Continue with successfully initialized devices
};

// Specification for a single device from config
struct DeviceSpec {
  std::string id;   // Device identifier (used by behavior tree)
  std::string type; // Device type (tempctl, motorctl, relayio, analogsensor)
  std::map<std::string, YAML::Node> config; // Type-specific parameters
};

// Complete provider configuration
struct ProviderConfig {
  std::string config_file_path; // Path to config file (for relative resolution)
  std::optional<std::string> provider_name; // Optional provider identity
  StartupPolicy startup_policy = StartupPolicy::Strict;
  std::vector<DeviceSpec> devices;
  SimulationMode simulation_mode;
  std::optional<double>
      tick_rate_hz; // Required for non_interacting and sim modes
  std::optional<std::string> physics_config_path; // Required for sim mode
  std::optional<double> ambient_temp_c;           // Optional for sim mode
  std::optional<std::string> ambient_signal_path; // Optional for sim mode
};

// Load provider configuration from YAML file
// Throws std::runtime_error if file cannot be read, parsed, or validated
ProviderConfig load_config(const std::string &path);

// Parse simulation mode from string
// Throws std::runtime_error if mode is invalid
SimulationMode parse_simulation_mode(const std::string &mode_str);

// Parse startup policy from string
// Throws std::runtime_error if policy is invalid
StartupPolicy parse_startup_policy(const std::string &policy_str);

} // namespace anolis_provider_sim
