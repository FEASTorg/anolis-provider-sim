#include "config.hpp"
#include <iostream>
#include <stdexcept>

namespace anolis_provider_sim {

ProviderConfig load_config(const std::string &path) {
  YAML::Node yaml;

  try {
    yaml = YAML::LoadFile(path);
  } catch (const YAML::Exception &e) {
    throw std::runtime_error("Failed to load config file '" + path +
                             "': " + e.what());
  }

  ProviderConfig config;

  // Parse devices section
  if (yaml["devices"]) {
    if (!yaml["devices"].IsSequence()) {
      throw std::runtime_error("'devices' must be a sequence");
    }

    for (const auto &device_node : yaml["devices"]) {
      if (!device_node.IsMap()) {
        std::cerr << "[CONFIG] Warning: skipping non-map device entry"
                  << std::endl;
        continue;
      }

      DeviceSpec spec;

      // Extract required fields: id and type
      if (!device_node["id"]) {
        std::cerr << "[CONFIG] Warning: skipping device without 'id' field"
                  << std::endl;
        continue;
      }
      if (!device_node["type"]) {
        std::cerr << "[CONFIG] Warning: skipping device without 'type' field"
                  << std::endl;
        continue;
      }

      spec.id = device_node["id"].as<std::string>();
      spec.type = device_node["type"].as<std::string>();

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

  // Parse simulation section (optional)
  if (yaml["simulation"]) {
    if (yaml["simulation"].IsMap()) {
      for (const auto &kv : yaml["simulation"]) {
        config.simulation[kv.first.as<std::string>()] = kv.second;
      }
    } else {
      std::cerr
          << "[CONFIG] Warning: 'simulation' section is not a map, ignoring"
          << std::endl;
    }
  }

  return config;
}

} // namespace anolis_provider_sim
