#pragma once

#include "simulation/adapters/protocol_adapter.hpp"

#ifdef HAVE_FLUXGRAPH

#include "fluxgraph_client.hpp"

#include <memory>
#include <set>

namespace sim_adapters {

class FluxGraphAdapter : public ProtocolAdapter {
public:
  explicit FluxGraphAdapter(const std::string &server_address);

  void connect(const std::string &address) override;
  void load_config(const std::string &config_path) override;
  void register_provider(const std::string &provider_name,
                         const std::vector<std::string> &device_ids) override;

  bool update_signals(const std::map<std::string, double> &actuators,
                      const std::string &unit,
                      std::chrono::milliseconds timeout) override;

  std::map<std::string, double>
  read_signals(const std::vector<std::string> &signal_paths) override;

  std::vector<sim_engine::Command> drain_commands() override;

  std::vector<std::string> list_signals() override;

private:
  std::unique_ptr<sim_flux::FluxGraphClient> client_;
  std::vector<std::string> output_paths_;
  std::set<std::string> registered_device_ids_;
};

} // namespace sim_adapters

#endif // HAVE_FLUXGRAPH
