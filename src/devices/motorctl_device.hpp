#pragma once

#include <map>
#include <string>
#include <vector>

#include "device_common.hpp"
#include "protocol.pb.h"

namespace sim_devices {
namespace motorctl {

using anolis::deviceprovider::v1::CapabilitySet;
using anolis::deviceprovider::v1::Device;
using anolis::deviceprovider::v1::SignalValue;
using anolis::deviceprovider::v1::Value;

// Device ID
constexpr const char *kDeviceId = "motorctl0";

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

} // namespace motorctl
} // namespace sim_devices
