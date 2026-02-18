#ifdef HAVE_FLUXGRAPH

#include "fluxgraph_client.hpp"

#include <chrono>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace sim_flux {

FluxGraphClient::FluxGraphClient(const std::string &server_address) {
  channel_ =
      grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials());
  stub_ = fluxgraph::rpc::FluxGraph::NewStub(channel_);

  const auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);
  if (!channel_->WaitForConnected(deadline)) {
    throw std::runtime_error("Failed to connect to FluxGraph server at " +
                             server_address);
  }
}

FluxGraphClient::~FluxGraphClient() = default;

void FluxGraphClient::load_config(const std::string &yaml_path) {
  std::ifstream file(yaml_path);
  if (!file) {
    throw std::runtime_error("Failed to open config file: " + yaml_path);
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  load_config_content(buffer.str());
}

void FluxGraphClient::load_config_content(const std::string &yaml_content) {
  check_connection();

  fluxgraph::rpc::ConfigRequest req;
  req.set_config_content(yaml_content);
  req.set_format("yaml");

  fluxgraph::rpc::ConfigResponse res;
  grpc::ClientContext ctx;
  const auto status = stub_->LoadConfig(&ctx, req, &res);
  if (!status.ok()) {
    throw std::runtime_error("LoadConfig RPC failed: " + status.error_message());
  }
  if (!res.success()) {
    throw std::runtime_error("LoadConfig failed: " + res.error_message());
  }

  // Config reload invalidates provider registration and all local caches.
  registered_ = false;
  session_id_.clear();
  sim_time_sec_ = 0.0;
  last_tick_occurred_ = false;
  pending_commands_.clear();
  invalidate_cache();
}

void FluxGraphClient::register_provider(const std::string &provider_id,
                                        const std::vector<std::string> &device_ids) {
  check_connection();

  fluxgraph::rpc::ProviderRegistration req;
  req.set_provider_id(provider_id);
  for (const auto &id : device_ids) {
    req.add_device_ids(id);
  }

  fluxgraph::rpc::ProviderRegistrationResponse res;
  grpc::ClientContext ctx;
  const auto status = stub_->RegisterProvider(&ctx, req, &res);
  if (!status.ok()) {
    throw std::runtime_error("RegisterProvider RPC failed: " +
                             status.error_message());
  }
  if (!res.success()) {
    throw std::runtime_error("RegisterProvider failed: " + res.error_message());
  }

  session_id_ = res.session_id();
  registered_ = true;
}

bool FluxGraphClient::update_signals(const std::map<std::string, double> &signals,
                                     const std::string &default_unit,
                                     std::chrono::milliseconds timeout) {
  if (!registered_) {
    throw std::runtime_error("FluxGraph provider session not registered");
  }

  fluxgraph::rpc::SignalUpdates req;
  req.set_session_id(session_id_);
  for (const auto &[path, value] : signals) {
    auto *sig = req.add_signals();
    sig->set_path(path);
    sig->set_value(value);
    sig->set_unit(default_unit);
  }

  fluxgraph::rpc::TickResponse res;
  grpc::ClientContext ctx;
  if (timeout.count() > 0) {
    const auto deadline = std::chrono::system_clock::now() + timeout;
    ctx.set_deadline(deadline);
  }
  const auto status = stub_->UpdateSignals(&ctx, req, &res);
  if (!status.ok()) {
    throw std::runtime_error("UpdateSignals RPC failed: " +
                             status.error_message());
  }

  last_tick_occurred_ = res.tick_occurred();
  sim_time_sec_ = res.sim_time_sec();

  pending_commands_.clear();
  pending_commands_.reserve(static_cast<size_t>(res.commands_size()));
  for (const auto &pb_cmd : res.commands()) {
    pending_commands_.push_back(convert_command(pb_cmd));
  }

  if (last_tick_occurred_) {
    invalidate_cache();
  }

  return last_tick_occurred_;
}

std::optional<FluxGraphClient::SignalInfo>
FluxGraphClient::read_signal(const std::string &path) {
  auto it = signal_cache_.find(path);
  if (it != signal_cache_.end()) {
    SignalInfo info;
    info.value = it->second;
    info.unit = unit_cache_[path];
    info.physics_driven = physics_driven_cache_[path];
    return info;
  }

  fluxgraph::rpc::SignalRequest req;
  req.add_paths(path);

  fluxgraph::rpc::SignalResponse res;
  grpc::ClientContext ctx;
  const auto status = stub_->ReadSignals(&ctx, req, &res);
  if (!status.ok() || res.signals_size() != 1) {
    return std::nullopt;
  }

  const auto &sig = res.signals(0);
  signal_cache_[path] = sig.value();
  unit_cache_[path] = sig.unit();
  physics_driven_cache_[path] = sig.physics_driven();

  SignalInfo info;
  info.value = sig.value();
  info.unit = sig.unit();
  info.physics_driven = sig.physics_driven();
  return info;
}

std::optional<double> FluxGraphClient::read_signal_value(const std::string &path) {
  auto info = read_signal(path);
  if (!info) {
    return std::nullopt;
  }
  return info->value;
}

std::vector<Command> FluxGraphClient::drain_commands() {
  std::vector<Command> out;
  out.swap(pending_commands_);
  return out;
}

void FluxGraphClient::reset() {
  fluxgraph::rpc::ResetRequest req;
  fluxgraph::rpc::ResetResponse res;
  grpc::ClientContext ctx;
  const auto status = stub_->Reset(&ctx, req, &res);
  if (!status.ok()) {
    throw std::runtime_error("Reset RPC failed: " + status.error_message());
  }
  if (!res.success()) {
    throw std::runtime_error("Reset failed: " + res.error_message());
  }

  sim_time_sec_ = 0.0;
  last_tick_occurred_ = false;
  pending_commands_.clear();
  invalidate_cache();
}

bool FluxGraphClient::is_connected() const {
  auto state = channel_->GetState(false);
  return state == GRPC_CHANNEL_READY || state == GRPC_CHANNEL_IDLE ||
         state == GRPC_CHANNEL_CONNECTING;
}

void FluxGraphClient::check_connection() {
  const auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(2);
  if (!channel_->WaitForConnected(deadline)) {
    throw std::runtime_error("FluxGraph server connection is not ready");
  }
}

Command FluxGraphClient::convert_command(const fluxgraph::rpc::Command &pb_cmd) {
  Command cmd;
  cmd.device_name = pb_cmd.device();
  cmd.function_name = pb_cmd.function();

  for (const auto &[key, arg] : pb_cmd.args()) {
    switch (arg.value_case()) {
    case fluxgraph::rpc::CommandArg::kDoubleVal:
      cmd.args[key] = arg.double_val();
      break;
    case fluxgraph::rpc::CommandArg::kIntVal:
      cmd.args[key] = static_cast<int64_t>(arg.int_val());
      break;
    case fluxgraph::rpc::CommandArg::kBoolVal:
      cmd.args[key] = arg.bool_val();
      break;
    case fluxgraph::rpc::CommandArg::kStringVal:
      cmd.args[key] = arg.string_val();
      break;
    case fluxgraph::rpc::CommandArg::VALUE_NOT_SET:
      break;
    }
  }

  return cmd;
}

void FluxGraphClient::invalidate_cache() {
  signal_cache_.clear();
  unit_cache_.clear();
  physics_driven_cache_.clear();
}

} // namespace sim_flux

#endif // HAVE_FLUXGRAPH
