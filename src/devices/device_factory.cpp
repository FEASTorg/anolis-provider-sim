#include "device_factory.hpp"
#include "analogsensor_device.hpp"
#include "motorctl_device.hpp"
#include "relayio_device.hpp"
#include "tempctl_device.hpp"

#include <iostream>

namespace anolis_provider_sim {

// Static registry
static std::map<std::string, DeviceRegistryEntry> g_device_registry;
static bool g_config_loaded = false;

bool DeviceFactory::initialize_device(const DeviceSpec &spec) {
  try {
    if (spec.type == "tempctl") {
      // Initialize tempctl with device-specific ID
      sim_devices::tempctl::init(spec.id);

      // TODO: In future, apply device-specific config parameters
      // For now, we use default initialization

      DeviceRegistryEntry entry;
      entry.id = spec.id;
      entry.type = spec.type;
      entry.config = spec.config;
      g_device_registry[spec.id] = entry;

      std::cerr << "[DeviceFactory] Initialized device '" << spec.id
                << "' (type: tempctl)" << std::endl;
      return true;
    } else if (spec.type == "motorctl") {
      sim_devices::motorctl::init(spec.id);

      DeviceRegistryEntry entry;
      entry.id = spec.id;
      entry.type = spec.type;
      entry.config = spec.config;
      g_device_registry[spec.id] = entry;

      std::cerr << "[DeviceFactory] Initialized device '" << spec.id
                << "' (type: motorctl)" << std::endl;
      return true;
    } else if (spec.type == "relayio") {
      sim_devices::relayio::init(spec.id);

      DeviceRegistryEntry entry;
      entry.id = spec.id;
      entry.type = spec.type;
      entry.config = spec.config;
      g_device_registry[spec.id] = entry;

      std::cerr << "[DeviceFactory] Initialized device '" << spec.id
                << "' (type: relayio)" << std::endl;
      return true;
    } else if (spec.type == "analogsensor") {
      sim_devices::analogsensor::init(spec.id);

      DeviceRegistryEntry entry;
      entry.id = spec.id;
      entry.type = spec.type;
      entry.config = spec.config;
      g_device_registry[spec.id] = entry;

      std::cerr << "[DeviceFactory] Initialized device '" << spec.id
                << "' (type: analogsensor)" << std::endl;
      return true;
    } else {
      std::cerr << "[DeviceFactory] Unknown device type: " << spec.type
                << std::endl;
      return false;
    }
  } catch (const std::exception &e) {
    std::cerr << "[DeviceFactory] Failed to initialize device '" << spec.id
              << "': " << e.what() << std::endl;
    return false;
  }
}

int DeviceFactory::initialize_from_config(const ProviderConfig &config) {
  g_device_registry.clear();
  int success_count = 0;

  for (const auto &spec : config.devices) {
    if (initialize_device(spec)) {
      success_count++;
    }
  }

  g_config_loaded = true;
  std::cerr << "[DeviceFactory] Initialized " << success_count << " / "
            << config.devices.size() << " devices" << std::endl;

  return success_count;
}

std::vector<DeviceRegistryEntry> DeviceFactory::get_registered_devices() {
  std::vector<DeviceRegistryEntry> result;
  for (const auto &kv : g_device_registry) {
    result.push_back(kv.second);
  }
  return result;
}

bool DeviceFactory::is_device_registered(const std::string &device_id) {
  return g_device_registry.find(device_id) != g_device_registry.end();
}

std::string DeviceFactory::get_device_type(const std::string &device_id) {
  auto it = g_device_registry.find(device_id);
  if (it != g_device_registry.end()) {
    return it->second.type;
  }
  return "";
}

void DeviceFactory::reset() {
  g_device_registry.clear();
  g_config_loaded = false;
}

bool DeviceFactory::is_config_loaded() { return g_config_loaded; }

} // namespace anolis_provider_sim
