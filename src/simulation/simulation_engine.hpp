#pragma once

/**
 * @file simulation_engine.hpp
 * @brief Backend-agnostic simulation engine contract used by the sim provider ticker.
 */

#include <cstdint>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace sim_engine {

/**
 * @brief Variant type used for simulation-emitted command arguments.
 */
using CommandValue = std::variant<double, int64_t, bool, std::string>;

/**
 * @brief Command emitted by a simulation backend for provider-side execution.
 */
struct Command {
  std::string device_id;
  std::string function_name;
  std::map<std::string, CommandValue> args;
};

/**
 * @brief Unified result returned from one simulation tick.
 */
struct TickResult {
  bool success = false;
  std::map<std::string, double> sensors;
  std::vector<Command> commands;
};

/**
 * @brief Abstract simulation backend interface used by the ticker thread.
 *
 * Implementations receive actuator snapshots from the provider and return the
 * sensor updates and optional commands that should be applied during the same
 * tick.
 */
class SimulationEngine {
public:
  virtual ~SimulationEngine() = default;

  /** @brief Initialize the backend from its mode-specific config path. */
  virtual void initialize(const std::string &config_path) = 0;

  /** @brief Publish the provider identity when a backend needs it for routing. */
  virtual void set_provider_id(const std::string &provider_id) {
    (void)provider_id;
  }

  /** @brief Register the active device IDs that the backend may address. */
  virtual void register_devices(const std::vector<std::string> &device_ids) = 0;

  /**
   * @brief Advance the simulation by one provider tick.
   *
   * @param actuators Point-in-time actuator snapshot collected from devices
   * @return Sensor updates and optional commands for the current tick
   */
  virtual TickResult tick(const std::map<std::string, double> &actuators) = 0;

  /** @brief List signal paths produced by the backend, when known. */
  virtual std::vector<std::string> list_signals() { return {}; }
};

} // namespace sim_engine
