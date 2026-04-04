#pragma once

/**
 * @file signal_registry.hpp
 * @brief Thread-safe registry that mediates between device state and simulation outputs.
 */

#include "signal_source.hpp"
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>

namespace sim_coordination {

/**
 * @brief Thread-safe signal registry for coordinating physics and device state.
 *
 * SignalRegistry implements ISignalSource and serves as the coordination layer
 * between the physics simulation engine and device implementations. It
 * maintains a cache of physics-driven signal values and delegates reads to
 * actual devices for non-physics signals.
 *
 * Threading:
 * All operations are protected by an internal mutex. The registry is safe for
 * concurrent access from the physics ticker thread and ADPP request handlers.
 */
class SignalRegistry : public ISignalSource {
public:
  SignalRegistry() = default;
  ~SignalRegistry() override = default;

  // Disable copy/move (singleton-like usage pattern)
  SignalRegistry(const SignalRegistry &) = delete;
  SignalRegistry &operator=(const SignalRegistry &) = delete;

  // ---- ISignalSource Implementation ----

  /**
   * @brief Read a signal value.
   *
   * If the signal is physics-driven, returns the cached value from physics.
   * Otherwise, delegates to the device_reader callback to get actual device
   * state.
   *
   * @param path Full signal path ("device_id/signal_id")
   * @return Signal value if available, nullopt otherwise
   */
  std::optional<double> read_signal(const std::string &path) override;

  /**
   * @brief Write a signal value (physics -> device).
   *
   * Marks the signal as physics-driven and caches the value. Subsequent reads
   * will return this cached value instead of querying the device.
   *
   * @param path Full signal path ("device_id/signal_id")
   * @param value Computed value from physics engine
   */
  void write_signal(const std::string &path, double value) override;

  /**
   * @brief Check if a signal is being driven by physics.
   *
   * @param path Full signal path
   * @return true if physics has written to this signal, false otherwise
   */
  bool is_physics_driven(const std::string &path) const;

  /**
   * @brief Explicitly mark a signal as physics-driven.
   *
   * Used during initialization to pre-populate physics_driven_signals_ from
   * graph configuration before the physics ticker starts writing values.
   *
   * @param path Full signal path to mark
   */
  void mark_physics_driven(const std::string &path);

  /**
   * @brief Clear all physics overrides and return to device state.
   *
   * Useful for mode transitions (physics -> inert) or testing.
   */
  void clear_physics_overrides();

  /**
   * @brief Set the callback for reading actual device state.
   *
   * This callback is invoked when read_signal() is called for a non-physics
   * signal. It should access the actual device state and return the value.
   *
   * @param reader Callback function: (path) -> optional<double>
   *
   * Example:
   *   registry.set_device_reader([](const std::string& path) {
   *       auto pos = path.find('/');
   *       std::string device_id = path.substr(0, pos);
   *       std::string signal_id = path.substr(pos + 1);
   *       return read_device_signal_internal(device_id, signal_id);
   *   });
   */
  void set_device_reader(
      std::function<std::optional<double>(const std::string &)> reader);

  /**
   * @brief Get all physics-driven signal paths.
   * @return Set of paths that physics has written to
   */
  std::set<std::string> get_physics_driven_signals() const;

  /**
   * @brief Get the current cached value for a physics-driven signal.
   * @param path Full signal path
   * @return Cached value if physics-driven, nullopt otherwise
   */
  std::optional<double> get_cached_value(const std::string &path) const;

private:
  std::map<std::string, double> signal_cache_;
  std::set<std::string> physics_driven_signals_;
  std::function<std::optional<double>(const std::string &)> device_reader_;
  mutable std::mutex registry_mutex_;
};

} // namespace sim_coordination
