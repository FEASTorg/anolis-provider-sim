#include "device_factory.hpp"
#include "devices/analogsensor/analogsensor_device.hpp"
#include "devices/motorctl/motorctl_device.hpp"
#include "devices/relayio/relayio_device.hpp"
#include "devices/tempctl/tempctl_device.hpp"
#include "logging/logger.hpp"

#include <algorithm>
#include <optional>
#include <sstream>

namespace anolis_provider_sim {

// Static registry
static std::map<std::string, DeviceRegistryEntry> g_device_registry;
static bool g_config_loaded = false;

// Helper: Parse double from config map
static std::optional<double>
parse_double(const std::map<std::string, YAML::Node> &config,
             const std::string &key) {
  auto it = config.find(key);
  if (it == config.end())
    return std::nullopt;

  try {
    return it->second.as<double>();
  } catch (...) {
    throw std::runtime_error("[DeviceFactory] Failed to parse '" + key +
                             "' as double");
  }
}

// Helper: Parse [min, max] range from config map
static std::optional<std::pair<double, double>>
parse_range(const std::map<std::string, YAML::Node> &config,
            const std::string &key) {
  auto it = config.find(key);
  if (it == config.end())
    return std::nullopt;

  try {
    // Expected format: sequence with 2 elements
    if (!it->second.IsSequence() || it->second.size() != 2) {
      throw std::runtime_error("[DeviceFactory] Invalid range format for '" +
                               key + "' (expected 2-element sequence)");
    }

    double min_val = it->second[0].as<double>();
    double max_val = it->second[1].as<double>();

    if (min_val >= max_val) {
      throw std::runtime_error(
          "[DeviceFactory] Invalid range (min >= max) for '" + key + "'");
    }

    return std::make_pair(min_val, max_val);
  } catch (const std::exception &) {
    // Re-throw to fail-fast on invalid config
    throw;
  }
}

bool DeviceFactory::initialize_device(const DeviceSpec &spec) {
  if (spec.type == "tempctl") {
    // Parse device-specific config parameters
    sim_devices::tempctl::Config config;
    config.initial_temp = parse_double(spec.config, "initial_temp");
    config.temp_range = parse_range(spec.config, "temp_range");

    // Initialize tempctl with device-specific ID and config
    sim_devices::tempctl::init(spec.id, config);

    DeviceRegistryEntry entry;
    entry.id = spec.id;
    entry.type = spec.type;
    entry.config = spec.config;
    g_device_registry[spec.id] = entry;

    std::ostringstream msg;
    msg << "Initialized device '" << spec.id << "' (type: tempctl)";
    if (config.initial_temp.has_value()) {
      msg << " with initial_temp=" << config.initial_temp.value();
    }
    PSIM_LOG_INFO("DeviceFactory", msg.str());
    return true;
  } else if (spec.type == "motorctl") {
    // Parse device-specific config parameters
    sim_devices::motorctl::Config config;
    config.max_speed = parse_double(spec.config, "max_speed");

    // Initialize motorctl with device-specific ID and config
    sim_devices::motorctl::init(spec.id, config);

    DeviceRegistryEntry entry;
    entry.id = spec.id;
    entry.type = spec.type;
    entry.config = spec.config;
    g_device_registry[spec.id] = entry;

    std::ostringstream msg;
    msg << "Initialized device '" << spec.id << "' (type: motorctl)";
    if (config.max_speed.has_value()) {
      msg << " with max_speed=" << config.max_speed.value();
    }
    PSIM_LOG_INFO("DeviceFactory", msg.str());
    return true;
  } else if (spec.type == "relayio") {
    sim_devices::relayio::init(spec.id);

    DeviceRegistryEntry entry;
    entry.id = spec.id;
    entry.type = spec.type;
    entry.config = spec.config;
    g_device_registry[spec.id] = entry;

    PSIM_LOG_INFO("DeviceFactory",
                  "Initialized device '" << spec.id << "' (type: relayio)");
    return true;
  } else if (spec.type == "analogsensor") {
    sim_devices::analogsensor::init(spec.id);

    DeviceRegistryEntry entry;
    entry.id = spec.id;
    entry.type = spec.type;
    entry.config = spec.config;
    g_device_registry[spec.id] = entry;

    PSIM_LOG_INFO(
        "DeviceFactory",
        "Initialized device '" << spec.id << "' (type: analogsensor)");
    return true;
  } else {
    throw std::runtime_error("[DeviceFactory] Unknown device type: " +
                             spec.type);
  }
}

DeviceInitializationReport
DeviceFactory::initialize_from_config(const ProviderConfig &config) {
  g_device_registry.clear();
  g_config_loaded = false;
  DeviceInitializationReport init_report;
  init_report.configured_device_count = config.devices.size();
  init_report.startup_policy = config.startup_policy;
  init_report.configured_device_ids.reserve(config.devices.size());
  for (const auto &device : config.devices) {
    init_report.configured_device_ids.push_back(device.id);
  }

  for (const auto &spec : config.devices) {
    std::optional<std::string> failure_reason;
    try {
      if (initialize_device(spec)) {
        init_report.successful_device_ids.push_back(spec.id);
      } else {
        failure_reason = "[DeviceFactory] initialize_device returned false";
      }
    } catch (const std::exception &e) {
      failure_reason = e.what();
    }

    if (!failure_reason) {
      continue;
    }

    init_report.failed_devices.push_back(
        DeviceInitFailure{spec.id, spec.type, *failure_reason});
    PSIM_LOG_WARN("DeviceFactory",
                  "Failed to initialize device '" << spec.id
                                                   << "' (type: " << spec.type
                                                   << "): " << *failure_reason);

    if (config.startup_policy == StartupPolicy::Strict) {
      g_device_registry.clear();
      throw std::runtime_error(
          "[DeviceFactory] startup_policy=strict: failed to initialize '" +
          spec.id + "' (type: " + spec.type + "): " + *failure_reason);
    }
  }

  g_config_loaded = true;
  std::ostringstream summary;
  summary << "Initialized " << init_report.successful_device_ids.size() << " / "
          << config.devices.size() << " devices";
  if (!init_report.failed_devices.empty()) {
    summary << " (degraded: " << init_report.failed_devices.size()
            << " failed)";
  }
  PSIM_LOG_INFO("DeviceFactory", summary.str());

  return init_report;
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
