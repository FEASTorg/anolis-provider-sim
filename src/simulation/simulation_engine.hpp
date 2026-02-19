#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace sim_engine {

using CommandValue = std::variant<double, int64_t, bool, std::string>;

// Command emitted by simulation (provider executes it through device APIs).
struct Command {
  std::string device_id;
  std::string function_name;
  std::map<std::string, CommandValue> args;
};

// Unified tick output for all simulation backends.
struct TickResult {
  bool success = false;
  std::map<std::string, double> sensors;
  std::vector<Command> commands;
};

class SimulationEngine {
public:
  virtual ~SimulationEngine() = default;

  virtual void initialize(const std::string &config_path) = 0;
  virtual void set_provider_id(const std::string &provider_id) {
    (void)provider_id;
  }
  virtual void register_devices(const std::vector<std::string> &device_ids) = 0;
  virtual TickResult tick(const std::map<std::string, double> &actuators) = 0;

  virtual std::vector<std::string> list_signals() { return {}; }
};

} // namespace sim_engine
