#pragma once

/**
 * @file chaos_control_device.hpp
 * @brief Synthetic control-only device that exposes runtime fault injection
 * APIs.
 */

#include <map>
#include <string>
#include <vector>

#include "devices/common/device_common.hpp"
#include "protocol.pb.h"

namespace sim_devices {
namespace chaos_control {

using anolis::deviceprovider::v1::CapabilitySet;
using anolis::deviceprovider::v1::Device;
using anolis::deviceprovider::v1::SignalValue;
using anolis::deviceprovider::v1::Value;

/** @brief Reserved device ID for the built-in chaos control surface. */
constexpr const char *kDeviceId = "chaos_control";

/** @brief Initialize the underlying fault injection state. */
void init();

/** @brief Return the synthetic device descriptor for the chaos control surface.
 */
Device get_device_info(bool include_health = false);

/** @brief Return the fixed capability surface for chaos fault injection
 * controls. */
CapabilitySet get_capabilities();

/** @brief Return no signals; this device is control-only by design. */
std::vector<SignalValue> read_signals(const std::vector<std::string> &signal_ids);

/** @brief Execute one fault injection control function. */
CallResult call_function(uint32_t function_id, const std::map<std::string, Value> &args);

}  // namespace chaos_control
}  // namespace sim_devices
