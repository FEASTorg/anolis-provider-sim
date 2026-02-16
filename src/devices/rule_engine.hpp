#pragma once

#include "../config.hpp"
#include <regex>
#include <string>

namespace sim_coordination {

// Forward declaration
class ISignalSource;

}

namespace sim_devices {

// Rule engine for evaluating automation rules in physics mode
// This lives in the provider executable (NOT the physics library)
// because it needs access to device functions and protocol types
class RuleEngine {
public:
  RuleEngine(sim_coordination::ISignalSource* signal_source,
             const anolis_provider_sim::PhysicsConfig& physics_config);

  // Evaluate all rules (called from physics ticker thread)
  void evaluate_rules();

private:
  // Parse and evaluate a condition string
  bool evaluate_condition(const std::string& condition);

  // Execute a rule action (device function call)
  void execute_action(const anolis_provider_sim::RuleAction& action);

  // Dependencies
  sim_coordination::ISignalSource* signal_source_;  // For reading signals
  anolis_provider_sim::PhysicsConfig physics_config_;  // Rule definitions

  // Condition regex (compiled once)
  std::regex condition_pattern_;
};

} // namespace sim_devices
