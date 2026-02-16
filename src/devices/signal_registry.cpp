#include "signal_registry.hpp"
#include <iostream>

namespace sim_coordination {

std::optional<double> SignalRegistry::read_signal(const std::string &path) {
  // Capture device_reader and check physics-driven status (with mutex)
  std::function<std::optional<double>(const std::string &)> reader;
  {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    if (physics_driven_signals_.count(path)) {
      // Return cached physics value
      auto it = signal_cache_.find(path);
      if (it != signal_cache_.end()) {
        return it->second;
      }
      // Physics marked as driven but hasn't written yet
      return std::nullopt;
    }

    // Capture device_reader for call outside mutex
    reader = device_reader_;
  }
  // Mutex released here before calling device_reader

  // Not physics-driven, delegate to device reader if available
  // CRITICAL: Call device_reader WITHOUT holding mutex to avoid deadlock
  // (device_reader may call back into registry methods like is_physics_driven)
  if (reader) {
    return reader(path);
  }

  // No device reader configured
  return std::nullopt;
}

void SignalRegistry::write_signal(const std::string &path, double value) {
  std::lock_guard<std::mutex> lock(registry_mutex_);

  // Mark as physics-driven (idempotent)
  physics_driven_signals_.insert(path);

  // Update cached value
  signal_cache_[path] = value;
}

bool SignalRegistry::is_physics_driven(const std::string &path) const {
  std::lock_guard<std::mutex> lock(registry_mutex_);
  return physics_driven_signals_.count(path) > 0;
}

void SignalRegistry::mark_physics_driven(const std::string &path) {
  std::lock_guard<std::mutex> lock(registry_mutex_);
  physics_driven_signals_.insert(path);
}

void SignalRegistry::clear_physics_overrides() {
  std::lock_guard<std::mutex> lock(registry_mutex_);
  physics_driven_signals_.clear();
  signal_cache_.clear();
}

void SignalRegistry::set_device_reader(
    std::function<std::optional<double>(const std::string &)> reader) {
  std::lock_guard<std::mutex> lock(registry_mutex_);
  device_reader_ = std::move(reader);
}

std::set<std::string> SignalRegistry::get_physics_driven_signals() const {
  std::lock_guard<std::mutex> lock(registry_mutex_);
  return physics_driven_signals_;
}

std::optional<double>
SignalRegistry::get_cached_value(const std::string &path) const {
  std::lock_guard<std::mutex> lock(registry_mutex_);
  auto it = signal_cache_.find(path);
  if (it != signal_cache_.end()) {
    return it->second;
  }
  return std::nullopt;
}

} // namespace sim_coordination
