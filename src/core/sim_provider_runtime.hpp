#pragma once

/**
 * @file sim_provider_runtime.hpp
 * @brief sim's implementation of the shared SDK `ProviderRuntime` seam.
 *
 * Thin adapter: each method delegates to the existing `sim_devices::` device
 * substrate + the startup report. The SDK's generic run-loop + handlers own all
 * ADPP envelope/policy; this only supplies sim's inventory, device-model
 * dispatch, and readiness. The HandleT seam is `std::monostate` (sim has no
 * hardware session) — but sim keeps its own richer `DeviceAdapter` struct
 * (physics axes the SDK template lacks), so it does not instantiate
 * `DeviceAdapter<HandleT>`; the dispatch goes through `sim_devices::` directly.
 */

#include "anolis/provider_sdk/runtime.hpp"
#include "core/startup_report.hpp"

namespace anolis_provider_sim {

class SimProviderRuntime : public anolis::provider_sdk::ProviderRuntime {
public:
    explicit SimProviderRuntime(DeviceInitializationReport report);

    anolis::provider_sdk::ProviderMetadata metadata() const override;
    anolis::provider_sdk::ReadinessReport readiness() const override;
    anolis::provider_sdk::DeviceHealthExtra device_health(const std::string& device_id) const override;
    std::vector<std::string> list_device_ids() const override;
    bool has_device(const std::string& device_id) const override;
    anolis::deviceprovider::v1::Device device_info(const std::string& device_id) const override;
    anolis::deviceprovider::v1::CapabilitySet capabilities(const std::string& device_id) const override;
    anolis::provider_sdk::AdapterReadResult read(const std::string& device_id,
                                                 const std::vector<std::string>& signal_ids) override;
    anolis::provider_sdk::AdapterCallResult call(const std::string& device_id, uint32_t function_id,
                                                 const anolis::provider_sdk::ValueMap& args) override;
    std::optional<uint32_t> resolve_function_id(const std::string& device_id,
                                                const std::string& function_name) const override;

private:
    DeviceInitializationReport report_;
};

}  // namespace anolis_provider_sim
