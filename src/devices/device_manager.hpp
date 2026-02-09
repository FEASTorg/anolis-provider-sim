#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "device_common.hpp"
#include "protocol.pb.h"

namespace sim_devices {

using anolis::deviceprovider::v0::CapabilitySet;
using anolis::deviceprovider::v0::Device;
using anolis::deviceprovider::v0::SignalValue;
using anolis::deviceprovider::v0::Value;

// ---- Public API used by handlers ----

std::vector<Device> list_devices(bool include_health = false);
CapabilitySet describe_device(const std::string &device_id);

std::vector<SignalValue>
read_signals(const std::string &device_id,
             const std::vector<std::string> &signal_ids);

CallResult call_function(const std::string &device_id, uint32_t function_id,
                         const std::map<std::string, Value> &args);

} // namespace sim_devices
