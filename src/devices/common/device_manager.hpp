#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "config.hpp"
#include "devices/common/device_common.hpp"
#include "devices/common/signal_registry.hpp"
#include "protocol.pb.h"
#include "simulation/simulation_engine.hpp"

namespace sim_devices {

using anolis::deviceprovider::v1::CapabilitySet;
using anolis::deviceprovider::v1::Device;
using anolis::deviceprovider::v1::SignalValue;
using anolis::deviceprovider::v1::Value;

// ---- Physics engine management ----

void initialize_physics(
    const anolis_provider_sim::ProviderConfig &provider_config);
void start_physics();
void stop_physics();
void set_simulation_engine(
    std::unique_ptr<sim_engine::SimulationEngine> engine);

// ---- Signal Registry (for physics-device coordination) ----

extern sim_coordination::SignalRegistry
    *g_signal_registry; // Global registry instance

// ---- Public API used by handlers ----

std::vector<Device> list_devices(bool include_health = false);
CapabilitySet describe_device(const std::string &device_id);

std::vector<SignalValue>
read_signals(const std::string &device_id,
             const std::vector<std::string> &signal_ids);

CallResult call_function(const std::string &device_id, uint32_t function_id,
                         const std::map<std::string, Value> &args);

} // namespace sim_devices
