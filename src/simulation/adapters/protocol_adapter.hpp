#pragma once

#include "simulation/simulation_engine.hpp"

#include <chrono>
#include <map>
#include <string>
#include <vector>

namespace sim_adapters {

class ProtocolAdapter {
public:
  virtual ~ProtocolAdapter() = default;

  virtual void connect(const std::string &address) = 0;
  virtual void load_config(const std::string &config_path) = 0;
  virtual void
  register_provider(const std::string &provider_name,
                    const std::vector<std::string> &device_ids) = 0;

  virtual bool update_signals(const std::map<std::string, double> &actuators,
                              const std::string &unit,
                              std::chrono::milliseconds timeout) = 0;

  virtual std::map<std::string, double>
  read_signals(const std::vector<std::string> &signal_paths) = 0;

  virtual std::vector<sim_engine::Command> drain_commands() = 0;

  virtual std::vector<std::string> list_signals() { return {}; }
};

} // namespace sim_adapters
