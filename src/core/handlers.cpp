/**
 * @file handlers.cpp
 * @brief Implementation of the sim provider's ADPP request handlers.
 *
 * Handlers stay intentionally thin: they validate request shape, translate
 * ADPP selectors into device-manager calls, and project startup diagnostics
 * through the runtime snapshot.
 */

#include "core/handlers.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <unordered_set>
#include <vector>

#include "core/health.hpp"
#include "core/runtime_state.hpp"
#include "core/transport/framed_stdio.hpp"
#include "devices/common/device_factory.hpp"
#include "devices/common/device_manager.hpp"
#include "logging/logger.hpp"

namespace anolis_provider_sim::handlers {

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

static inline void set_status(anolis::deviceprovider::v1::Response &resp, Status::Code code, const std::string &msg) {
    resp.mutable_status()->set_code(code);
    resp.mutable_status()->set_message(msg);
}

void handle_hello(const HelloRequest &req, anolis::deviceprovider::v1::Response &resp) {
    if (req.protocol_version() != "v1") {
        set_status(resp, Status::CODE_FAILED_PRECONDITION, "unsupported protocol_version; expected v1");
        return;
    }

    auto *hello = resp.mutable_hello();
    hello->set_protocol_version("v1");
    hello->set_provider_name("anolis-provider-sim");
    hello->set_provider_version(ANOLIS_PROVIDER_SIM_VERSION);

    (*hello->mutable_metadata())["transport"] = "stdio+uint32_le";
    (*hello->mutable_metadata())["max_frame_bytes"] = std::to_string(anolis_provider_sim::transport::kMaxFrameBytes);
    (*hello->mutable_metadata())["supports_wait_ready"] = "true";

    set_status_ok(resp);
}

void handle_list_devices(const ListDevicesRequest &req, anolis::deviceprovider::v1::Response &resp) {
    auto *out = resp.mutable_list_devices();

    const auto devices = sim_devices::list_devices(req.include_health());
    for (const auto &d : devices) {
        *out->add_devices() = d;
    }

    if (req.include_health()) {
        const auto device_health = anolis_provider_sim::health::make_list_devices_health(devices);
        for (const auto &health : device_health) {
            *out->add_device_health() = health;
        }
    }

    set_status_ok(resp);
}

void handle_describe_device(const DescribeDeviceRequest &req, anolis::deviceprovider::v1::Response &resp) {
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

// [§7.3] Best-effort min_timestamp: sim reads are live (current ticker state),
// so any past min_timestamp is already satisfied; a value older than the
// requested min_timestamp (e.g. an unsatisfiable future timestamp) is flagged
// QUALITY_STALE rather than reported fresh.
void apply_min_timestamp(const anolis::deviceprovider::v1::ReadSignalsRequest &req,
                         anolis::deviceprovider::v1::ReadSignalsResponse &out) {
    if (!req.has_min_timestamp()) {
        return;
    }
    const auto &min_ts = req.min_timestamp();
    for (auto &value : *out.mutable_values()) {
        const auto &ts = value.timestamp();
        const bool older =
            ts.seconds() < min_ts.seconds() || (ts.seconds() == min_ts.seconds() && ts.nanos() < min_ts.nanos());
        if (older) {
            value.set_quality(anolis::deviceprovider::v1::SignalValue::QUALITY_STALE);
        }
    }
}

void handle_read_signals(const ReadSignalsRequest &req, anolis::deviceprovider::v1::Response &resp) {
    if (req.device_id().empty()) {
        set_status(resp, Status::CODE_INVALID_ARGUMENT, "device_id is required");
        return;
    }

    std::vector<std::string> ids;
    ids.reserve(static_cast<size_t>(req.signal_ids_size()));
    for (const auto &s : req.signal_ids()) ids.push_back(s);

    // `chaos_control` is always addressable even though it intentionally exposes
    // no readable signals through the normal device registry path.
    if (req.device_id() != "chaos_control" &&
        (!anolis_provider_sim::DeviceFactory::is_config_loaded() ||
         !anolis_provider_sim::DeviceFactory::is_device_registered(req.device_id()))) {
        set_status(resp, Status::CODE_NOT_FOUND, "unknown device_id: " + req.device_id());
        return;
    }

    // semantics.md 7.4: a provider MUST choose ONE consistent policy for unknown
    // signal ids. We fail the whole read with CODE_NOT_FOUND if ANY requested id is
    // unknown (the spec's recommended policy, and what the other providers do) —
    // never partial-OK for some-unknown but NOT_FOUND for all-unknown.
    if (!ids.empty()) {
        std::unordered_set<std::string> known;
        if (req.device_id() != "chaos_control") {
            // chaos_control intentionally exposes no readable signals; describing it is
            // not meaningful, so every explicit id is treated as unknown below.
            const auto caps = sim_devices::describe_device(req.device_id());
            known.reserve(static_cast<size_t>(caps.signals_size()));
            for (const auto &sig : caps.signals()) known.insert(sig.signal_id());
        }
        for (const auto &id : ids) {
            if (known.find(id) == known.end()) {
                set_status(resp, Status::CODE_NOT_FOUND,
                           "unknown signal_id '" + id + "' for device '" + req.device_id() + "'");
                return;
            }
        }
    }

    const auto values = sim_devices::read_signals(req.device_id(), ids);

    auto *out = resp.mutable_read_signals();
    out->set_device_id(req.device_id());
    for (const auto &v : values) {
        *out->add_values() = v;
    }
    apply_min_timestamp(req, *out);

    set_status_ok(resp);
}

void handle_call(const CallRequest &req, anolis::deviceprovider::v1::Response &resp) {
    if (req.device_id().empty()) {
        set_status(resp, Status::CODE_INVALID_ARGUMENT, "device_id is required");
        return;
    }
    if (req.function_id() == 0 && req.function_name().empty()) {
        set_status(resp, Status::CODE_INVALID_ARGUMENT, "function_id or function_name is required");
        return;
    }

    uint32_t resolved_function_id = req.function_id();

    // semantics.md 6.2: if both function_id and function_name are provided, the
    // provider MUST PREFER function_id (not reject a mismatch). We only resolve the
    // name when function_id is unset.
    if (req.function_id() == 0) {
        const auto function_id = sim_devices::resolve_function_id(req.device_id(), req.function_name());
        if (!function_id.has_value()) {
            set_status(resp, Status::CODE_NOT_FOUND,
                       "unknown function_name '" + req.function_name() + "' for device_id '" + req.device_id() + "'");
            return;
        }
        resolved_function_id = *function_id;
    }

    std::map<std::string, anolis::deviceprovider::v1::Value> args;
    for (const auto &kv : req.args()) {
        args.emplace(kv.first, kv.second);
    }

    const auto result = sim_devices::call_function(req.device_id(), resolved_function_id, args);
    if (result.code != Status::CODE_OK) {
        set_status(resp, result.code, result.message);
        return;
    }

    auto *out = resp.mutable_call();
    out->set_device_id(req.device_id());
    // ADPP §8: populate the declared `accepted` result on success.
    anolis::deviceprovider::v1::Value accepted;
    accepted.set_type(anolis::deviceprovider::v1::VALUE_TYPE_BOOL);
    accepted.set_bool_value(true);
    (*out->mutable_results())["accepted"] = accepted;
    set_status_ok(resp);
}

void handle_get_health(const GetHealthRequest & /*req*/, anolis::deviceprovider::v1::Response &resp) {
    const auto runtime_state = anolis_provider_sim::runtime::snapshot();
    const auto init_report = runtime_state.startup_report;
    auto *out = resp.mutable_get_health();
    *out->mutable_provider() = anolis_provider_sim::health::make_provider_health(init_report);

    const auto device_health = anolis_provider_sim::health::make_get_health_devices(init_report);
    for (const auto &health : device_health) {
        *out->add_devices() = health;
    }

    set_status_ok(resp);
}

void handle_wait_ready(const WaitReadyRequest & /*req*/, anolis::deviceprovider::v1::Response &resp) {
    PSIM_LOG_INFO("Handlers", "Processing wait_ready() request");

    const auto runtime_state = anolis_provider_sim::runtime::snapshot();
    const auto init_report = runtime_state.startup_report;
    auto *out = resp.mutable_wait_ready();
    // Sim mode has no asynchronous hardware warm-up, so readiness is projected
    // directly from the startup report captured during process initialization.
    (*out->mutable_diagnostics())["init_time_ms"] = "0";
    (*out->mutable_diagnostics())["device_count"] = std::to_string(sim_devices::list_devices(false).size());
    (*out->mutable_diagnostics())["startup_policy"] =
        anolis_provider_sim::health::startup_policy_name(init_report.startup_policy);
    (*out->mutable_diagnostics())["startup_configured_devices"] = std::to_string(init_report.configured_device_count);
    (*out->mutable_diagnostics())["startup_initialized_devices"] =
        std::to_string(init_report.successful_device_ids.size());
    (*out->mutable_diagnostics())["startup_failed_devices"] = std::to_string(init_report.failed_devices.size());
    (*out->mutable_diagnostics())["startup_degraded"] = init_report.failed_devices.empty() ? "false" : "true";
    (*out->mutable_diagnostics())["provider_version"] = ANOLIS_PROVIDER_SIM_VERSION;
    (*out->mutable_diagnostics())["provider_impl"] = "sim";

    set_status_ok(resp);
}

void handle_unimplemented(anolis::deviceprovider::v1::Response &resp, const std::string &message) {
    set_status(resp, Status::CODE_UNIMPLEMENTED, message);
}

}  // namespace anolis_provider_sim::handlers
