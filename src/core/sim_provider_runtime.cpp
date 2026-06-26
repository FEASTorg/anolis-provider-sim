#include "core/sim_provider_runtime.hpp"

#include <map>
#include <utility>

#include "chaos/fault_injection.hpp"
#include "devices/common/device_adapter.hpp"
#include "devices/common/device_manager.hpp"

#ifndef ANOLIS_PROVIDER_SIM_VERSION
#define ANOLIS_PROVIDER_SIM_VERSION "0.0.0"
#endif

namespace anolis_provider_sim {

namespace adpp = anolis::deviceprovider::v1;
namespace sdk = anolis::provider_sdk;

SimProviderRuntime::SimProviderRuntime(DeviceInitializationReport report) : report_(std::move(report)) {}

sdk::ProviderMetadata SimProviderRuntime::metadata() const {
    sdk::ProviderMetadata meta;
    meta.name = "anolis-provider-sim";
    meta.version = ANOLIS_PROVIDER_SIM_VERSION;
    meta.protocol_version = "v1";
    // The SDK's handle_hello injects transport + max_frame_bytes itself; supply
    // only sim's extra advertisement (preserves the pre-migration Hello metadata).
    meta.hello_extra["supports_wait_ready"] = "true";
    return meta;
}

sdk::ReadinessReport SimProviderRuntime::readiness() const {
    sdk::ReadinessReport r;
    r.ready = true;  // sim has no asynchronous hardware warm-up
    r.configured_device_count = static_cast<int>(report_.configured_device_count);
    r.successful_device_ids = report_.successful_device_ids;
    for (const auto& failure : report_.failed_devices) {
        // Preserve sim's pre-migration device-health message phrasing (the SDK uses
        // the provider-supplied reason verbatim as the DeviceHealth.message).
        r.failed_devices.push_back(
            {failure.device_id, failure.type, "startup initialization failed: " + failure.reason});
    }
    r.startup_policy = report_.startup_policy == StartupPolicy::Strict ? "strict" : "degraded";
    r.provider_impl = "sim";
    r.extra_diagnostics["init_time_ms"] = "0";  // sim projects readiness from the startup snapshot
    return r;
}

std::vector<std::string> SimProviderRuntime::list_device_ids() const {
    std::vector<std::string> ids;
    for (const auto& device : sim_devices::list_devices(false)) {
        ids.push_back(device.device_id());
    }
    return ids;
}

bool SimProviderRuntime::has_device(const std::string& device_id) const {
    // A fault-injected-unavailable device is filtered out (matches pre-migration
    // list_devices(false) semantics, which the SDK describe/read handlers gate on):
    // describe/read then return NOT_FOUND, as before, until the fault clears.
    if (sim_devices::fault_injection::is_device_unavailable(device_id)) {
        return false;
    }
    // adapter_by_id is chaos-aware (returns the chaos adapter for "chaos_control")
    // and otherwise checks the device registry.
    return sim_devices::adapter_by_id(device_id) != nullptr;
}

adpp::Device SimProviderRuntime::device_info(const std::string& device_id) const {
    const auto* adapter = sim_devices::adapter_by_id(device_id);
    if (adapter == nullptr || adapter->get_device_info == nullptr) {
        return {};
    }
    return adapter->get_device_info(device_id, /*include_health=*/false);
}

adpp::CapabilitySet SimProviderRuntime::capabilities(const std::string& device_id) const {
    return sim_devices::describe_device(device_id);
}

sdk::AdapterReadResult SimProviderRuntime::read(const std::string& device_id,
                                                const std::vector<std::string>& signal_ids) {
    // sim reads never fail (quality is carried per-value, incl. fault-injection).
    sdk::AdapterReadResult result;
    result.ok = true;
    result.error_code = adpp::Status::CODE_OK;
    result.values = sim_devices::read_signals(device_id, signal_ids);
    return result;
}

sdk::AdapterCallResult SimProviderRuntime::call(const std::string& device_id, uint32_t function_id,
                                                const sdk::ValueMap& args) {
    std::map<std::string, adpp::Value> arg_map;
    for (const auto& [key, value] : args) {
        arg_map.emplace(key, value);
    }
    const sim_devices::CallResult result = sim_devices::call_function(device_id, function_id, arg_map);
    return {result.code == adpp::Status::CODE_OK, result.code, result.message};
}

std::optional<uint32_t> SimProviderRuntime::resolve_function_id(const std::string& device_id,
                                                                const std::string& function_name) const {
    return sim_devices::resolve_function_id(device_id, function_name);
}

}  // namespace anolis_provider_sim
