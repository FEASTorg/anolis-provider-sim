#pragma once

#include <map>
#include <string>
#include <vector>

#include "devices/common/device_common.hpp"
#include "protocol.pb.h"

namespace sim_devices {
namespace relayio {

using anolis::deviceprovider::v1::CapabilitySet;
using anolis::deviceprovider::v1::Device;
using anolis::deviceprovider::v1::SignalValue;
using anolis::deviceprovider::v1::Value;

// Device ID
constexpr const char *kDeviceId = "relayio0";

// Initialize device state for a specific instance
void init(const std::string &device_id);

// Update physics for a specific instance (called by device_manager)
void update_physics(const std::string &device_id, double dt);

// Get device info for a specific instance
Device get_device_info(const std::string &device_id,
                       bool include_health = false);

// Get capabilities (type-level, not instance-specific)
CapabilitySet get_capabilities();

// Read signals from a specific instance
std::vector<SignalValue>
read_signals(const std::string &device_id,
             const std::vector<std::string> &signal_ids);

// Call function on a specific instance
CallResult call_function(const std::string &device_id, uint32_t function_id,
                         const std::map<std::string, Value> &args);

} // namespace relayio
} // namespace sim_devices
