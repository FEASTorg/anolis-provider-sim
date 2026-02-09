#include "device_manager.hpp"

#include <chrono>
#include <thread>

#include "../fault_injection.hpp"
#include "analogsensor_device.hpp"
#include "device_factory.hpp"
#include "motorctl_device.hpp"
#include "relayio_device.hpp"
#include "sim_control_device.hpp"
#include "tempctl_device.hpp"

namespace sim_devices {

// -----------------------------
// Physics time-stepping
// -----------------------------

static std::chrono::steady_clock::time_point last_update =
    std::chrono::steady_clock::now();

static void step_world() {
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

std::vector<Device> list_devices(bool include_health) {
  std::vector<Device> out;

  // Check if using config-based registry
  if (anolis_provider_sim::DeviceFactory::is_config_loaded()) {
    // Use configured devices
    auto registered =
        anolis_provider_sim::DeviceFactory::get_registered_devices();
    for (const auto &entry : registered) {
      if (fault_injection::is_device_unavailable(entry.id)) {
        continue; // Skip unavailable devices
      }

      // Get device info based on type
      Device device_info;
      if (entry.type == "tempctl") {
        device_info = tempctl::get_device_info(include_health);
      } else if (entry.type == "motorctl") {
        device_info = motorctl::get_device_info(include_health);
      } else if (entry.type == "relayio") {
        device_info = relayio::get_device_info(include_health);
      } else if (entry.type == "analogsensor") {
        device_info = analogsensor::get_device_info(include_health);
      } else {
        continue; // Unknown type
      }

      // Override device_id with configured ID
      device_info.set_device_id(entry.id);
      out.push_back(device_info);
    }

    // Always include sim_control device
    out.push_back(sim_control::get_device_info(include_health));
  }

  return out;
}

CapabilitySet describe_device(const std::string &device_id) {
  // Check if device is unavailable due to fault injection
  if (fault_injection::is_device_unavailable(device_id)) {
    return CapabilitySet(); // Return empty to simulate unavailable
  }

  // Map configured device ID to type if using registry
  std::string device_type = device_id;
  if (anolis_provider_sim::DeviceFactory::is_config_loaded() &&
      anolis_provider_sim::DeviceFactory::is_device_registered(device_id)) {
    device_type =
        anolis_provider_sim::DeviceFactory::get_device_type(device_id);
  }

  // Route to device capabilities based on type
  if (device_type == "tempctl") {
    return tempctl::get_capabilities();
  }

  if (device_type == "motorctl") {
    return motorctl::get_capabilities();
  }

  if (device_type == "relayio") {
    return relayio::get_capabilities();
  }

  if (device_type == "analogsensor") {
    return analogsensor::get_capabilities();
  }

  if (device_id == sim_control::kDeviceId) {
    return sim_control::get_capabilities();
  }

  // Unknown device: return empty caps
  return CapabilitySet();
}

std::vector<SignalValue>
read_signals(const std::string &device_id,
             const std::vector<std::string> &signal_ids) {
  step_world();

  // Check if device is unavailable due to fault injection
  if (fault_injection::is_device_unavailable(device_id)) {
    // std::cerr << "[Sim] Device " << device_id << " is UNAVAILABLE\n";
    return {}; // Return empty to simulate unavailable
  }

  // Map configured device ID to type if using registry
  std::string device_type = device_id;
  if (anolis_provider_sim::DeviceFactory::is_config_loaded() &&
      anolis_provider_sim::DeviceFactory::is_device_registered(device_id)) {
    device_type =
        anolis_provider_sim::DeviceFactory::get_device_type(device_id);
  }

  // Read signals from device based on type
  std::vector<SignalValue> signals;

  if (device_type == "tempctl") {
    signals = tempctl::read_signals(signal_ids);
  } else if (device_type == "motorctl") {
    signals = motorctl::read_signals(signal_ids);
  } else if (device_type == "relayio") {
    signals = relayio::read_signals(signal_ids);
  } else if (device_type == "analogsensor") {
    signals = analogsensor::read_signals(signal_ids);
  } else if (device_id == sim_control::kDeviceId) {
    signals = sim_control::read_signals(signal_ids);
  } else {
    // Unknown device
    return {};
  }

  // Apply signal fault injection if any faults are active
  for (auto &signal : signals) {
    if (fault_injection::is_signal_faulted(device_id, signal.signal_id())) {
      // Override quality to FAULT
      signal.set_quality(SignalValue::QUALITY_FAULT);
      // Value remains frozen (no update to value field)
    }
  }

  return signals;
}

CallResult call_function(const std::string &device_id, uint32_t function_id,
                         const std::map<std::string, Value> &args) {
  step_world();

  // Check if device is unavailable due to fault injection
  if (fault_injection::is_device_unavailable(device_id)) {
    return bad("device unavailable (injected fault)");
  }

  // Check for injected call latency
  int64_t latency_ms = fault_injection::get_call_latency(device_id);
  if (latency_ms > 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(latency_ms));
  }

  // Check for injected call failure (need function name - convert function_id
  // to name) For simplicity, we'll check using string representation of
  // function_id
  std::string function_id_str = std::to_string(function_id);
  if (fault_injection::should_call_fail(device_id, function_id_str)) {
    return bad("function call failed (injected fault)");
  }

  // Map configured device ID to type if using registry
  std::string device_type = device_id;
  if (anolis_provider_sim::DeviceFactory::is_config_loaded() &&
      anolis_provider_sim::DeviceFactory::is_device_registered(device_id)) {
    device_type =
        anolis_provider_sim::DeviceFactory::get_device_type(device_id);
  }

  // Route to device implementations based on type
  if (device_type == "tempctl") {
    return tempctl::call_function(function_id, args);
  }

  if (device_type == "motorctl") {
    return motorctl::call_function(function_id, args);
  }

  if (device_type == "relayio") {
    return relayio::call_function(function_id, args);
  }

  if (device_type == "analogsensor") {
    return analogsensor::call_function(function_id, args);
  }

  if (device_id == sim_control::kDeviceId) {
    return sim_control::call_function(function_id, args);
  }

  return nf("unknown device_id");
}

} // namespace sim_devices
