#pragma once

#include "device_common.hpp"

namespace sim_devices
{
    namespace analogsensor
    {

        using anolis::deviceprovider::v0::CapabilitySet;
        using anolis::deviceprovider::v0::Device;

        static constexpr const char *kDeviceId = "analogsensor0";

        void init();
        void update_physics(double dt);

        Device get_device_info(bool include_health);
        CapabilitySet get_capabilities();
        std::vector<SignalValue> read_signals(const std::vector<std::string> &signal_ids);
        CallResult call_function(uint32_t function_id, const std::map<std::string, Value> &args);

    } // namespace analogsensor
} // namespace sim_devices
