#pragma once

#include <grpcpp/grpcpp.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "fluxgraph.grpc.pb.h"

namespace sim_flux {

using CommandValue = std::variant<double, int64_t, bool, std::string>;

// Rule command emitted by FluxGraph for provider-owned devices.
struct Command {
  std::string device_name;
  std::string function_name;
  std::map<std::string, CommandValue> args;
};

class FluxGraphClient {
public:
  explicit FluxGraphClient(const std::string &server_address);
  ~FluxGraphClient();

  void load_config(const std::string &yaml_path);
  void load_config_content(const std::string &yaml_content);

  void register_provider(const std::string &provider_id,
                         const std::vector<std::string> &device_ids);

  bool update_signals(const std::map<std::string, double> &signals,
                      const std::string &default_unit = "dimensionless");

  struct SignalInfo {
    double value = 0.0;
    std::string unit;
    bool physics_driven = false;
  };

  std::optional<SignalInfo> read_signal(const std::string &path);
  std::optional<double> read_signal_value(const std::string &path);

  std::vector<Command> drain_commands();

  void reset();

  bool is_connected() const;
  double current_sim_time() const { return sim_time_sec_; }
  bool last_tick_occurred() const { return last_tick_occurred_; }

private:
  void check_connection();
  Command convert_command(const fluxgraph::rpc::Command &pb_cmd);
  void invalidate_cache();

  std::shared_ptr<grpc::Channel> channel_;
  std::unique_ptr<fluxgraph::rpc::FluxGraph::Stub> stub_;

  std::string session_id_;
  bool registered_ = false;

  double sim_time_sec_ = 0.0;
  bool last_tick_occurred_ = false;

  std::vector<Command> pending_commands_;

  std::map<std::string, double> signal_cache_;
  std::map<std::string, std::string> unit_cache_;
  std::map<std::string, bool> physics_driven_cache_;
};

} // namespace sim_flux
