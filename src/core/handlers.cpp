#include "core/handlers.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "core/health.hpp"
#include "core/transport/framed_stdio.hpp"
#include "devices/common/device_factory.hpp"
#include "devices/common/device_manager.hpp"

namespace handlers {

using anolis::deviceprovider::v1::CallRequest;
using anolis::deviceprovider::v1::DescribeDeviceRequest;
using anolis::deviceprovider::v1::GetHealthRequest;
using anolis::deviceprovider::v1::HelloRequest;
using anolis::deviceprovider::v1::ListDevicesRequest;
using anolis::deviceprovider::v1::ReadSignalsRequest;
using anolis::deviceprovider::v1::Status;
using anolis::deviceprovider::v1::WaitReadyRequest;

#ifndef ANOLIS_PROVIDER_SIM_VERSION
#define ANOLIS_PROVIDER_SIM_VERSION "0.0.0"
#endif

static inline void set_status_ok(anolis::deviceprovider::v1::Response &resp) {
  resp.mutable_status()->set_code(Status::CODE_OK);
  resp.mutable_status()->set_message("ok");
}

static inline void set_status(anolis::deviceprovider::v1::Response &resp,
                              Status::Code code, const std::string &msg) {
  resp.mutable_status()->set_code(code);
  resp.mutable_status()->set_message(msg);
}

void handle_hello(const HelloRequest &req,
                  anolis::deviceprovider::v1::Response &resp) {
  if (req.protocol_version() != "v1") {
    set_status(resp, Status::CODE_FAILED_PRECONDITION,
               "unsupported protocol_version; expected v1");
    return;
  }

  auto *hello = resp.mutable_hello();
  hello->set_protocol_version("v1");
  hello->set_provider_name("anolis-provider-sim");
  hello->set_provider_version(ANOLIS_PROVIDER_SIM_VERSION);

  (*hello->mutable_metadata())["transport"] = "stdio+uint32_le";
  (*hello->mutable_metadata())["max_frame_bytes"] =
      std::to_string(transport::kMaxFrameBytes);
  (*hello->mutable_metadata())["supports_wait_ready"] = "true";

  set_status_ok(resp);
}

void handle_list_devices(const ListDevicesRequest &req,
                         anolis::deviceprovider::v1::Response &resp) {
  auto *out = resp.mutable_list_devices();

  const auto devices = sim_devices::list_devices(req.include_health());
  for (const auto &d : devices) {
    *out->add_devices() = d;
  }

  if (req.include_health()) {
    const auto device_health = sim_health::make_list_devices_health(devices);
    for (const auto &health : device_health) {
      *out->add_device_health() = health;
    }
  }

  set_status_ok(resp);
}

void handle_describe_device(const DescribeDeviceRequest &req,
                            anolis::deviceprovider::v1::Response &resp) {
  if (req.device_id().empty()) {
    set_status(resp, Status::CODE_INVALID_ARGUMENT, "device_id is required");
    return;
  }

  const auto devices = sim_devices::list_devices(false);
  bool found = false;
  anolis::deviceprovider::v1::Device dev;
  for (const auto &d : devices) {
    if (d.device_id() == req.device_id()) {
      dev = d;
      found = true;
      break;
    }
  }

  if (!found) {
    set_status(resp, Status::CODE_NOT_FOUND, "unknown device_id");
    return;
  }

  auto *out = resp.mutable_describe_device();
  *out->mutable_device() = dev;
  *out->mutable_capabilities() = sim_devices::describe_device(req.device_id());

  set_status_ok(resp);
}

void handle_read_signals(const ReadSignalsRequest &req,
                         anolis::deviceprovider::v1::Response &resp) {
  if (req.device_id().empty()) {
    set_status(resp, Status::CODE_INVALID_ARGUMENT, "device_id is required");
    return;
  }

  std::vector<std::string> ids;
  ids.reserve(static_cast<size_t>(req.signal_ids_size()));
  for (const auto &s : req.signal_ids())
    ids.push_back(s);

  // Check if device exists before attempting to read signals
  // Special-case chaos_control (has no signals but is always available)
  if (req.device_id() != "chaos_control" &&
      (!anolis_provider_sim::DeviceFactory::is_config_loaded() ||
       !anolis_provider_sim::DeviceFactory::is_device_registered(
           req.device_id()))) {
    set_status(resp, Status::CODE_NOT_FOUND,
               "unknown device_id: " + req.device_id());
    return;
  }

  const auto values = sim_devices::read_signals(req.device_id(), ids);

  if (!ids.empty() && values.empty()) {
    // v1 policy: if explicit ids were requested and none are returned, treat as
    // NOT_FOUND
    set_status(resp, Status::CODE_NOT_FOUND,
               "no requested signals found for device");
    return;
  }

  auto *out = resp.mutable_read_signals();
  out->set_device_id(req.device_id());
  for (const auto &v : values) {
    *out->add_values() = v;
  }

  set_status_ok(resp);
}

void handle_call(const CallRequest &req,
                 anolis::deviceprovider::v1::Response &resp) {
  if (req.device_id().empty()) {
    set_status(resp, Status::CODE_INVALID_ARGUMENT, "device_id is required");
    return;
  }
  if (req.function_id() == 0 && req.function_name().empty()) {
    set_status(resp, Status::CODE_INVALID_ARGUMENT,
               "function_id or function_name is required");
    return;
  }

  uint32_t resolved_function_id = req.function_id();

  if (req.function_id() == 0) {
    const auto function_id =
        sim_devices::resolve_function_id(req.device_id(), req.function_name());
    if (!function_id.has_value()) {
      set_status(resp, Status::CODE_NOT_FOUND,
                 "unknown function_name '" + req.function_name() +
                     "' for device_id '" + req.device_id() + "'");
      return;
    }
    resolved_function_id = *function_id;
  } else if (!req.function_name().empty()) {
    const auto by_name =
        sim_devices::resolve_function_id(req.device_id(), req.function_name());
    if (by_name.has_value() && *by_name != req.function_id()) {
      set_status(resp, Status::CODE_INVALID_ARGUMENT,
                 "function_id/function_name mismatch");
      return;
    }
  }

  std::map<std::string, anolis::deviceprovider::v1::Value> args;
  for (const auto &kv : req.args()) {
    args.emplace(kv.first, kv.second);
  }

  const auto result =
      sim_devices::call_function(req.device_id(), resolved_function_id, args);
  if (result.code != Status::CODE_OK) {
    set_status(resp, result.code, result.message);
    return;
  }

  auto *out = resp.mutable_call();
  out->set_device_id(req.device_id());
  // v1 sim: no results and no operation_id
  set_status_ok(resp);
}

void handle_get_health(const GetHealthRequest & /*req*/,
                       anolis::deviceprovider::v1::Response &resp) {
  const auto init_report =
      anolis_provider_sim::DeviceFactory::get_initialization_report();
  auto *out = resp.mutable_get_health();
  *out->mutable_provider() = sim_health::make_provider_health(init_report);

  const auto device_health = sim_health::make_get_health_devices(init_report);
  for (const auto &health : device_health) {
    *out->add_devices() = health;
  }

  set_status_ok(resp);
}

void handle_wait_ready(const WaitReadyRequest & /*req*/,
                       anolis::deviceprovider::v1::Response &resp) {
  // Simulated hardware initialization - in real providers this would:
  // - Initialize communication buses (I2C, SPI, USB, etc.)
  // - Perform device discovery and enumeration
  // - Run self-tests and calibration
  // - Wait for hardware warm-up periods
  // For sim, we just report immediate readiness

  std::cerr << "[WaitReady] Processing wait_ready() request\n";

  const auto init_report =
      anolis_provider_sim::DeviceFactory::get_initialization_report();
  auto *out = resp.mutable_wait_ready();
  (*out->mutable_diagnostics())["init_time_ms"] = "0";
  (*out->mutable_diagnostics())["device_count"] =
      std::to_string(sim_devices::list_devices(false).size());
  (*out->mutable_diagnostics())["startup_policy"] =
      sim_health::startup_policy_name(init_report.startup_policy);
  (*out->mutable_diagnostics())["startup_configured_devices"] =
      std::to_string(init_report.configured_device_count);
  (*out->mutable_diagnostics())["startup_initialized_devices"] =
      std::to_string(init_report.successful_device_ids.size());
  (*out->mutable_diagnostics())["startup_failed_devices"] =
      std::to_string(init_report.failed_devices.size());
  (*out->mutable_diagnostics())["startup_degraded"] =
      init_report.failed_devices.empty() ? "false" : "true";
  (*out->mutable_diagnostics())["provider_version"] =
      ANOLIS_PROVIDER_SIM_VERSION;
  (*out->mutable_diagnostics())["provider_impl"] = "sim";

  set_status_ok(resp);
}

void handle_unimplemented(anolis::deviceprovider::v1::Response &resp) {
  set_status(resp, Status::CODE_UNIMPLEMENTED, "operation not implemented");
}

} // namespace handlers
