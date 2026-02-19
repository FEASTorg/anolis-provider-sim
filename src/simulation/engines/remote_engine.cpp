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

void RemoteEngine::set_provider_id(const std::string &provider_id) {
  provider_id_ = provider_id.empty() ? "provider-sim" : provider_id;
}

void RemoteEngine::register_devices(const std::vector<std::string> &device_ids) {
  device_ids_ = device_ids;
  adapter_->register_provider(provider_id_, device_ids);
}

TickResult RemoteEngine::tick(const std::map<std::string, double> &actuators) {
  try {
    auto timeout = std::chrono::milliseconds(2000);
    if (tick_rate_hz_ > 0.0) {
      // Multi-provider: use 20x tick period to allow barrier synchronization
      // At 10Hz: 20 * (1/10) = 2 seconds
      const auto timeout_ms = static_cast<long long>((20.0 / tick_rate_hz_) * 1000.0);
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
