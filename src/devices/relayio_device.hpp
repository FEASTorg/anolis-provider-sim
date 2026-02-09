#pragma once

#include <map>
#include <string>
#include <vector>

#include "device_common.hpp"
#include "protocol.pb.h"

namespace sim_devices {
namespace relayio {

using anolis::deviceprovider::v0::CapabilitySet;
using anolis::deviceprovider::v0::Device;
using anolis::deviceprovider::v0::SignalValue;
using anolis::deviceprovider::v0::Value;

// Device ID
constexpr const char *kDeviceId = "relayio0";

// Initialize device state
void init();

// Update physics (called by device_manager)
void update_physics(double dt);

// Get device info
Device get_device_info(bool include_health = false);

// Get capabilities
CapabilitySet get_capabilities();

// Read signals
std::vector<SignalValue>
read_signals(const std::vector<std::string> &signal_ids);

// Call function
CallResult call_function(uint32_t function_id,
                         const std::map<std::string, Value> &args);

} // namespace relayio
} // namespace sim_devices
