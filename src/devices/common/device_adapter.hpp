#pragma once

/**
 * @file device_adapter.hpp
 * @brief Uniform per-type device-adapter descriptor + the single typed dispatch
 * point for sim's device types.
 *
 * Each device type (tempctl/motorctl/relayio/analogsensor) is a small module
 * (`devices/<type>/`) exposing its behavior through this fn-ptr descriptor.
 * `adapter_for()` is the one place device-type dispatch happens — an exhaustive
 * switch over a closed enum (compiler-checked under -Werror=switch), not a
 * registry/vtable. Descriptors are `static const`, stateless; per-instance state
 * stays in the modules. The simulation-engine axis (SimulationEngine /
 * ProtocolAdapter) is orthogonal and unaffected.
 */

#include <yaml-cpp/yaml.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "devices/common/device_common.hpp"
#include "protocol.pb.h"

namespace sim_devices {

using anolis::deviceprovider::v1::CapabilitySet;
using anolis::deviceprovider::v1::Device;
using anolis::deviceprovider::v1::SignalValue;
using anolis::deviceprovider::v1::Value;

// Raw per-device config (the YAML subtree from the provider config); each
// adapter parses the keys it understands.
using DeviceConfigMap = std::map<std::string, YAML::Node>;

enum class SimDeviceType { Tempctl, Motorctl, Relayio, Analogsensor };

// Map the config/registry type string to the closed enum; nullopt if unknown.
std::optional<SimDeviceType> sim_device_type_from_string(const std::string &type);

// Uniform device-type interface (the union of sim's axes: capabilities, init,
// physics, optional closed-loop control, info, read, and per-type actuation).
struct DeviceAdapter {
    CapabilitySet (*get_capabilities)();
    void (*init)(const std::string &device_id, const DeviceConfigMap &config);
    void (*update_physics)(const std::string &device_id, double dt);
    void (*update_control)(const std::string &device_id);  // no-op where N/A (only tempctl has closed-loop)
    Device (*get_device_info)(const std::string &device_id, bool include_health);
    std::vector<SignalValue> (*read_signals)(const std::string &device_id, const std::vector<std::string> &signal_ids);
    CallResult (*call_function)(const std::string &device_id, uint32_t function_id,
                                const std::map<std::string, Value> &args);
    // Actuator signal-id suffixes (appended after "<device_id>/") the simulation
    // engine collects as physics inputs; nullptr/0 for types with no actuators.
    const char *const *actuator_signal_ids;
    std::size_t actuator_signal_count;
};

// The single dispatch site — exhaustive over SimDeviceType.
const DeviceAdapter &adapter_for(SimDeviceType type);

// Resolve a registered device_id to its adapter via the DeviceFactory registry;
// nullptr if the id is not registered or its type is unknown.
const DeviceAdapter *adapter_by_id(const std::string &device_id);

}  // namespace sim_devices
