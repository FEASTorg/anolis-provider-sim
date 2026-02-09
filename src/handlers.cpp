#include "handlers.hpp"

#include <string>
#include <vector>

#include "devices/device_manager.hpp"
#include "health.hpp"
#include "transport/framed_stdio.hpp"

namespace handlers {

using anolis::deviceprovider::v0::CallRequest;
using anolis::deviceprovider::v0::DescribeDeviceRequest;
using anolis::deviceprovider::v0::GetHealthRequest;
using anolis::deviceprovider::v0::HelloRequest;
using anolis::deviceprovider::v0::ListDevicesRequest;
using anolis::deviceprovider::v0::ReadSignalsRequest;
using anolis::deviceprovider::v0::Status;

static inline void set_status_ok(anolis::deviceprovider::v0::Response &resp) {
  resp.mutable_status()->set_code(Status::CODE_OK);
  resp.mutable_status()->set_message("ok");
}

static inline void set_status(anolis::deviceprovider::v0::Response &resp,
                              Status::Code code, const std::string &msg) {
  resp.mutable_status()->set_code(code);
  resp.mutable_status()->set_message(msg);
}

void handle_hello(const HelloRequest &req,
                  anolis::deviceprovider::v0::Response &resp) {
  if (req.protocol_version() != "v0") {
    set_status(resp, Status::CODE_FAILED_PRECONDITION,
               "unsupported protocol_version; expected v0");
    return;
  }

  auto *hello = resp.mutable_hello();
  hello->set_protocol_version("v0");
  hello->set_provider_name("anolis-provider-sim");
  hello->set_provider_version("0.0.2");

  (*hello->mutable_metadata())["transport"] = "stdio+uint32_le";
  (*hello->mutable_metadata())["max_frame_bytes"] =
      std::to_string(transport::kMaxFrameBytes);

  set_status_ok(resp);
}

void handle_list_devices(const ListDevicesRequest &req,
                         anolis::deviceprovider::v0::Response &resp) {
  auto *out = resp.mutable_list_devices();

  const auto devices = sim_devices::list_devices(req.include_health());
  for (const auto &d : devices) {
    *out->add_devices() = d;
  }

  // v0 sim: we ignore include_health for now (device_health omitted
  // intentionally).
  set_status_ok(resp);
}

void handle_describe_device(const DescribeDeviceRequest &req,
                            anolis::deviceprovider::v0::Response &resp) {
  if (req.device_id().empty()) {
    set_status(resp, Status::CODE_INVALID_ARGUMENT, "device_id is required");
    return;
  }

  const auto devices = sim_devices::list_devices(false);
  bool found = false;
  anolis::deviceprovider::v0::Device dev;
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
                         anolis::deviceprovider::v0::Response &resp) {
  if (req.device_id().empty()) {
    set_status(resp, Status::CODE_INVALID_ARGUMENT, "device_id is required");
    return;
  }

  std::vector<std::string> ids;
  ids.reserve(static_cast<size_t>(req.signal_ids_size()));
  for (const auto &s : req.signal_ids())
    ids.push_back(s);

  const auto values = sim_devices::read_signals(req.device_id(), ids);

  if (!ids.empty() && values.empty()) {
    // v0 policy: if explicit ids were requested and none are returned, treat as
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
                 anolis::deviceprovider::v0::Response &resp) {
  if (req.device_id().empty()) {
    set_status(resp, Status::CODE_INVALID_ARGUMENT, "device_id is required");
    return;
  }
  if (req.function_id() == 0 && req.function_name().empty()) {
    set_status(resp, Status::CODE_INVALID_ARGUMENT,
               "function_id or function_name is required");
    return;
  }

  // v0: only function_id supported (matches our DescribeDevice contract).
  if (req.function_id() == 0) {
    set_status(resp, Status::CODE_UNIMPLEMENTED,
               "function_name lookup not implemented in sim provider v0");
    return;
  }

  std::map<std::string, anolis::deviceprovider::v0::Value> args;
  for (const auto &kv : req.args()) {
    args.emplace(kv.first, kv.second);
  }

  const auto result =
      sim_devices::call_function(req.device_id(), req.function_id(), args);
  if (result.code != Status::CODE_OK) {
    set_status(resp, result.code, result.message);
    return;
  }

  auto *out = resp.mutable_call();
  out->set_device_id(req.device_id());
  // v0 sim: no results and no operation_id
  set_status_ok(resp);
}

void handle_get_health(const GetHealthRequest & /*req*/,
                       anolis::deviceprovider::v0::Response &resp) {
  auto *out = resp.mutable_get_health();
  *out->mutable_provider() = sim_health::make_provider_health_ok();

  // v0 sim: device health is optional; include basic OK states for known
  // devices
  const auto devices = sim_devices::list_devices(false);
  for (const auto &d : devices) {
    auto *dh = out->add_devices();
    dh->set_device_id(d.device_id());
    dh->set_state(anolis::deviceprovider::v0::DeviceHealth::STATE_OK);
    dh->set_message("ok");
    // last_seen omitted in v0 sim
    (*dh->mutable_metrics())["impl"] = "sim";
  }

  set_status_ok(resp);
}

void handle_unimplemented(anolis::deviceprovider::v0::Response &resp) {
  set_status(resp, Status::CODE_UNIMPLEMENTED, "operation not implemented");
}

} // namespace handlers
