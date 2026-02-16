#include "rule_engine.hpp"
#include "device_manager.hpp"
#include "signal_source.hpp"
#include <cmath>
#include <iostream>

namespace sim_devices {

RuleEngine::RuleEngine(
    sim_coordination::ISignalSource* signal_source,
    const anolis_provider_sim::PhysicsConfig& physics_config)
    : signal_source_(signal_source),
      physics_config_(physics_config),
      condition_pattern_(R"((\w+)/(\w+)\s*([<>]=?|==|!=)\s*([-+]?\d*\.?\d+))") {
}

void RuleEngine::evaluate_rules() {
  for (const auto& rule : physics_config_.rules) {
    try {
      // Evaluate condition
      bool condition_met = evaluate_condition(rule.condition);

      if (condition_met) {
        // Execute all actions in order
        for (const auto& action : rule.actions) {
          execute_action(action);
        }
      }
    } catch (const std::exception& e) {
      std::cerr << "[RuleEngine] Rule '" << rule.id << "' failed: "
                << e.what() << std::endl;
      // Continue to next rule (log_and_continue policy)
    }
  }
}

bool RuleEngine::evaluate_condition(const std::string& condition) {
  // Parse condition: "object_id/signal_id comparator number"
  // Example: "chamber_thermal/temperature > 85.0"
  std::smatch match;

  if (!std::regex_match(condition, match, condition_pattern_)) {
    throw std::runtime_error("Invalid condition syntax: " + condition);
  }

  std::string object_id = match[1];
  std::string signal_id = match[2];
  std::string comparator = match[3];
  double threshold = std::stod(match[4]);

  // Read signal value from registry (model or device)
  std::string path = object_id + "/" + signal_id;
  std::optional<double> value = signal_source_->read_signal(path);

  if (!value) {
    // Signal not available, condition is false
    return false;
  }

  // Compare
  if (comparator == "<")
    return *value < threshold;
  if (comparator == ">")
    return *value > threshold;
  if (comparator == "<=")
    return *value <= threshold;
  if (comparator == ">=")
    return *value >= threshold;
  if (comparator == "==")
    return std::abs(*value - threshold) < 1e-6;
  if (comparator == "!=")
    return std::abs(*value - threshold) >= 1e-6;

  throw std::runtime_error("Unknown comparator: " + comparator);
}

void RuleEngine::execute_action(const anolis_provider_sim::RuleAction& action) {
  // Convert YAML args to protobuf Values
  std::map<std::string, anolis::deviceprovider::v1::Value> args;

  for (const auto& [key, yaml_node] : action.args) {
    anolis::deviceprovider::v1::Value val;

    // Convert YAML node to Value based on type
    if (yaml_node.IsScalar()) {
      std::string scalar_val = yaml_node.Scalar();
      
      // Try bool first (check for literal true/false)
      if (scalar_val == "true" || scalar_val == "false" ||
          scalar_val == "True" || scalar_val == "False" ||
          scalar_val == "TRUE" || scalar_val == "FALSE") {
        bool b = yaml_node.as<bool>();
        val.set_type(anolis::deviceprovider::v1::ValueType::VALUE_TYPE_BOOL);
        val.set_bool_value(b);
      }
      // Try int64 before double (check if no decimal point or exponent)
      else if (scalar_val.find('.') == std::string::npos && 
               scalar_val.find('e') == std::string::npos &&
               scalar_val.find('E') == std::string::npos) {
        try {
          int64_t i = yaml_node.as<int64_t>();
          val.set_type(anolis::deviceprovider::v1::ValueType::VALUE_TYPE_INT64);
          val.set_int64_value(i);
        } catch (...) {
          // Fall through to double
          double d = yaml_node.as<double>();
          val.set_type(anolis::deviceprovider::v1::ValueType::VALUE_TYPE_DOUBLE);
          val.set_double_value(d);
        }
      }
      // Try as double
      else {
        try {
          double d = yaml_node.as<double>();
          val.set_type(anolis::deviceprovider::v1::ValueType::VALUE_TYPE_DOUBLE);
          val.set_double_value(d);
        } catch (...) {
          // Default to string
          std::string s = yaml_node.as<std::string>();
          val.set_type(anolis::deviceprovider::v1::ValueType::VALUE_TYPE_STRING);
          val.set_string_value(s);
        }
      }
    } else {
      // Complex type, convert to string
      val.set_type(anolis::deviceprovider::v1::ValueType::VALUE_TYPE_STRING);
      val.set_string_value("[complex]");
    }

    args[key] = val;
  }

  // Look up function ID by name
  auto device_caps = describe_device(action.device_id);
  uint32_t function_id = 0;

  for (const auto& func : device_caps.functions()) {
    if (func.name() == action.function_name) {
      function_id = func.function_id();
      break;
    }
  }

  if (function_id == 0) {
    throw std::runtime_error("Unknown function '" + action.function_name +
                             "' on device '" + action.device_id + "'");
  }

  // Execute function call
  auto result = call_function(action.device_id, function_id, args);

  if (result.code != Status::CODE_OK) {
    // log_and_continue policy: continue execution
  }
}

} // namespace sim_devices
