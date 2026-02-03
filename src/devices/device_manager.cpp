#include "device_manager.hpp"

#include <chrono>

#include "tempctl_device.hpp"
#include "motorctl_device.hpp"

namespace sim_devices
{

    // -----------------------------
    // Physics time-stepping
    // -----------------------------

    static std::chrono::steady_clock::time_point last_update = std::chrono::steady_clock::now();

    static void step_world()
    {
        const auto now = std::chrono::steady_clock::now();
        const std::chrono::duration<double> dt = now - last_update;
        double seconds = dt.count();
        if (seconds <= 0.0)
            return;

        // Clamp dt to avoid huge jumps if paused under debugger.
        seconds = clamp(seconds, 0.0, 0.25); // max 250ms per step
        last_update = now;

        // Update all device physics
        tempctl::update_physics(seconds);
        motorctl::update_physics(seconds);
    }

    // -----------------------------
    // Public API
    // -----------------------------

    std::vector<Device> list_devices(bool include_health)
    {
        std::vector<Device> out;
        out.push_back(tempctl::get_device_info(include_health));
        out.push_back(motorctl::get_device_info(include_health));
        return out;
    }

    CapabilitySet describe_device(const std::string &device_id)
    {
        if (device_id == tempctl::kDeviceId)
        {
            return tempctl::get_capabilities();
        }

        if (device_id == motorctl::kDeviceId)
        {
            return motorctl::get_capabilities();
        }

        // Unknown device: return empty caps
        return CapabilitySet();
    }

    std::vector<SignalValue> read_signals(
        const std::string &device_id,
        const std::vector<std::string> &signal_ids)
    {
        step_world();

        if (device_id == tempctl::kDeviceId)
        {
            return tempctl::read_signals(signal_ids);
        }

        if (device_id == motorctl::kDeviceId)
        {
            return motorctl::read_signals(signal_ids);
        }

        // Unknown device
        return {};
    }

    CallResult call_function(
        const std::string &device_id,
        uint32_t function_id,
        const std::map<std::string, Value> &args)
    {
        step_world();

        if (device_id == tempctl::kDeviceId)
        {
            return tempctl::call_function(function_id, args);
        }

        if (device_id == motorctl::kDeviceId)
        {
            return motorctl::call_function(function_id, args);
        }

        return nf("unknown device_id");
    }

} // namespace sim_devices
