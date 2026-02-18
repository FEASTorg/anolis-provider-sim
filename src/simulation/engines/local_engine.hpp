#pragma once

#include "simulation/simulation_engine.hpp"
#include <chrono>

namespace sim_engine {

class LocalEngine : public SimulationEngine {
public:
  void initialize(const std::string &config_path) override;
  void register_devices(const std::vector<std::string> &device_ids) override;

  TickResult tick(const std::map<std::string, double> &actuators) override;

private:
  std::chrono::steady_clock::time_point last_update_ =
      std::chrono::steady_clock::now();
};

} // namespace sim_engine
