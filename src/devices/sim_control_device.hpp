#pragma once

#include <map>
#include <string>
#include <vector>

#include "protocol.pb.h"
#include "device_common.hpp"

namespace sim_devices
{
    namespace sim_control
    {

        using anolis::deviceprovider::v0::CapabilitySet;
        using anolis::deviceprovider::v0::Device;
        using anolis::deviceprovider::v0::SignalValue;
        using anolis::deviceprovider::v0::Value;

        // Device ID
        constexpr const char *kDeviceId = "sim_control";

        // Initialize device state
        void init();

        // No physics for control device
        // void update_physics(double dt);  -- not needed

        // Get device info
        Device get_device_info(bool include_health = false);

        // Get capabilities
        CapabilitySet get_capabilities();

        // Read signals (no signals for control device)
        std::vector<SignalValue> read_signals(const std::vector<std::string> &signal_ids);

        // Call function (fault injection controls)
        CallResult call_function(uint32_t function_id, const std::map<std::string, Value> &args);

    } // namespace sim_control
} // namespace sim_devices
