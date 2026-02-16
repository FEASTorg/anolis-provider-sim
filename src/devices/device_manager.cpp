#include "device_manager.hpp"

#include <chrono>
#include <filesystem>
#include <thread>

#include "../config.hpp"
#include "../fault_injection.hpp"
#include "../physics/sim_physics.hpp"
#include "analogsensor_device.hpp"
#include "device_factory.hpp"
#include "motorctl_device.hpp"
#include "relayio_device.hpp"
#include "rule_engine.hpp"
#include "signal_registry.hpp"
#include "sim_control_device.hpp"
#include "tempctl_device.hpp"

namespace sim_devices {

// -----------------------------
// Physics engine and signal registry (owned by device_manager)
// -----------------------------

static std::unique_ptr<sim_physics::SimPhysics> g_physics_engine;
static std::unique_ptr<sim_coordination::SignalRegistry>
    g_signal_registry_owned;
sim_coordination::SignalRegistry *g_signal_registry = nullptr; // Public pointer

static std::unique_ptr<RuleEngine> g_rule_engine;  // Rule evaluation engine

// -----------------------------
// Physics time-stepping (non_interacting mode only)
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

  // Update all device physics for each registered device instance
  if (anolis_provider_sim::DeviceFactory::is_config_loaded()) {
    auto registered =
        anolis_provider_sim::DeviceFactory::get_registered_devices();
    for (const auto &entry : registered) {
      if (entry.type == "tempctl") {
        tempctl::update_physics(entry.id, seconds);
      } else if (entry.type == "motorctl") {
        motorctl::update_physics(entry.id, seconds);
      } else if (entry.type == "relayio") {
        relayio::update_physics(entry.id, seconds);
      } else if (entry.type == "analogsensor") {
        analogsensor::update_physics(entry.id, seconds);
      }
    }
  } else {
    // Fallback: update singleton instances (legacy behavior)
    tempctl::update_physics(tempctl::kDeviceId, seconds);
    motorctl::update_physics(motorctl::kDeviceId, seconds);
    relayio::update_physics(relayio::kDeviceId, seconds);
    analogsensor::update_physics(analogsensor::kDeviceId, seconds);
  }
}

// -----------------------------
// Physics engine management
// -----------------------------

void initialize_physics(
    const anolis_provider_sim::ProviderConfig &provider_config) {

  // Create SignalRegistry for physics-device coordination
  g_signal_registry_owned =
      std::make_unique<sim_coordination::SignalRegistry>();
  g_signal_registry = g_signal_registry_owned.get();

  // Set up device reader callback for registry
  // This is called when physics reads actuator signals (non-physics-driven)
  g_signal_registry->set_device_reader(
      [](const std::string &path) -> std::optional<double> {
        // Parse path: "device_id/signal_id"
        size_t slash_pos = path.find('/');
        if (slash_pos == std::string::npos) {
          return std::nullopt;
        }

        std::string device_id = path.substr(0, slash_pos);
        std::string signal_id = path.substr(slash_pos + 1);

        // Read actual device signal via device manager
        auto signals = read_signals(device_id, {signal_id});
        if (signals.empty()) {
          return std::nullopt;
        }

        // Convert protobuf Value to double
        // NOTE: This is the ONLY place we convert ADPP protocol types to double
        const auto &val = signals[0].value();
        using anolis::deviceprovider::v1::ValueType;

        if (val.type() == ValueType::VALUE_TYPE_DOUBLE) {
          return val.double_value();
        } else if (val.type() == ValueType::VALUE_TYPE_INT64) {
          return static_cast<double>(val.int64_value());
        } else if (val.type() == ValueType::VALUE_TYPE_BOOL) {
          return val.bool_value() ? 1.0 : 0.0;
        }

        return std::nullopt;
      });

  // Load physics config if in physics mode
  if (provider_config.simulation_mode ==
      anolis_provider_sim::SimulationMode::Physics) {
    std::cerr << "[DeviceManager] Loading physics config from: "
              << *provider_config.physics_config_path << std::endl;

    // Resolve physics config path relative to provider config directory
    std::filesystem::path config_dir =
        std::filesystem::path(provider_config.config_file_path).parent_path();
    std::filesystem::path physics_path =
        config_dir / *provider_config.physics_config_path;

    anolis_provider_sim::PhysicsConfig physics_config =
        anolis_provider_sim::load_physics_config(physics_path.string());

    // Create physics engine with signal registry
    g_physics_engine = std::make_unique<sim_physics::SimPhysics>(
        g_signal_registry, provider_config, physics_config);

    // Create rule engine if rules are defined
    if (!physics_config.rules.empty()) {
      g_rule_engine = std::make_unique<RuleEngine>(g_signal_registry, physics_config);
      
      // Set callback so physics ticker calls rule engine
      g_physics_engine->set_rule_callback([&]() {
        if (g_rule_engine) {
          g_rule_engine->evaluate_rules();
        }
      });
      
      std::cerr << "[DeviceManager] Initialized " << physics_config.rules.size()
                << " automation rules" << std::endl;
    }
  } else {
    // Create minimal physics engine for non-physics modes
    anolis_provider_sim::PhysicsConfig empty_physics_config;
    g_physics_engine = std::make_unique<sim_physics::SimPhysics>(
        g_signal_registry, provider_config, empty_physics_config);
  }
}

void start_physics() {
  if (g_physics_engine) {
    g_physics_engine->start();
  }
}

void stop_physics() {
  if (g_physics_engine) {
   g_physics_engine->stop();
    g_physics_engine.reset();
  }
  
  // Clean up rule engine
  g_rule_engine.reset();
  g_signal_registry = nullptr;
  g_signal_registry_owned.reset();
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

      // Get device info based on type, passing the configured device_id
      Device device_info;
      if (entry.type == "tempctl") {
        device_info = tempctl::get_device_info(entry.id, include_health);
      } else if (entry.type == "motorctl") {
        device_info = motorctl::get_device_info(entry.id, include_health);
      } else if (entry.type == "relayio") {
        device_info = relayio::get_device_info(entry.id, include_health);
      } else if (entry.type == "analogsensor") {
        device_info = analogsensor::get_device_info(entry.id, include_health);
      } else {
        continue; // Unknown type
      }

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

  // Check for special sim_control device
  if (device_id == sim_control::kDeviceId) {
    return sim_control::get_capabilities();
  }

  // Reject unknown device_id - require config registration
  if (!anolis_provider_sim::DeviceFactory::is_config_loaded() ||
      !anolis_provider_sim::DeviceFactory::is_device_registered(device_id)) {
    return CapabilitySet(); // Return empty for unknown devices
  }

  // Get device type from registry
  std::string device_type =
      anolis_provider_sim::DeviceFactory::get_device_type(device_id);

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
  // Phase 22: No automatic stepping in request handlers
  // Ticker threads drive physics updates

  // Check if device is unavailable due to fault injection
  if (fault_injection::is_device_unavailable(device_id)) {
    // std::cerr << "[Sim] Device " << device_id << " is UNAVAILABLE\n";
    return {}; // Return empty to simulate unavailable
  }

  // Check for special sim_control device
  if (device_id == sim_control::kDeviceId) {
    return sim_control::read_signals(signal_ids);
  }

  // Reject unknown device_id - require config registration
  if (!anolis_provider_sim::DeviceFactory::is_config_loaded() ||
      !anolis_provider_sim::DeviceFactory::is_device_registered(device_id)) {
    return {}; // Return empty for unknown devices
  }

  // Get device type from registry
  std::string device_type =
      anolis_provider_sim::DeviceFactory::get_device_type(device_id);

  // Read signals from device based on type
  std::vector<SignalValue> signals;

  if (device_type == "tempctl") {
    signals = tempctl::read_signals(device_id, signal_ids);
  } else if (device_type == "motorctl") {
    signals = motorctl::read_signals(device_id, signal_ids);
  } else if (device_type == "relayio") {
    signals = relayio::read_signals(device_id, signal_ids);
  } else if (device_type == "analogsensor") {
    signals = analogsensor::read_signals(device_id, signal_ids);
  } else {
    // Unknown device type
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

  // Check for special sim_control device
  if (device_id == sim_control::kDeviceId) {
    return sim_control::call_function(function_id, args);
  }

  // Reject unknown device_id - require config registration
  if (!anolis_provider_sim::DeviceFactory::is_config_loaded() ||
      !anolis_provider_sim::DeviceFactory::is_device_registered(device_id)) {
    return nf("unknown device_id: " + device_id);
  }

  // Get device type from registry
  std::string device_type =
      anolis_provider_sim::DeviceFactory::get_device_type(device_id);

  // Route to device implementations based on type
  if (device_type == "tempctl") {
    return tempctl::call_function(device_id, function_id, args);
  }

  if (device_type == "motorctl") {
    return motorctl::call_function(device_id, function_id, args);
  }

  if (device_type == "relayio") {
    return relayio::call_function(device_id, function_id, args);
  }

  if (device_type == "analogsensor") {
    return analogsensor::call_function(device_id, function_id, args);
  }

  return nf("unknown device type: " + device_type);
}

} // namespace sim_devices
