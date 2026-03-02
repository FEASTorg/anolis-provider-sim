#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>

#include "config.hpp"

namespace anolis_provider_sim {

// Device registry entry
struct DeviceRegistryEntry {
  std::string id;   // Device identifier
  std::string type; // Device type (tempctl, motorctl, etc.)
  std::map<std::string, YAML::Node> config; // Device-specific config
};

struct DeviceInitFailure {
  std::string device_id;
  std::string type;
  std::string reason;
};

struct DeviceInitializationReport {
  std::size_t configured_device_count = 0;
  StartupPolicy startup_policy = StartupPolicy::Strict;
  std::vector<std::string> configured_device_ids;
  std::vector<std::string> successful_device_ids;
  std::vector<DeviceInitFailure> failed_devices;
};

// Device Factory - manages device creation and initialization from config
class DeviceFactory {
public:
  // Initialize a device from specification
  // Returns true on success, false if device type unknown or init failed
  static bool initialize_device(const DeviceSpec &spec);

  // Initialize all devices from provider config
  // Startup policy determines strict vs degraded behavior.
  static DeviceInitializationReport
  initialize_from_config(const ProviderConfig &config);

  // Last startup report (for health/diagnostics)
  static DeviceInitializationReport get_initialization_report();

  // Get list of registered device IDs and types
  static std::vector<DeviceRegistryEntry> get_registered_devices();

  // Check if a device ID is registered
  static bool is_device_registered(const std::string &device_id);

  // Get device type for a registered device ID
  static std::string get_device_type(const std::string &device_id);

  // Reset registry (for testing)
  static void reset();

  // Check if config has been loaded
  static bool is_config_loaded();
};

} // namespace anolis_provider_sim
