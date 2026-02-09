#pragma once

#include <map>
#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>

namespace anolis_provider_sim {

// Specification for a single device from config
struct DeviceSpec {
  std::string id;   // Device identifier (used by behavior tree)
  std::string type; // Device type (tempctl, motorctl, relayio, analogsensor)
  std::map<std::string, YAML::Node> config; // Type-specific parameters
};

// Complete provider configuration
struct ProviderConfig {
  std::vector<DeviceSpec> devices;
  std::map<std::string, YAML::Node> simulation; // Optional simulation params
};

// Load configuration from YAML file
// Throws std::runtime_error if file cannot be read or parsed
ProviderConfig load_config(const std::string &path);

} // namespace anolis_provider_sim
