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
  Physics         // Fixed-tick, full physics engine with signal routing
};

// Transform primitive type
enum class TransformType {
  FirstOrderLag,
  Noise,
  Saturation,
  Linear,
  Deadband,
  RateLimiter,
  Delay,
  MovingAverage
};

// Graph edge with optional transform
struct GraphEdge {
  std::string source; // device_id/signal_id or model_id/signal_id
  std::string target; // model_id/signal_id (Phase 22 v1)
  std::optional<TransformType> transform_type;
  std::map<std::string, YAML::Node> transform_params;
};

// Physics model specification
struct PhysicsModelSpec {
  std::string id;
  std::string type;
  std::map<std::string, YAML::Node> params;
};

// Rule action
struct RuleAction {
  std::string device_id;
  std::string function_name;
  std::map<std::string, YAML::Node> args;
};

// Rule specification
struct RuleSpec {
  std::string id;
  std::string condition; // Simple comparison format
  std::vector<RuleAction> actions;
  std::string on_error; // Phase 22: "log_and_continue" only
};

// Physics configuration
struct PhysicsConfig {
  std::vector<GraphEdge> signal_graph;
  std::vector<PhysicsModelSpec> models;
  std::vector<RuleSpec> rules;
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
  std::vector<DeviceSpec> devices;
  SimulationMode simulation_mode;
  std::optional<double>
      tick_rate_hz; // Required for non_interacting and physics modes
  std::optional<std::string> physics_config_path; // Required for physics mode
  std::map<std::string, YAML::Node>
      simulation; // Legacy params (will be migrated)
};

// Load provider configuration from YAML file
// Throws std::runtime_error if file cannot be read, parsed, or validated
ProviderConfig load_config(const std::string &path);

// Load physics configuration from YAML file
// Throws std::runtime_error if file cannot be read, parsed, or validated
PhysicsConfig load_physics_config(const std::string &path);

// Parse simulation mode from string
// Throws std::runtime_error if mode is invalid
SimulationMode parse_simulation_mode(const std::string &mode_str);

} // namespace anolis_provider_sim
