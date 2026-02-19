#pragma once

#include "simulation/adapters/protocol_adapter.hpp"
#include "simulation/simulation_engine.hpp"

#include <memory>

namespace sim_engine {

class RemoteEngine : public SimulationEngine {
public:
  explicit RemoteEngine(std::unique_ptr<sim_adapters::ProtocolAdapter> adapter,
                        double tick_rate_hz = 10.0);

  void initialize(const std::string &config_path) override;
  void set_provider_id(const std::string &provider_id) override;
  void register_devices(const std::vector<std::string> &device_ids) override;

  TickResult tick(const std::map<std::string, double> &actuators) override;

  std::vector<std::string> list_signals() override;

private:
  std::unique_ptr<sim_adapters::ProtocolAdapter> adapter_;
  std::vector<std::string> device_ids_;
  double tick_rate_hz_;
  std::string provider_id_ = "provider-sim";
};

} // namespace sim_engine
