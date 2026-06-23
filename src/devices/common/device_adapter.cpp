#include "devices/common/device_adapter.hpp"

#include <stdexcept>
#include <utility>

#include "devices/analogsensor/analogsensor_device.hpp"
#include "devices/common/device_factory.hpp"
#include "devices/motorctl/motorctl_device.hpp"
#include "devices/relayio/relayio_device.hpp"
#include "devices/tempctl/tempctl_device.hpp"

namespace sim_devices {
namespace {

// --- per-type config parsing (each adapter parses only the keys it knows) ----

std::optional<double> cfg_double(const DeviceConfigMap &config, const std::string &key) {
    auto it = config.find(key);
    if (it == config.end()) {
        return std::nullopt;
    }
    try {
        return it->second.as<double>();
    } catch (...) {
        throw std::runtime_error("[DeviceAdapter] Failed to parse '" + key + "' as double");
    }
}

std::optional<std::pair<double, double>> cfg_range(const DeviceConfigMap &config, const std::string &key) {
    auto it = config.find(key);
    if (it == config.end()) {
        return std::nullopt;
    }
    if (!it->second.IsSequence() || it->second.size() != 2) {
        throw std::runtime_error("[DeviceAdapter] Invalid range format for '" + key +
                                 "' (expected 2-element sequence)");
    }
    const double min_val = it->second[0].as<double>();
    const double max_val = it->second[1].as<double>();
    if (min_val >= max_val) {
        throw std::runtime_error("[DeviceAdapter] Invalid range (min >= max) for '" + key + "'");
    }
    return std::make_pair(min_val, max_val);
}

void tempctl_init(const std::string &id, const DeviceConfigMap &config) {
    tempctl::Config c;
    c.initial_temp = cfg_double(config, "initial_temp");
    c.temp_range = cfg_range(config, "temp_range");
    tempctl::init(id, c);
}

void motorctl_init(const std::string &id, const DeviceConfigMap &config) {
    motorctl::Config c;
    c.max_speed = cfg_double(config, "max_speed");
    motorctl::init(id, c);
}

void relayio_init(const std::string &id, const DeviceConfigMap & /*config*/) { relayio::init(id); }

void analogsensor_init(const std::string &id, const DeviceConfigMap & /*config*/) { analogsensor::init(id); }

// Types without closed-loop control share this no-op so the descriptor slot is
// always callable.
void no_update_control(const std::string & /*id*/) {}

// Per-type actuator signal-id suffixes the engine feeds back as physics inputs
// (must match the signals each module publishes).
const char *const kTempctlActuators[] = {"relay1_state", "relay2_state"};
const char *const kMotorctlActuators[] = {"motor1_duty", "motor2_duty"};
const char *const kRelayioActuators[] = {"relay_ch1_state", "relay_ch2_state", "relay_ch3_state", "relay_ch4_state"};

const DeviceAdapter kTempctl{
    &tempctl::get_capabilities,  &tempctl_init,          &tempctl::update_physics, &tempctl::update_control,
    &tempctl::get_device_info,   &tempctl::read_signals, &tempctl::call_function,  kTempctlActuators,
    std::size(kTempctlActuators)};

const DeviceAdapter kMotorctl{
    &motorctl::get_capabilities,  &motorctl_init,          &motorctl::update_physics, &no_update_control,
    &motorctl::get_device_info,   &motorctl::read_signals, &motorctl::call_function,  kMotorctlActuators,
    std::size(kMotorctlActuators)};

const DeviceAdapter kRelayio{
    &relayio::get_capabilities,  &relayio_init,          &relayio::update_physics, &no_update_control,
    &relayio::get_device_info,   &relayio::read_signals, &relayio::call_function,  kRelayioActuators,
    std::size(kRelayioActuators)};

const DeviceAdapter kAnalogsensor{&analogsensor::get_capabilities,
                                  &analogsensor_init,
                                  &analogsensor::update_physics,
                                  &no_update_control,
                                  &analogsensor::get_device_info,
                                  &analogsensor::read_signals,
                                  &analogsensor::call_function,
                                  nullptr,
                                  0};

}  // namespace

std::optional<SimDeviceType> sim_device_type_from_string(const std::string &type) {
    if (type == "tempctl") {
        return SimDeviceType::Tempctl;
    }
    if (type == "motorctl") {
        return SimDeviceType::Motorctl;
    }
    if (type == "relayio") {
        return SimDeviceType::Relayio;
    }
    if (type == "analogsensor") {
        return SimDeviceType::Analogsensor;
    }
    return std::nullopt;
}

const DeviceAdapter &adapter_for(SimDeviceType type) {
    switch (type) {
        case SimDeviceType::Tempctl:
            return kTempctl;
        case SimDeviceType::Motorctl:
            return kMotorctl;
        case SimDeviceType::Relayio:
            return kRelayio;
        case SimDeviceType::Analogsensor:
            return kAnalogsensor;
    }
    return kTempctl;  // unreachable: exhaustive switch over a closed enum
}

const DeviceAdapter *adapter_by_id(const std::string &device_id) {
    using anolis_provider_sim::DeviceFactory;
    if (!DeviceFactory::is_config_loaded() || !DeviceFactory::is_device_registered(device_id)) {
        return nullptr;
    }
    const std::optional<SimDeviceType> type = sim_device_type_from_string(DeviceFactory::get_device_type(device_id));
    if (!type.has_value()) {
        return nullptr;
    }
    return &adapter_for(type.value());
}

}  // namespace sim_devices
