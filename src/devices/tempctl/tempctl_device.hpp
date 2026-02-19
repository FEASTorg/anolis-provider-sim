#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "devices/common/device_common.hpp"
#include "protocol.pb.h"

namespace sim_devices {
namespace tempctl {

using anolis::deviceprovider::v1::CapabilitySet;
using anolis::deviceprovider::v1::Device;
using anolis::deviceprovider::v1::SignalValue;
using anolis::deviceprovider::v1::Value;

// Device ID
constexpr const char *kDeviceId = "tempctl0";

// Configuration parameters
struct Config {
  std::optional<double> initial_temp; // Initial temperature (C)
  std::optional<std::pair<double, double>> temp_range; // Min/max temp range (C)
};

// Initialize device state for a specific instance
void init(const std::string &device_id, const Config &config = Config{});

// Update physics for a specific instance (called by device_manager)
void update_physics(const std::string &device_id, double dt);

// Update control logic (closed-loop bang-bang control)
// Reads current temperature from signal registry, updates relay states based on
// mode/setpoint
void update_control(const std::string &device_id);

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

} // namespace tempctl
} // namespace sim_devices
