#include "device_factory.hpp"

#include <algorithm>
#include <optional>
#include <sstream>

#include "devices/common/device_adapter.hpp"
#include "logging/logger.hpp"

namespace anolis_provider_sim {

// Static registry
static std::map<std::string, DeviceRegistryEntry> g_device_registry;
static bool g_config_loaded = false;

bool DeviceFactory::initialize_device(const DeviceSpec &spec) {
    // Device-type dispatch is the single typed switch in device_adapter; each
    // adapter parses its own config keys and initializes its module state.
    const std::optional<sim_devices::SimDeviceType> type = sim_devices::sim_device_type_from_string(spec.type);
    if (!type.has_value()) {
        throw std::runtime_error("[DeviceFactory] Unknown device type: " + spec.type);
    }

    sim_devices::adapter_for(type.value()).init(spec.id, spec.config);

    DeviceRegistryEntry entry;
    entry.id = spec.id;
    entry.type = spec.type;
    entry.config = spec.config;
    g_device_registry[spec.id] = entry;

    PSIM_LOG_INFO("DeviceFactory", "Initialized device '" << spec.id << "' (type: " << spec.type << ")");
    return true;
}

DeviceInitializationReport DeviceFactory::initialize_from_config(const ProviderConfig &config) {
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

        init_report.failed_devices.push_back(DeviceInitFailure{spec.id, spec.type, *failure_reason});
        PSIM_LOG_WARN("DeviceFactory", "Failed to initialize device '" << spec.id << "' (type: " << spec.type
                                                                       << "): " << *failure_reason);

        if (config.startup_policy == StartupPolicy::Strict) {
            g_device_registry.clear();
            throw std::runtime_error("[DeviceFactory] startup_policy=strict: failed to initialize '" + spec.id +
                                     "' (type: " + spec.type + "): " + *failure_reason);
        }
    }

    g_config_loaded = true;
    std::ostringstream summary;
    summary << "Initialized " << init_report.successful_device_ids.size() << " / " << config.devices.size()
            << " devices";
    if (!init_report.failed_devices.empty()) {
        summary << " (degraded: " << init_report.failed_devices.size() << " failed)";
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

}  // namespace anolis_provider_sim
