#ifdef HAVE_FLUXGRAPH

#include "fluxgraph_adapter.hpp"

#include "../config.hpp"
#include "../config_translator.hpp"

#include <set>
#include <utility>

namespace sim_adapters {

FluxGraphAdapter::FluxGraphAdapter(const std::string &server_address) {
  client_ = std::make_unique<sim_flux::FluxGraphClient>(server_address);
}

void FluxGraphAdapter::connect(const std::string &) {
  // Already connected in constructor.
}

void FluxGraphAdapter::load_config(const std::string &config_path) {
  std::string fluxgraph_yaml =
      sim_config::translate_to_fluxgraph_format(config_path);
  client_->load_config_content(fluxgraph_yaml);

  output_paths_.clear();
  std::set<std::string> dedup;
  const auto cfg = anolis_provider_sim::load_physics_config(config_path);
  for (const auto &edge : cfg.signal_graph) {
    if (dedup.insert(edge.target).second) {
      output_paths_.push_back(edge.target);
    }
  }
}

void FluxGraphAdapter::register_provider(
    const std::string &provider_name,
    const std::vector<std::string> &device_ids) {
  registered_device_ids_.clear();
  for (const auto &id : device_ids) {
    registered_device_ids_.insert(id);
  }
  client_->register_provider(provider_name, device_ids);
}

bool FluxGraphAdapter::update_signals(
    const std::map<std::string, double> &actuators,
    const std::string &unit,
    std::chrono::milliseconds timeout) {
  return client_->update_signals(actuators, unit, timeout);
}

std::map<std::string, double> FluxGraphAdapter::read_signals(
    const std::vector<std::string> &signal_paths) {
  std::map<std::string, double> sensors;

  std::vector<std::string> paths_to_read = signal_paths;
  if (paths_to_read.empty()) {
    for (const auto &path : output_paths_) {
      const auto slash = path.find('/');
      if (slash == std::string::npos) {
        continue;
      }
      const auto device_id = path.substr(0, slash);
      if (registered_device_ids_.count(device_id) > 0) {
        paths_to_read.push_back(path);
      }
    }
  }

  for (const auto &path : paths_to_read) {
    if (auto value = client_->read_signal_value(path)) {
      sensors[path] = *value;
    }
  }

  return sensors;
}

std::vector<sim_engine::Command> FluxGraphAdapter::drain_commands() {
  std::vector<sim_engine::Command> out;
  for (const auto &cmd : client_->drain_commands()) {
    sim_engine::Command mapped;
    mapped.device_id = cmd.device_name;
    mapped.function_name = cmd.function_name;
    mapped.args = cmd.args;
    out.push_back(std::move(mapped));
  }
  return out;
}

} // namespace sim_adapters

#endif // HAVE_FLUXGRAPH
