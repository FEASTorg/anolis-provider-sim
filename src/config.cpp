#include "config.hpp"
#include <filesystem>
#include <iostream>
#include <regex>
#include <set>
#include <stdexcept>

namespace anolis_provider_sim {

namespace fs = std::filesystem;
namespace {
constexpr const char *kReservedChaosControlId = "chaos_control";
}

// Parse simulation mode from string
SimulationMode parse_simulation_mode(const std::string &mode_str) {
  if (mode_str == "non_interacting") {
    return SimulationMode::NonInteracting;
  } else if (mode_str == "inert") {
    return SimulationMode::Inert;
  } else if (mode_str == "sim") {
    return SimulationMode::Sim;
  } else {
    throw std::runtime_error("Invalid simulation.mode: '" + mode_str +
                             "'. Valid values: non_interacting, inert, sim");
  }
}

StartupPolicy parse_startup_policy(const std::string &policy_str) {
  if (policy_str == "strict") {
    return StartupPolicy::Strict;
  }
  if (policy_str == "degraded") {
    return StartupPolicy::Degraded;
  }
  throw std::runtime_error("Invalid startup_policy: '" + policy_str +
                           "'. Valid values: strict, degraded");
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

  // Parse optional provider section
  if (yaml["provider"]) {
    if (!yaml["provider"].IsMap()) {
      throw std::runtime_error("[CONFIG] 'provider' section must be a map");
    }
    if (!yaml["provider"]["name"]) {
      throw std::runtime_error("[CONFIG] 'provider.name' is required when "
                               "'provider' section is present");
    }
    std::string provider_name;
    try {
      provider_name = yaml["provider"]["name"].as<std::string>();
    } catch (const YAML::Exception &) {
      throw std::runtime_error(
          "[CONFIG] Invalid provider.name: must be a string");
    }
    const std::regex valid_pattern("^[A-Za-z0-9_.-]{1,64}$");
    if (!std::regex_match(provider_name, valid_pattern)) {
      throw std::runtime_error("[CONFIG] Invalid provider.name: must match "
                               "^[A-Za-z0-9_.-]{1,64}$");
    }
    config.provider_name = provider_name;
  }

  // Parse optional startup policy (default: strict)
  if (yaml["startup_policy"]) {
    try {
      const std::string startup_policy_str =
          yaml["startup_policy"].as<std::string>();
      config.startup_policy = parse_startup_policy(startup_policy_str);
    } catch (const YAML::Exception &) {
      throw std::runtime_error(
          "[CONFIG] Invalid startup_policy: must be a string");
    } catch (const std::runtime_error &e) {
      throw std::runtime_error("[CONFIG] " + std::string(e.what()));
    }
  }

  // Parse devices section
  if (yaml["devices"]) {
    if (!yaml["devices"].IsSequence()) {
      throw std::runtime_error("'devices' must be a sequence");
    }

    std::set<std::string> seen_device_ids;
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

      if (spec.id.empty()) {
        throw std::runtime_error("[CONFIG] Invalid devices[" +
                                 std::to_string(i) +
                                 "]: 'id' must not be empty");
      }
      if (spec.type.empty()) {
        throw std::runtime_error("[CONFIG] Invalid devices[" +
                                 std::to_string(i) +
                                 "]: 'type' must not be empty");
      }
      if (spec.id == kReservedChaosControlId) {
        throw std::runtime_error(
            "[CONFIG] devices[].id 'chaos_control' is reserved and cannot be "
            "configured explicitly");
      }

      if (!seen_device_ids.insert(spec.id).second) {
        throw std::runtime_error("[CONFIG] Duplicate device id: '" + spec.id +
                                 "'");
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

  std::set<std::string> provided_simulation_keys;
  const std::set<std::string> known_simulation_keys = {
      "mode", "tick_rate_hz", "physics_config", "ambient_temp_c",
      "ambient_signal_path"};
  for (const auto &kv : yaml["simulation"]) {
    const std::string key = kv.first.as<std::string>();
    provided_simulation_keys.insert(key);

    if (key == "noise_enabled" || key == "update_rate_hz") {
      throw std::runtime_error("[CONFIG] simulation." + key +
                               " is no longer supported");
    }

    if (known_simulation_keys.find(key) == known_simulation_keys.end()) {
      throw std::runtime_error("[CONFIG] Unknown simulation key: '" + key +
                               "'");
    }
  }

  // Parse simulation.tick_rate_hz (required for non_interacting and sim)
  if (yaml["simulation"]["tick_rate_hz"]) {
    double tick_rate = 0.0;
    try {
      tick_rate = yaml["simulation"]["tick_rate_hz"].as<double>();
    } catch (const YAML::Exception &) {
      throw std::runtime_error("[CONFIG] simulation.tick_rate_hz must be "
                               "numeric in range [0.1, 1000.0]");
    }

    // Validate bounds
    if (tick_rate < 0.1 || tick_rate > 1000.0) {
      throw std::runtime_error(
          "[CONFIG] simulation.tick_rate_hz must be in range [0.1, 1000.0]");
    }

    config.tick_rate_hz = tick_rate;
  }

  // Parse simulation.physics_config (required for sim mode)
  if (yaml["simulation"]["physics_config"]) {
    try {
      config.physics_config_path =
          yaml["simulation"]["physics_config"].as<std::string>();
    } catch (const YAML::Exception &) {
      throw std::runtime_error(
          "[CONFIG] simulation.physics_config must be a string");
    }

    if (config.physics_config_path->empty()) {
      throw std::runtime_error(
          "[CONFIG] simulation.physics_config cannot be empty");
    }
  }

  if (yaml["simulation"]["ambient_temp_c"]) {
    try {
      config.ambient_temp_c = yaml["simulation"]["ambient_temp_c"].as<double>();
    } catch (const YAML::Exception &) {
      throw std::runtime_error(
          "[CONFIG] simulation.ambient_temp_c must be numeric");
    }
  }

  if (yaml["simulation"]["ambient_signal_path"]) {
    try {
      config.ambient_signal_path =
          yaml["simulation"]["ambient_signal_path"].as<std::string>();
    } catch (const YAML::Exception &) {
      throw std::runtime_error(
          "[CONFIG] simulation.ambient_signal_path must be a string");
    }

    if (config.ambient_signal_path->empty()) {
      throw std::runtime_error(
          "[CONFIG] simulation.ambient_signal_path cannot be empty");
    }
  }

  if (config.ambient_signal_path && !config.ambient_temp_c) {
    throw std::runtime_error("[CONFIG] simulation.ambient_signal_path requires "
                             "simulation.ambient_temp_c");
  }

  const auto ensure_mode_allowed_keys = [&](const std::set<std::string> &keys,
                                            const std::string &mode_name) {
    for (const auto &provided_key : provided_simulation_keys) {
      if (keys.find(provided_key) == keys.end()) {
        throw std::runtime_error("[CONFIG] simulation." + provided_key +
                                 " is not valid for mode=" + mode_name);
      }
    }
  };

  // Startup validation matrix
  switch (config.simulation_mode) {
  case SimulationMode::NonInteracting:
    ensure_mode_allowed_keys({"mode", "tick_rate_hz"}, "non_interacting");
    if (!config.tick_rate_hz) {
      throw std::runtime_error(
          "[CONFIG] mode=non_interacting requires simulation.tick_rate_hz");
    }
    break;

  case SimulationMode::Inert:
    ensure_mode_allowed_keys({"mode"}, "inert");
    break;

  case SimulationMode::Sim:
    ensure_mode_allowed_keys({"mode", "tick_rate_hz", "physics_config",
                              "ambient_temp_c", "ambient_signal_path"},
                             "sim");
    if (!config.tick_rate_hz) {
      throw std::runtime_error(
          "[CONFIG] mode=sim requires simulation.tick_rate_hz");
    }
    if (!config.physics_config_path) {
      throw std::runtime_error(
          "[CONFIG] mode=sim requires simulation.physics_config");
    }
    break;
  }

  // Validate physics_bindings are only used in sim mode
  if (config.simulation_mode != SimulationMode::Sim) {
    for (const auto &device : config.devices) {
      if (device.config.find("physics_bindings") != device.config.end()) {
        throw std::runtime_error("[CONFIG] Device '" + device.id +
                                 "' has physics_bindings but mode!= sim "
                                 "(prevents silent ignored config)");
      }
    }
  }

  return config;
}

} // namespace anolis_provider_sim
