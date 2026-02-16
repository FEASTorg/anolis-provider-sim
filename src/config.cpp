#include "config.hpp"
#include <filesystem>
#include <iostream>
#include <regex>
#include <stdexcept>

namespace anolis_provider_sim {

namespace fs = std::filesystem;

// Parse simulation mode from string
SimulationMode parse_simulation_mode(const std::string &mode_str) {
  if (mode_str == "non_interacting") {
    return SimulationMode::NonInteracting;
  } else if (mode_str == "inert") {
    return SimulationMode::Inert;
  } else if (mode_str == "physics") {
    return SimulationMode::Physics;
  } else {
    throw std::runtime_error(
        "Invalid simulation.mode: '" + mode_str +
        "'. Valid values: non_interacting, inert, physics");
  }
}

// Parse transform type from string
static TransformType parse_transform_type(const std::string &type_str) {
  if (type_str == "first_order_lag") {
    return TransformType::FirstOrderLag;
  } else if (type_str == "noise") {
    return TransformType::Noise;
  } else if (type_str == "saturation") {
    return TransformType::Saturation;
  } else if (type_str == "linear") {
    return TransformType::Linear;
  } else if (type_str == "deadband") {
    return TransformType::Deadband;
  } else if (type_str == "rate_limiter") {
    return TransformType::RateLimiter;
  } else if (type_str == "delay") {
    return TransformType::Delay;
  } else if (type_str == "moving_average") {
    return TransformType::MovingAverage;
  } else {
    throw std::runtime_error("Invalid transform type: '" + type_str + "'");
  }
}

// Validate transform parameters based on type
static void
validate_transform_params(TransformType type,
                          const std::map<std::string, YAML::Node> &params) {
  switch (type) {
  case TransformType::FirstOrderLag: {
    if (params.find("tau_s") == params.end()) {
      throw std::runtime_error(
          "FirstOrderLag: missing required parameter 'tau_s'");
    }
    double tau_s = params.at("tau_s").as<double>();
    if (tau_s <= 0.0) {
      throw std::runtime_error("FirstOrderLag: tau_s must be > 0.0");
    }
    break;
  }
  case TransformType::Noise: {
    if (params.find("amplitude") == params.end()) {
      throw std::runtime_error("Noise: missing required parameter 'amplitude'");
    }
    if (params.find("seed") == params.end()) {
      throw std::runtime_error(
          "Noise: missing required parameter 'seed' (determinism requirement)");
    }
    double amplitude = params.at("amplitude").as<double>();
    if (amplitude <= 0.0) {
      throw std::runtime_error("Noise: amplitude must be > 0.0");
    }
    // Validate seed is an integer
    try {
      params.at("seed").as<int>();
    } catch (...) {
      throw std::runtime_error("Noise: seed must be an integer");
    }
    break;
  }
  case TransformType::Saturation: {
    if (params.find("min") == params.end()) {
      throw std::runtime_error("Saturation: missing required parameter 'min'");
    }
    if (params.find("max") == params.end()) {
      throw std::runtime_error("Saturation: missing required parameter 'max'");
    }
    double min = params.at("min").as<double>();
    double max = params.at("max").as<double>();
    if (min > max) {
      throw std::runtime_error("Saturation: min must be <= max");
    }
    break;
  }
  case TransformType::Linear: {
    if (params.find("scale") == params.end()) {
      throw std::runtime_error("Linear: missing required parameter 'scale'");
    }
    // Check clamp bounds if both present
    bool has_clamp_min = params.find("clamp_min") != params.end();
    bool has_clamp_max = params.find("clamp_max") != params.end();
    if (has_clamp_min && has_clamp_max) {
      double clamp_min = params.at("clamp_min").as<double>();
      double clamp_max = params.at("clamp_max").as<double>();
      if (clamp_min > clamp_max) {
        throw std::runtime_error("Linear: clamp_min must be <= clamp_max");
      }
    }
    break;
  }
  case TransformType::Deadband: {
    if (params.find("threshold") == params.end()) {
      throw std::runtime_error(
          "Deadband: missing required parameter 'threshold'");
    }
    double threshold = params.at("threshold").as<double>();
    if (threshold < 0.0) {
      throw std::runtime_error("Deadband: threshold must be >= 0.0");
    }
    break;
  }
  case TransformType::RateLimiter: {
    if (params.find("max_rate_per_sec") == params.end()) {
      throw std::runtime_error(
          "RateLimiter: missing required parameter 'max_rate_per_sec'");
    }
    double max_rate = params.at("max_rate_per_sec").as<double>();
    if (max_rate <= 0.0) {
      throw std::runtime_error("RateLimiter: max_rate_per_sec must be > 0.0");
    }
    break;
  }
  case TransformType::Delay: {
    if (params.find("delay_sec") == params.end()) {
      throw std::runtime_error("Delay: missing required parameter 'delay_sec'");
    }
    double delay_sec = params.at("delay_sec").as<double>();
    if (delay_sec < 0.0) {
      throw std::runtime_error("Delay: delay_sec must be >= 0.0");
    }
    // buffer_size is optional, but if present must be positive integer
    if (params.find("buffer_size") != params.end()) {
      try {
        int buffer_size = params.at("buffer_size").as<int>();
        if (buffer_size <= 0) {
          throw std::runtime_error("Delay: buffer_size must be > 0");
        }
      } catch (const YAML::Exception &) {
        throw std::runtime_error("Delay: buffer_size must be an integer");
      }
    }
    break;
  }
  case TransformType::MovingAverage: {
    if (params.find("window_size") == params.end()) {
      throw std::runtime_error(
          "MovingAverage: missing required parameter 'window_size'");
    }
    try {
      int window_size = params.at("window_size").as<int>();
      if (window_size <= 0) {
        throw std::runtime_error("MovingAverage: window_size must be > 0");
      }
    } catch (const YAML::Exception &) {
      throw std::runtime_error("MovingAverage: window_size must be an integer");
    }
    break;
  }
  }
}

// Validate rule condition grammar (simple comparison only)
static void validate_rule_condition(const std::string &condition) {
  // Grammar: signal_ref comparator numeric_literal
  // signal_ref: (device_id|model_id) "/" signal_id
  // comparator: < | > | <= | >= | == | !=
  // numeric_literal: floating point number

  std::regex pattern(
      R"(^\s*([a-zA-Z_][a-zA-Z0-9_]*)/([a-zA-Z_][a-zA-Z0-9_]*)\s*(<|>|<=|>=|==|!=)\s*(-?\d+(?:\.\d+)?(?:[eE][+-]?\d+)?)\s*$)");

  if (!std::regex_match(condition, pattern)) {
    throw std::runtime_error(
        "Invalid rule condition: '" + condition +
        "'. Expected format: 'device_id/signal_id comparator numeric_literal'");
  }
}

ProviderConfig load_config(const std::string &path) {
  YAML::Node yaml;

  try {
    yaml = YAML::LoadFile(path);
  } catch (const YAML::Exception &e) {
    throw std::runtime_error("Failed to load config file '" + path +
                             "': " + e.what());
  }

  ProviderConfig config;
  config.config_file_path = fs::absolute(path).string();

  // Parse devices section
  if (yaml["devices"]) {
    if (!yaml["devices"].IsSequence()) {
      throw std::runtime_error("'devices' must be a sequence");
    }

    for (std::size_t i = 0; i < yaml["devices"].size(); ++i) {
      const auto &device_node = yaml["devices"][i];
      if (!device_node.IsMap()) {
        throw std::runtime_error("[CONFIG] Invalid devices[" +
                                 std::to_string(i) + "]: entry must be a map");
      }

      DeviceSpec spec;

      // Extract required fields: id and type
      if (!device_node["id"]) {
        throw std::runtime_error("[CONFIG] Invalid devices[" +
                                 std::to_string(i) +
                                 "]: missing required field 'id'");
      }
      if (!device_node["type"]) {
        throw std::runtime_error("[CONFIG] Invalid devices[" +
                                 std::to_string(i) +
                                 "]: missing required field 'type'");
      }

      try {
        spec.id = device_node["id"].as<std::string>();
        spec.type = device_node["type"].as<std::string>();
      } catch (const YAML::Exception &e) {
        throw std::runtime_error("[CONFIG] Invalid devices[" +
                                 std::to_string(i) + "]: " + e.what());
      }

      // Store all other fields as configuration parameters
      for (const auto &kv : device_node) {
        std::string key = kv.first.as<std::string>();
        if (key != "id" && key != "type") {
          spec.config[key] = kv.second;
        }
      }

      config.devices.push_back(spec);
    }
  }

  // Parse simulation section - REQUIRED
  if (!yaml["simulation"]) {
    throw std::runtime_error("[CONFIG] Missing required 'simulation' section");
  }

  if (!yaml["simulation"].IsMap()) {
    throw std::runtime_error("[CONFIG] 'simulation' section must be a map");
  }

  // Parse simulation.mode - REQUIRED
  if (!yaml["simulation"]["mode"]) {
    throw std::runtime_error("[CONFIG] Missing required 'simulation.mode'");
  }

  try {
    std::string mode_str = yaml["simulation"]["mode"].as<std::string>();
    config.simulation_mode = parse_simulation_mode(mode_str);
  } catch (const std::exception &e) {
    throw std::runtime_error("[CONFIG] Invalid simulation.mode: " +
                             std::string(e.what()));
  }

  // Parse simulation.tick_rate_hz (required for non_interacting and physics)
  if (yaml["simulation"]["tick_rate_hz"]) {
    double tick_rate = yaml["simulation"]["tick_rate_hz"].as<double>();

    // Validate bounds
    if (tick_rate < 0.1 || tick_rate > 1000.0) {
      throw std::runtime_error(
          "[CONFIG] simulation.tick_rate_hz must be in range [0.1, 1000.0]");
    }

    config.tick_rate_hz = tick_rate;
  }

  // Parse simulation.physics_config (required for physics mode)
  if (yaml["simulation"]["physics_config"]) {
    config.physics_config_path =
        yaml["simulation"]["physics_config"].as<std::string>();
  }

  // Startup validation matrix
  switch (config.simulation_mode) {
  case SimulationMode::NonInteracting:
    if (!config.tick_rate_hz) {
      throw std::runtime_error(
          "[CONFIG] mode=non_interacting requires simulation.tick_rate_hz");
    }
    if (config.physics_config_path) {
      throw std::runtime_error(
          "[CONFIG] mode=non_interacting cannot have physics_config (prevents "
          "silent ignored config)");
    }
    break;

  case SimulationMode::Inert:
    if (config.tick_rate_hz) {
      throw std::runtime_error(
          "[CONFIG] mode=inert cannot have simulation.tick_rate_hz (prevents "
          "ignored config)");
    }
    if (config.physics_config_path) {
      throw std::runtime_error("[CONFIG] mode=inert cannot have physics_config "
                               "(prevents silent ignored config)");
    }
    break;

  case SimulationMode::Physics:
    if (!config.tick_rate_hz) {
      throw std::runtime_error(
          "[CONFIG] mode=physics requires simulation.tick_rate_hz");
    }
    if (!config.physics_config_path) {
      throw std::runtime_error(
          "[CONFIG] mode=physics requires simulation.physics_config");
    }
    break;
  }

  // Validate physics_bindings are only used in physics mode
  if (config.simulation_mode != SimulationMode::Physics) {
    for (const auto &device : config.devices) {
      if (device.config.find("physics_bindings") != device.config.end()) {
        throw std::runtime_error("[CONFIG] Device '" + device.id +
                                 "' has physics_bindings but mode!= physics "
                                 "(prevents silent ignored config)");
      }
    }
  }

  // Store legacy simulation params for backward compatibility
  for (const auto &kv : yaml["simulation"]) {
    std::string key = kv.first.as<std::string>();
    if (key != "mode" && key != "tick_rate_hz" && key != "physics_config") {
      config.simulation[key] = kv.second;
    }
  }

  return config;
}

PhysicsConfig load_physics_config(const std::string &path) {
  YAML::Node yaml;

  try {
    yaml = YAML::LoadFile(path);
  } catch (const YAML::Exception &e) {
    throw std::runtime_error("Failed to load physics config file '" + path +
                             "': " + e.what());
  }

  if (!yaml["physics"]) {
    throw std::runtime_error(
        "[PHYSICS CONFIG] Missing required 'physics' section");
  }

  if (!yaml["physics"].IsMap()) {
    throw std::runtime_error(
        "[PHYSICS CONFIG] 'physics' section must be a map");
  }

  PhysicsConfig config;

  // Parse signal_graph
  if (yaml["physics"]["signal_graph"]) {
    if (!yaml["physics"]["signal_graph"].IsSequence()) {
      throw std::runtime_error(
          "[PHYSICS CONFIG] 'signal_graph' must be a sequence");
    }

    std::set<std::pair<std::string, std::string>>
        edge_set; // For duplicate detection

    for (std::size_t i = 0; i < yaml["physics"]["signal_graph"].size(); ++i) {
      const auto &edge_node = yaml["physics"]["signal_graph"][i];

      if (!edge_node["source"]) {
        throw std::runtime_error("[PHYSICS CONFIG] signal_graph[" +
                                 std::to_string(i) +
                                 "]: missing required field 'source'");
      }
      if (!edge_node["target"]) {
        throw std::runtime_error("[PHYSICS CONFIG] signal_graph[" +
                                 std::to_string(i) +
                                 "]: missing required field 'target'");
      }

      GraphEdge edge;
      edge.source = edge_node["source"].as<std::string>();
      edge.target = edge_node["target"].as<std::string>();

      // Check for duplicate edges
      auto edge_pair = std::make_pair(edge.source, edge.target);
      if (edge_set.find(edge_pair) != edge_set.end()) {
        throw std::runtime_error("[PHYSICS CONFIG] Duplicate edge: " +
                                 edge.source + " -> " + edge.target);
      }
      edge_set.insert(edge_pair);

      // Parse optional transform
      if (edge_node["transform"]) {
        if (!edge_node["transform"].IsMap()) {
          throw std::runtime_error("[PHYSICS CONFIG] signal_graph[" +
                                   std::to_string(i) +
                                   "]: 'transform' must be a map");
        }

        if (!edge_node["transform"]["type"]) {
          throw std::runtime_error(
              "[PHYSICS CONFIG] signal_graph[" + std::to_string(i) +
              "]: transform missing required field 'type'");
        }

        std::string type_str = edge_node["transform"]["type"].as<std::string>();
        edge.transform_type = parse_transform_type(type_str);

        // Extract transform parameters
        for (const auto &kv : edge_node["transform"]) {
          std::string key = kv.first.as<std::string>();
          if (key != "type") {
            edge.transform_params[key] = kv.second;
          }
        }

        // Validate transform parameters
        validate_transform_params(*edge.transform_type, edge.transform_params);
      }

      config.signal_graph.push_back(edge);
    }
  }

  // Parse models
  if (yaml["physics"]["models"]) {
    if (!yaml["physics"]["models"].IsSequence()) {
      throw std::runtime_error("[PHYSICS CONFIG] 'models' must be a sequence");
    }

    std::set<std::string> model_ids; // For duplicate detection

    for (std::size_t i = 0; i < yaml["physics"]["models"].size(); ++i) {
      const auto &model_node = yaml["physics"]["models"][i];

      if (!model_node["id"]) {
        throw std::runtime_error("[PHYSICS CONFIG] models[" +
                                 std::to_string(i) +
                                 "]: missing required field 'id'");
      }
      if (!model_node["type"]) {
        throw std::runtime_error("[PHYSICS CONFIG] models[" +
                                 std::to_string(i) +
                                 "]: missing required field 'type'");
      }

      PhysicsModelSpec spec;
      spec.id = model_node["id"].as<std::string>();
      spec.type = model_node["type"].as<std::string>();

      // Check for duplicate model IDs
      if (model_ids.find(spec.id) != model_ids.end()) {
        throw std::runtime_error("[PHYSICS CONFIG] Duplicate model ID: " +
                                 spec.id);
      }
      model_ids.insert(spec.id);

      // Extract model parameters
      if (model_node["params"]) {
        if (!model_node["params"].IsMap()) {
          throw std::runtime_error("[PHYSICS CONFIG] models[" +
                                   std::to_string(i) +
                                   "]: 'params' must be a map");
        }
        for (const auto &kv : model_node["params"]) {
          spec.params[kv.first.as<std::string>()] = kv.second;
        }
      }

      config.models.push_back(spec);
    }
  }

  // Parse rules
  if (yaml["physics"]["rules"]) {
    if (!yaml["physics"]["rules"].IsSequence()) {
      throw std::runtime_error("[PHYSICS CONFIG] 'rules' must be a sequence");
    }

    std::set<std::string> rule_ids; // For duplicate detection

    for (std::size_t i = 0; i < yaml["physics"]["rules"].size(); ++i) {
      const auto &rule_node = yaml["physics"]["rules"][i];

      if (!rule_node["id"]) {
        throw std::runtime_error("[PHYSICS CONFIG] rules[" + std::to_string(i) +
                                 "]: missing required field 'id'");
      }
      if (!rule_node["condition"]) {
        throw std::runtime_error("[PHYSICS CONFIG] rules[" + std::to_string(i) +
                                 "]: missing required field 'condition'");
      }
      if (!rule_node["actions"]) {
        throw std::runtime_error("[PHYSICS CONFIG] rules[" + std::to_string(i) +
                                 "]: missing required field 'actions'");
      }

      RuleSpec spec;
      spec.id = rule_node["id"].as<std::string>();
      spec.condition = rule_node["condition"].as<std::string>();

      // Check for duplicate rule IDs
      if (rule_ids.find(spec.id) != rule_ids.end()) {
        throw std::runtime_error("[PHYSICS CONFIG] Duplicate rule ID: " +
                                 spec.id);
      }
      rule_ids.insert(spec.id);

      // Validate condition grammar
      validate_rule_condition(spec.condition);

      // Parse on_error (default: log_and_continue)
      if (rule_node["on_error"]) {
        spec.on_error = rule_node["on_error"].as<std::string>();
        if (spec.on_error != "log_and_continue") {
          throw std::runtime_error(
              "[PHYSICS CONFIG] rules[" + std::to_string(i) +
              "]: Phase 22 only supports on_error='log_and_continue'");
        }
      } else {
        spec.on_error = "log_and_continue";
      }

      // Parse actions
      if (!rule_node["actions"].IsSequence()) {
        throw std::runtime_error("[PHYSICS CONFIG] rules[" + std::to_string(i) +
                                 "]: 'actions' must be a sequence");
      }

      for (std::size_t j = 0; j < rule_node["actions"].size(); ++j) {
        const auto &action_node = rule_node["actions"][j];

        if (!action_node["device"]) {
          throw std::runtime_error(
              "[PHYSICS CONFIG] rules[" + std::to_string(i) + "].actions[" +
              std::to_string(j) + "]: missing required field 'device'");
        }
        if (!action_node["function"]) {
          throw std::runtime_error(
              "[PHYSICS CONFIG] rules[" + std::to_string(i) + "].actions[" +
              std::to_string(j) + "]: missing required field 'function'");
        }

        RuleAction action;
        action.device_id = action_node["device"].as<std::string>();
        action.function_name = action_node["function"].as<std::string>();

        // Parse args (optional)
        if (action_node["args"]) {
          if (!action_node["args"].IsMap()) {
            throw std::runtime_error(
                "[PHYSICS CONFIG] rules[" + std::to_string(i) + "].actions[" +
                std::to_string(j) + "]: 'args' must be a map");
          }
          for (const auto &kv : action_node["args"]) {
            action.args[kv.first.as<std::string>()] = kv.second;
          }
        }

        spec.actions.push_back(action);
      }

      config.rules.push_back(spec);
    }
  }

  return config;
}

} // namespace anolis_provider_sim
