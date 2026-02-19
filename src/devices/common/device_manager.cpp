#include "devices/common/device_manager.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <thread>

#include "config.hpp"
#include "chaos/fault_injection.hpp"
#include "devices/analogsensor/analogsensor_device.hpp"
#include "device_factory.hpp"
#include "devices/motorctl/motorctl_device.hpp"
#include "devices/relayio/relayio_device.hpp"
#include "chaos/chaos_control_device.hpp"
#include "devices/tempctl/tempctl_device.hpp"

namespace sim_devices {

// -----------------------------
// Shared coordination/runtime state
// -----------------------------

static std::unique_ptr<sim_coordination::SignalRegistry> g_signal_registry_owned;
sim_coordination::SignalRegistry *g_signal_registry = nullptr;

static std::unique_ptr<sim_engine::SimulationEngine> g_simulation_engine;

static std::unique_ptr<std::thread> g_ticker_thread;
static std::atomic<bool> g_ticker_running{false};
static double g_tick_rate_hz = 10.0;
static anolis_provider_sim::SimulationMode g_sim_mode =
    anolis_provider_sim::SimulationMode::Inert;

static std::map<std::string, std::map<std::string, uint32_t>>
    g_function_name_to_id;
static std::vector<std::string> g_physics_output_paths;

struct ConstantSignalInput {
  std::string path;
  double value;
};
static std::optional<ConstantSignalInput> g_ambient_input;

// -----------------------------
// Helpers
// -----------------------------

void set_simulation_engine(std::unique_ptr<sim_engine::SimulationEngine> engine) {
  g_simulation_engine = std::move(engine);
}

static void cache_device_capabilities() {
  g_function_name_to_id.clear();

  for (const auto &dev : list_devices(false)) {
    const std::string device_id = dev.device_id();
    auto caps = describe_device(device_id);

    for (const auto &func : caps.functions()) {
      g_function_name_to_id[device_id][func.name()] = func.function_id();
    }
  }
}

static void rebuild_physics_output_paths(
    const anolis_provider_sim::ProviderConfig &provider_config) {
  g_physics_output_paths.clear();

  // Sim mode: query the engine directly - it already parsed FluxGraph config
  if (provider_config.simulation_mode == anolis_provider_sim::SimulationMode::Sim) {
    if (g_simulation_engine) {
      std::set<std::string> known_device_ids;
      for (const auto &dev : provider_config.devices) {
        known_device_ids.insert(dev.id);
      }

      auto signal_paths = g_simulation_engine->list_signals();
      for (const auto &path : signal_paths) {
        // Filter to only signals owned by our devices
        const auto slash = path.find('/');
        if (slash != std::string::npos) {
          std::string device_id = path.substr(0, slash);
          if (known_device_ids.count(device_id) > 0) {
            g_physics_output_paths.push_back(path);
            if (g_signal_registry) {
              g_signal_registry->mark_physics_driven(path);
            }
          }
        }
      }
    }
    return;
  }

  // Non-interacting mode: parse legacy format physics config
  if (!provider_config.physics_config_path) {
    return;
  }

  std::filesystem::path config_dir =
      std::filesystem::path(provider_config.config_file_path).parent_path();
  std::filesystem::path physics_path =
      config_dir / *provider_config.physics_config_path;

  auto physics_cfg = anolis_provider_sim::load_physics_config(physics_path.string());

  std::set<std::string> known_device_ids;
  std::set<std::string> seen_paths;
  for (const auto &dev : provider_config.devices) {
    known_device_ids.insert(dev.id);
  }

  for (const auto &edge : physics_cfg.signal_graph) {
    const auto slash = edge.target.find('/');
    if (slash == std::string::npos) {
      continue;
    }

    const std::string target_device = edge.target.substr(0, slash);
    if (known_device_ids.count(target_device) == 0) {
      continue;
    }
    if (!seen_paths.insert(edge.target).second) {
      continue;
    }

    g_physics_output_paths.push_back(edge.target);
    if (g_signal_registry) {
      g_signal_registry->mark_physics_driven(edge.target);
    }
  }
}

static void maybe_collect_signal(const std::string &path,
                                 std::map<std::string, double> &out) {
  if (!g_signal_registry) {
    return;
  }
  if (g_signal_registry->is_physics_driven(path)) {
    // Provider should not overwrite physics-owned outputs.
    return;
  }

  auto v = g_signal_registry->read_signal(path);
  if (v) {
    out[path] = *v;
  }
}

static void collect_actuator_signals(std::map<std::string, double> &signals) {
  auto registered = anolis_provider_sim::DeviceFactory::get_registered_devices();
  for (const auto &entry : registered) {
    if (entry.type == "tempctl") {
      maybe_collect_signal(entry.id + "/relay1_state", signals);
      maybe_collect_signal(entry.id + "/relay2_state", signals);
    } else if (entry.type == "motorctl") {
      maybe_collect_signal(entry.id + "/motor1_duty", signals);
      maybe_collect_signal(entry.id + "/motor2_duty", signals);
    } else if (entry.type == "relayio") {
      maybe_collect_signal(entry.id + "/relay_ch1_state", signals);
      maybe_collect_signal(entry.id + "/relay_ch2_state", signals);
      maybe_collect_signal(entry.id + "/relay_ch3_state", signals);
      maybe_collect_signal(entry.id + "/relay_ch4_state", signals);
    }
  }
}

static void configure_simulation_inputs(
    const anolis_provider_sim::ProviderConfig &provider_config) {
  g_ambient_input.reset();

  if (provider_config.simulation_mode != anolis_provider_sim::SimulationMode::Sim) {
    return;
  }

  const auto ambient_it = provider_config.simulation.find("ambient_temp_c");
  if (ambient_it == provider_config.simulation.end()) {
    return;
  }

  double ambient_temp = 0.0;
  try {
    ambient_temp = ambient_it->second.as<double>();
  } catch (const YAML::Exception &) {
    throw std::runtime_error(
        "[CONFIG] simulation.ambient_temp_c must be numeric");
  }

  std::string ambient_path = "environment/ambient_temp";
  const auto path_it = provider_config.simulation.find("ambient_signal_path");
  if (path_it != provider_config.simulation.end()) {
    try {
      ambient_path = path_it->second.as<std::string>();
    } catch (const YAML::Exception &) {
      throw std::runtime_error(
          "[CONFIG] simulation.ambient_signal_path must be a string");
    }
  }

  if (ambient_path.empty()) {
    throw std::runtime_error(
        "[CONFIG] simulation.ambient_signal_path cannot be empty");
  }

  g_ambient_input = ConstantSignalInput{ambient_path, ambient_temp};
  std::cerr << "[DeviceManager] Configured ambient input: " << ambient_path
            << "=" << ambient_temp << "\n";
}

static void execute_engine_command(const sim_engine::Command &cmd) {
  const auto dev_it = g_function_name_to_id.find(cmd.device_id);
  if (dev_it == g_function_name_to_id.end()) {
    std::cerr << "[DeviceManager] Unknown command device: " << cmd.device_id
              << "\n";
    return;
  }

  const auto fn_it = dev_it->second.find(cmd.function_name);
  if (fn_it == dev_it->second.end()) {
    std::cerr << "[DeviceManager] Unknown command function: " << cmd.device_id
              << "::" << cmd.function_name << "\n";
    return;
  }

  std::map<std::string, anolis::deviceprovider::v1::Value> pb_args;
  for (const auto &[k, v] : cmd.args) {
    anolis::deviceprovider::v1::Value pb;
    if (std::holds_alternative<double>(v)) {
      pb.set_type(anolis::deviceprovider::v1::ValueType::VALUE_TYPE_DOUBLE);
      pb.set_double_value(std::get<double>(v));
    } else if (std::holds_alternative<int64_t>(v)) {
      pb.set_type(anolis::deviceprovider::v1::ValueType::VALUE_TYPE_INT64);
      pb.set_int64_value(std::get<int64_t>(v));
    } else if (std::holds_alternative<bool>(v)) {
      pb.set_type(anolis::deviceprovider::v1::ValueType::VALUE_TYPE_BOOL);
      pb.set_bool_value(std::get<bool>(v));
    } else if (std::holds_alternative<std::string>(v)) {
      pb.set_type(anolis::deviceprovider::v1::ValueType::VALUE_TYPE_STRING);
      pb.set_string_value(std::get<std::string>(v));
    }
    pb_args[k] = pb;
  }

  const auto result = call_function(cmd.device_id, fn_it->second, pb_args);
  if (result.code != anolis::deviceprovider::v1::Status::CODE_OK) {
    std::cerr << "[DeviceManager] Command failed: " << result.message << "\n";
  }
}

static void ticker_thread_func(double tick_rate_hz) {
  const double dt = 1.0 / tick_rate_hz;
  const auto tick_duration =
      std::chrono::duration_cast<std::chrono::steady_clock::duration>(
          std::chrono::duration<double>(dt));
  
  const auto thread_start = std::chrono::steady_clock::now();
  const auto thread_start_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      thread_start.time_since_epoch()).count();
  
  std::cerr << "[Ticker] Thread started at steady_clock=" << thread_start_ms << "ms\\n";
  std::cerr << "[Ticker] Tick period: " << (dt * 1000.0) << "ms (@" << tick_rate_hz << " Hz)\\n";
  
  // Start ticking immediately from thread creation time
  auto next_tick = thread_start;

  int tick_count = 0;
  
  while (g_ticker_running.load()) {
    const auto tick_start = std::chrono::steady_clock::now();
    const auto tick_start_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        tick_start.time_since_epoch()).count();
    
    // Update device control logic BEFORE collecting actuator states.
    // This allows closed-loop controllers to read sensor values from the
    // previous tick and update relay/actuator states accordingly.
    auto devices = anolis_provider_sim::DeviceFactory::get_registered_devices();
    for (const auto &dev : devices) {
      if (dev.type == "tempctl") {
        sim_devices::tempctl::update_control(dev.id);
      }
      // Future: add control updates for other device types here
    }
    
    std::map<std::string, double> actuators;
    collect_actuator_signals(actuators);

    if (g_sim_mode == anolis_provider_sim::SimulationMode::Sim &&
        g_ambient_input.has_value()) {
      actuators[g_ambient_input->path] = g_ambient_input->value;
    }

    // Debug first 2 ticks only
    if (tick_count < 2) {
      std::cerr << "[Ticker] Tick #" << tick_count 
                << " at steady_clock " << tick_start_ms << " ms"
                << " (delta=" << (tick_start_ms - thread_start_ms) << "ms from thread start)\n";
      std::cerr << "[Ticker]   Sending " << actuators.size() << " signals\n";
    }

    if (!g_simulation_engine) {
      std::cerr << "[DeviceManager] Missing simulation engine in ticker\n";
      break;
    }

    const sim_engine::TickResult result = g_simulation_engine->tick(actuators);

    if (result.success) {
      if (tick_count < 2) {
        const auto tick_end = std::chrono::steady_clock::now();
        const auto tick_end_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            tick_end.time_since_epoch()).count();
        const auto rpc_duration_ms = tick_end_ms - tick_start_ms;
        std::cerr << "[Ticker] Tick #" << tick_count 
                  << " SUCCESS (RPC took " << rpc_duration_ms << "ms)\n";
      }
      
      if (g_signal_registry) {
        for (const auto &[path, value] : result.sensors) {
          g_signal_registry->write_signal(path, value);
        }
      }

      for (const auto &cmd : result.commands) {
        execute_engine_command(cmd);
      }
    } else {
      if (tick_count < 2) {
        std::cerr << "[Ticker] Tick #" << tick_count << " FAILED (maintaining schedule)\n";
      }
      // Continue with stale data but MAINTAIN THE TICK SCHEDULE.
      // Don't let timeouts/failures shift our phase relative to other providers.
    }

    tick_count++;
    
    // CRITICAL: Always advance next_tick by tick_duration, regardless of success/failure.
    // This maintains consistent phase alignment across providers even when one times out.
    next_tick += tick_duration;
    
    // If we're significantly behind (e.g., first startup, or multi-second block),
    // catch up gradually rather than immediately resetting phase.
    const auto now = std::chrono::steady_clock::now();
    while (next_tick <= now && g_ticker_running.load()) {
      next_tick += tick_duration;
    }
    
    std::this_thread::sleep_until(next_tick);
  }
}

// -----------------------------
// Physics lifecycle
// -----------------------------

void initialize_physics(
    const anolis_provider_sim::ProviderConfig &provider_config) {
  g_sim_mode = provider_config.simulation_mode;
  g_tick_rate_hz = provider_config.tick_rate_hz.value_or(10.0);
  configure_simulation_inputs(provider_config);

  g_signal_registry_owned = std::make_unique<sim_coordination::SignalRegistry>();
  g_signal_registry = g_signal_registry_owned.get();

  // Non-physics reads delegate to current device state.
  g_signal_registry->set_device_reader(
      [](const std::string &path) -> std::optional<double> {
        const size_t slash_pos = path.find('/');
        if (slash_pos == std::string::npos) {
          return std::nullopt;
        }

        const std::string device_id = path.substr(0, slash_pos);
        const std::string signal_id = path.substr(slash_pos + 1);

        const auto signals = read_signals(device_id, {signal_id});
        if (signals.empty()) {
          return std::nullopt;
        }

        const auto &val = signals[0].value();
        using anolis::deviceprovider::v1::ValueType;
        if (val.type() == ValueType::VALUE_TYPE_DOUBLE) {
          return val.double_value();
        }
        if (val.type() == ValueType::VALUE_TYPE_INT64) {
          return static_cast<double>(val.int64_value());
        }
        if (val.type() == ValueType::VALUE_TYPE_BOOL) {
          return val.bool_value() ? 1.0 : 0.0;
        }
        return std::nullopt;
      });

  cache_device_capabilities();
  rebuild_physics_output_paths(provider_config);
}

void start_physics() {
  if (g_sim_mode == anolis_provider_sim::SimulationMode::Inert) {
    std::cerr << "[DeviceManager] start_physics: inert mode, skipping\n";
    return;
  }
  if (!g_simulation_engine) {
    std::cerr << "[DeviceManager] Non-inert mode requires simulation engine; "
                 "ticker not started\n";
    return;
  }
  if (g_ticker_running.load()) {
    std::cerr << "[DeviceManager] start_physics: already running, skipping\n";
    return;
  }

  std::cerr << "[DeviceManager] start_physics: spawning ticker thread (@" 
            << g_tick_rate_hz << " Hz)\n";
  g_ticker_running = true;
  g_ticker_thread = std::make_unique<std::thread>(ticker_thread_func, g_tick_rate_hz);
  std::cerr << "[DeviceManager] start_physics: ticker thread started\n";
}

void stop_physics() {
  g_ticker_running = false;
  if (g_ticker_thread && g_ticker_thread->joinable()) {
    g_ticker_thread->join();
  }
  g_ticker_thread.reset();

  g_physics_output_paths.clear();
  g_function_name_to_id.clear();
  g_signal_registry = nullptr;
  g_signal_registry_owned.reset();
  g_simulation_engine.reset();
  g_ambient_input.reset();
}

// -----------------------------
// Public API
// -----------------------------

std::vector<Device> list_devices(bool include_health) {
  std::vector<Device> out;

  if (anolis_provider_sim::DeviceFactory::is_config_loaded()) {
    auto registered = anolis_provider_sim::DeviceFactory::get_registered_devices();
    for (const auto &entry : registered) {
      if (fault_injection::is_device_unavailable(entry.id)) {
        continue;
      }

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
        continue;
      }
      out.push_back(device_info);
    }

    out.push_back(chaos_control::get_device_info(include_health));
  }

  return out;
}

CapabilitySet describe_device(const std::string &device_id) {
  if (fault_injection::is_device_unavailable(device_id)) {
    return CapabilitySet();
  }

  if (device_id == chaos_control::kDeviceId) {
    return chaos_control::get_capabilities();
  }

  if (!anolis_provider_sim::DeviceFactory::is_config_loaded() ||
      !anolis_provider_sim::DeviceFactory::is_device_registered(device_id)) {
    return CapabilitySet();
  }

  const std::string device_type =
      anolis_provider_sim::DeviceFactory::get_device_type(device_id);

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
  return CapabilitySet();
}

std::vector<SignalValue>
read_signals(const std::string &device_id,
             const std::vector<std::string> &signal_ids) {
  if (fault_injection::is_device_unavailable(device_id)) {
    return {};
  }

  if (device_id == chaos_control::kDeviceId) {
    return chaos_control::read_signals(signal_ids);
  }

  if (!anolis_provider_sim::DeviceFactory::is_config_loaded() ||
      !anolis_provider_sim::DeviceFactory::is_device_registered(device_id)) {
    return {};
  }

  const std::string device_type =
      anolis_provider_sim::DeviceFactory::get_device_type(device_id);

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
    return {};
  }

  for (auto &signal : signals) {
    if (fault_injection::is_signal_faulted(device_id, signal.signal_id())) {
      signal.set_quality(SignalValue::QUALITY_FAULT);
    }
  }

  return signals;
}

CallResult call_function(const std::string &device_id, uint32_t function_id,
                         const std::map<std::string, Value> &args) {
  if (fault_injection::is_device_unavailable(device_id)) {
    return bad("device unavailable (injected fault)");
  }

  const int64_t latency_ms = fault_injection::get_call_latency(device_id);
  if (latency_ms > 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(latency_ms));
  }

  const std::string function_id_str = std::to_string(function_id);
  if (fault_injection::should_call_fail(device_id, function_id_str)) {
    return bad("function call failed (injected fault)");
  }

  if (device_id == chaos_control::kDeviceId) {
    return chaos_control::call_function(function_id, args);
  }

  if (!anolis_provider_sim::DeviceFactory::is_config_loaded() ||
      !anolis_provider_sim::DeviceFactory::is_device_registered(device_id)) {
    return nf("unknown device_id: " + device_id);
  }

  const std::string device_type =
      anolis_provider_sim::DeviceFactory::get_device_type(device_id);
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
