#include "remote_engine.hpp"

#include <chrono>
#include <exception>
#include <iostream>

namespace sim_engine {

RemoteEngine::RemoteEngine(
    std::unique_ptr<sim_adapters::ProtocolAdapter> adapter,
    double tick_rate_hz)
    : adapter_(std::move(adapter)), tick_rate_hz_(tick_rate_hz) {}

void RemoteEngine::initialize(const std::string &config_path) {
  adapter_->load_config(config_path);
}

void RemoteEngine::register_devices(const std::vector<std::string> &device_ids) {
  device_ids_ = device_ids;
  // Phase 26: fixed single-instance provider identity.
  adapter_->register_provider("provider-sim", device_ids);
}

TickResult RemoteEngine::tick(const std::map<std::string, double> &actuators) {
  try {
    auto timeout = std::chrono::milliseconds(200);
    if (tick_rate_hz_ > 0.0) {
      const auto timeout_ms = static_cast<long long>((2.0 / tick_rate_hz_) * 1000.0);
      timeout = std::chrono::milliseconds(timeout_ms > 0 ? timeout_ms : 1);
    }

    const bool success =
        adapter_->update_signals(actuators, "dimensionless", timeout);
    if (!success) {
      std::cerr << "[RemoteEngine] Tick failed or timed out\n";
      return TickResult{false, {}, {}};
    }

    std::vector<std::string> sensor_paths;
    auto sensors = adapter_->read_signals(sensor_paths);
    auto commands = adapter_->drain_commands();

    return TickResult{true, sensors, commands};
  } catch (const std::exception &e) {
    std::cerr << "[RemoteEngine] Tick error: " << e.what() << "\n";
    return TickResult{false, {}, {}};
  }
}

std::vector<std::string> RemoteEngine::list_signals() {
  return adapter_->list_signals();
}

} // namespace sim_engine
