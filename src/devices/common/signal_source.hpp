#pragma once

/**
 * @file signal_source.hpp
 * @brief Coordination interface shared by device code and simulation backends.
 */

#include <optional>
#include <string>

namespace sim_coordination {

/**
 * @brief Abstract interface for signal sources (actuators) and sinks (sensors).
 *
 * This interface provides the abstraction boundary between the physics
 * simulation engine and the device provider implementation. The physics engine
 * depends ONLY on this interface, not on ADPP protocol types or specific device
 * implementations.
 *
 * Threading:
 * Implementations must be thread-safe because reads and writes can arrive from
 * the physics ticker thread and ADPP request handler threads.
 */
class ISignalSource {
public:
  virtual ~ISignalSource() = default;

  /**
   * @brief Read a signal value by path.
   *
   * @param path Full signal path in format "device_id/signal_id" or
   * "model_id/output_id"
   * @return Signal value if available, nullopt if signal doesn't exist or is
   * unavailable
   *
   * Threading:
   * Safe for concurrent calls from multiple threads.
   */
  virtual std::optional<double> read_signal(const std::string &path) = 0;

  /**
   * @brief Write a signal value by path.
   *
   * This is called by the physics engine to update computed sensor values.
   * The implementation is responsible for routing this value to the appropriate
   * device state or cache.
   *
   * @param path Full signal path in format "device_id/signal_id"
   * @param value Computed signal value from physics
   *
   * Threading:
   * Safe for concurrent calls from multiple threads.
   */
  virtual void write_signal(const std::string &path, double value) = 0;
};

} // namespace sim_coordination
