#include "device_manager.hpp"

#include <chrono>
#include <thread>

#include "tempctl_device.hpp"
#include "motorctl_device.hpp"
#include "relayio_device.hpp"
#include "analogsensor_device.hpp"
#include "sim_control_device.hpp"
#include "../fault_injection.hpp"

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
        relayio::update_physics(seconds);
        analogsensor::update_physics(seconds);
    }

    // -----------------------------
    // Public API
    // -----------------------------

    std::vector<Device> list_devices(bool include_health)
    {
        std::vector<Device> out;

        if (!fault_injection::is_device_unavailable(tempctl::kDeviceId))
            out.push_back(tempctl::get_device_info(include_health));
        
        if (!fault_injection::is_device_unavailable(motorctl::kDeviceId))
            out.push_back(motorctl::get_device_info(include_health));

        if (!fault_injection::is_device_unavailable(relayio::kDeviceId))
            out.push_back(relayio::get_device_info(include_health));

        if (!fault_injection::is_device_unavailable(analogsensor::kDeviceId))
            out.push_back(analogsensor::get_device_info(include_health));

        // Control device is always available (unless we want to simulate total provider failure)
        // But the test injects faults per device.
        out.push_back(sim_control::get_device_info(include_health));
        
        return out;
    }

    CapabilitySet describe_device(const std::string &device_id)
    {
        // Check if device is unavailable due to fault injection
        if (fault_injection::is_device_unavailable(device_id))
        {
            return CapabilitySet(); // Return empty to simulate unavailable
        }

        if (device_id == tempctl::kDeviceId)
        {
            return tempctl::get_capabilities();
        }

        if (device_id == motorctl::kDeviceId)
        {
            return motorctl::get_capabilities();
        }

        if (device_id == relayio::kDeviceId)
        {
            return relayio::get_capabilities();
        }

        if (device_id == analogsensor::kDeviceId)
        {
            return analogsensor::get_capabilities();
        }

        if (device_id == sim_control::kDeviceId)
        {
            return sim_control::get_capabilities();
        }

        // Unknown device: return empty caps
        return CapabilitySet();
    }

    std::vector<SignalValue> read_signals(
        const std::string &device_id,
        const std::vector<std::string> &signal_ids)
    {
        step_world();

        // Check if device is unavailable due to fault injection
        if (fault_injection::is_device_unavailable(device_id))
        {
            // std::cerr << "[Sim] Device " << device_id << " is UNAVAILABLE\n";
            return {}; // Return empty to simulate unavailable
        }

        // Read signals from device
        std::vector<SignalValue> signals;
        
        if (device_id == tempctl::kDeviceId)
        {
            signals = tempctl::read_signals(signal_ids);
        }
        else if (device_id == motorctl::kDeviceId)
        {
            signals = motorctl::read_signals(signal_ids);
        }
        else if (device_id == relayio::kDeviceId)
        {
            signals = relayio::read_signals(signal_ids);
        }
        else if (device_id == analogsensor::kDeviceId)
        {
            signals = analogsensor::read_signals(signal_ids);
        }
        else if (device_id == sim_control::kDeviceId)
        {
            signals = sim_control::read_signals(signal_ids);
        }
        else
        {
            // Unknown device
            return {};
        }

        // Apply signal fault injection if any faults are active
        for (auto &signal : signals)
        {
            if (fault_injection::is_signal_faulted(device_id, signal.signal_id()))
            {
                // Override quality to FAULT
                signal.set_quality(SignalValue::QUALITY_FAULT);
                // Value remains frozen (no update to value field)
            }
        }

        return signals;
    }

    CallResult call_function(
        const std::string &device_id,
        uint32_t function_id,
        const std::map<std::string, Value> &args)
    {
        step_world();

        // Check if device is unavailable due to fault injection
        if (fault_injection::is_device_unavailable(device_id))
        {
            return bad("device unavailable (injected fault)");
        }

        // Check for injected call latency
        int64_t latency_ms = fault_injection::get_call_latency(device_id);
        if (latency_ms > 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(latency_ms));
        }

        // Check for injected call failure (need function name - convert function_id to name)
        // For simplicity, we'll check using string representation of function_id
        std::string function_id_str = std::to_string(function_id);
        if (fault_injection::should_call_fail(device_id, function_id_str))
        {
            return bad("function call failed (injected fault)");
        }

        // Route to device implementations
        if (device_id == tempctl::kDeviceId)
        {
            return tempctl::call_function(function_id, args);
        }

        if (device_id == motorctl::kDeviceId)
        {
            return motorctl::call_function(function_id, args);
        }

        if (device_id == relayio::kDeviceId)
        {
            return relayio::call_function(function_id, args);
        }

        if (device_id == analogsensor::kDeviceId)
        {
            return analogsensor::call_function(function_id, args);
        }

        if (device_id == sim_control::kDeviceId)
        {
            return sim_control::call_function(function_id, args);
        }

        return nf("unknown device_id");
    }

} // namespace sim_devices
