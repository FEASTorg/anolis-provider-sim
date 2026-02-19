#ifdef HAVE_FLUXGRAPH

#include "fluxgraph_client.hpp"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace sim_flux {

namespace {

uint64_t fnv1a_64(const std::string &data) {
  uint64_t hash = 0xcbf29ce484222325ULL;
  for (const unsigned char c : data) {
    hash ^= static_cast<uint64_t>(c);
    hash *= 0x100000001b3ULL;
  }
  return hash;
}

std::string to_hex_string(uint64_t value) {
  char buf[17];
  std::snprintf(buf, sizeof(buf), "%016llx",
                static_cast<unsigned long long>(value));
  return std::string(buf);
}

constexpr std::chrono::milliseconds kDefaultRpcTimeout{2000};
constexpr std::chrono::milliseconds kConfigRpcTimeout{5000};
constexpr int kMaxRpcAttempts = 2;

void set_rpc_deadline(grpc::ClientContext &ctx,
                      std::chrono::milliseconds timeout) {
  if (timeout.count() <= 0) {
    timeout = kDefaultRpcTimeout;
  }
  ctx.set_deadline(std::chrono::system_clock::now() + timeout);
}

bool is_retryable_status(const grpc::Status &status) {
  switch (status.error_code()) {
  case grpc::StatusCode::UNAVAILABLE:
  case grpc::StatusCode::DEADLINE_EXCEEDED:
  case grpc::StatusCode::RESOURCE_EXHAUSTED:
  case grpc::StatusCode::ABORTED:
    return true;
  default:
    return false;
  }
}

} // namespace

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

FluxGraphClient::~FluxGraphClient() { unregister_provider_noexcept(); }

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
  const std::string hash_input = std::string("yaml\n") + yaml_content;
  req.set_config_hash(to_hex_string(fnv1a_64(hash_input)));

  fluxgraph::rpc::ConfigResponse res;
  grpc::Status status;
  bool rpc_ok = false;
  for (int attempt = 0; attempt < kMaxRpcAttempts; ++attempt) {
    grpc::ClientContext ctx;
    set_rpc_deadline(ctx, kConfigRpcTimeout);
    status = stub_->LoadConfig(&ctx, req, &res);
    if (status.ok()) {
      rpc_ok = true;
      break;
    }
    if (!is_retryable_status(status) || attempt + 1 >= kMaxRpcAttempts) {
      break;
    }
  }
  if (!rpc_ok) {
    throw std::runtime_error("LoadConfig RPC failed: code=" +
                             std::to_string(static_cast<int>(status.error_code())) +
                             " message=" + status.error_message());
  }
  if (!res.success()) {
    throw std::runtime_error("LoadConfig failed: " + res.error_message());
  }

  // Only a changed config invalidates provider registration/session state.
  if (res.config_changed()) {
    registered_ = false;
    session_id_.clear();
    sim_time_sec_ = 0.0;
    last_tick_occurred_ = false;
    pending_commands_.clear();
    invalidate_cache();
  }
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
  grpc::Status status;
  bool rpc_ok = false;
  for (int attempt = 0; attempt < kMaxRpcAttempts; ++attempt) {
    grpc::ClientContext ctx;
    set_rpc_deadline(ctx, kDefaultRpcTimeout);
    status = stub_->RegisterProvider(&ctx, req, &res);
    if (status.ok()) {
      rpc_ok = true;
      break;
    }
    if (!is_retryable_status(status) || attempt + 1 >= kMaxRpcAttempts) {
      break;
    }
  }
  if (!rpc_ok) {
    throw std::runtime_error("RegisterProvider RPC failed: code=" +
                             std::to_string(static_cast<int>(status.error_code())) +
                             " message=" + status.error_message());
  }
  if (!res.success()) {
    throw std::runtime_error("RegisterProvider failed: " + res.error_message());
  }

  provider_id_ = provider_id;
  registered_device_ids_ = device_ids;
  session_id_ = res.session_id();
  registered_ = true;
}

bool FluxGraphClient::update_signals(const std::map<std::string, double> &signals,
                                     const std::string &default_unit,
                                     std::chrono::milliseconds timeout) {
  if (!registered_) {
    throw std::runtime_error("FluxGraph provider session not registered");
  }

  fluxgraph::rpc::SignalUpdates req_template;
  for (const auto &[path, value] : signals) {
    auto *sig = req_template.add_signals();
    sig->set_path(path);
    sig->set_value(value);
    sig->set_unit(default_unit);
  }

  const std::chrono::milliseconds rpc_timeout =
      (timeout.count() > 0) ? timeout : kDefaultRpcTimeout;

  bool retried_after_unauth = false;
  grpc::Status status;
  fluxgraph::rpc::TickResponse res;

  for (int attempt = 0; attempt < kMaxRpcAttempts; ++attempt) {
    fluxgraph::rpc::SignalUpdates req = req_template;
    req.set_session_id(session_id_);

    grpc::ClientContext ctx;
    set_rpc_deadline(ctx, rpc_timeout);
    status = stub_->UpdateSignals(&ctx, req, &res);
    if (status.ok()) {
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

    if (status.error_code() == grpc::StatusCode::UNAUTHENTICATED &&
        !retried_after_unauth) {
      registered_ = false;
      session_id_.clear();
      retried_after_unauth = true;
      if (try_reregister_session()) {
        continue;
      }
      break;
    }

    if (is_retryable_status(status) && attempt + 1 < kMaxRpcAttempts) {
      continue;
    }

    break;
  }

  throw std::runtime_error("UpdateSignals RPC failed: code=" +
                           std::to_string(static_cast<int>(status.error_code())) +
                           " message=" + status.error_message());
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

  bool retried_after_unauth = false;
  grpc::Status status;
  fluxgraph::rpc::SignalResponse res;

  for (int attempt = 0; attempt < kMaxRpcAttempts; ++attempt) {
    grpc::ClientContext ctx;
    set_rpc_deadline(ctx, kDefaultRpcTimeout);
    status = stub_->ReadSignals(&ctx, req, &res);
    if (status.ok()) {
      break;
    }

    if (status.error_code() == grpc::StatusCode::UNAUTHENTICATED &&
        !retried_after_unauth && registered_) {
      retried_after_unauth = true;
      if (try_reregister_session()) {
        continue;
      }
    }

    if (!is_retryable_status(status) || attempt + 1 >= kMaxRpcAttempts) {
      return std::nullopt;
    }
  }

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
  grpc::Status status;
  bool rpc_ok = false;
  for (int attempt = 0; attempt < kMaxRpcAttempts; ++attempt) {
    grpc::ClientContext ctx;
    set_rpc_deadline(ctx, kDefaultRpcTimeout);
    status = stub_->Reset(&ctx, req, &res);
    if (status.ok()) {
      rpc_ok = true;
      break;
    }
    if (!is_retryable_status(status) || attempt + 1 >= kMaxRpcAttempts) {
      break;
    }
  }
  if (!rpc_ok) {
    throw std::runtime_error("Reset RPC failed: code=" +
                             std::to_string(static_cast<int>(status.error_code())) +
                             " message=" + status.error_message());
  }
  if (!res.success()) {
    throw std::runtime_error("Reset failed: " + res.error_message());
  }

  sim_time_sec_ = 0.0;
  last_tick_occurred_ = false;
  pending_commands_.clear();
  invalidate_cache();
}

bool FluxGraphClient::try_reregister_session() {
  if (provider_id_.empty()) {
    return false;
  }

  fluxgraph::rpc::ProviderRegistration req;
  req.set_provider_id(provider_id_);
  for (const auto &id : registered_device_ids_) {
    req.add_device_ids(id);
  }

  fluxgraph::rpc::ProviderRegistrationResponse res;
  grpc::Status status;
  for (int attempt = 0; attempt < kMaxRpcAttempts; ++attempt) {
    grpc::ClientContext ctx;
    set_rpc_deadline(ctx, kDefaultRpcTimeout);
    status = stub_->RegisterProvider(&ctx, req, &res);
    if (status.ok()) {
      if (!res.success()) {
        return false;
      }
      session_id_ = res.session_id();
      registered_ = true;
      return true;
    }

    if (!is_retryable_status(status) || attempt + 1 >= kMaxRpcAttempts) {
      break;
    }
  }

  return false;
}

void FluxGraphClient::unregister_provider_noexcept() {
  if (!registered_ || session_id_.empty()) {
    return;
  }

  fluxgraph::rpc::UnregisterRequest req;
  req.set_session_id(session_id_);
  fluxgraph::rpc::UnregisterResponse res;
  grpc::ClientContext ctx;
  set_rpc_deadline(ctx, kDefaultRpcTimeout);
  (void)stub_->UnregisterProvider(&ctx, req, &res);

  registered_ = false;
  session_id_.clear();
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
