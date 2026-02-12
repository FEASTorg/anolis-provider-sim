#pragma once

#include "device_common.hpp"

namespace sim_devices {
namespace analogsensor {

using anolis::deviceprovider::v1::CapabilitySet;
using anolis::deviceprovider::v1::Device;

static constexpr const char *kDeviceId = "analogsensor0";

void init(const std::string &device_id);
void update_physics(const std::string &device_id, double dt);

Device get_device_info(const std::string &device_id, bool include_health);
CapabilitySet get_capabilities();
std::vector<SignalValue>
read_signals(const std::string &device_id, const std::vector<std::string> &signal_ids);
CallResult call_function(const std::string &device_id, uint32_t function_id,
                         const std::map<std::string, Value> &args);

} // namespace analogsensor
} // namespace sim_devices
