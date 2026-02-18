#pragma once

#include "../simulation_engine.hpp"

namespace sim_engine {

class NullEngine : public SimulationEngine {
public:
  void initialize(const std::string &) override {}
  void register_devices(const std::vector<std::string> &) override {}

  TickResult tick(const std::map<std::string, double> &) override {
    // Inert mode never runs the ticker; keep null behavior explicit.
    return TickResult{false, {}, {}};
  }
};

} // namespace sim_engine
