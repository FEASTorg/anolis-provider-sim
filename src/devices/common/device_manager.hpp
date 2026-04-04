#pragma once

/**
 * @file device_manager.hpp
 * @brief Shared device and physics-coordination entry points used by ADPP handlers.
 */

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
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

/**
 * @brief Initialize physics coordination state for the active device set.
 *
 * This prepares the signal registry, simulation engine, and any mode-specific
 * inputs before `start_physics()` is called.
 */
void initialize_physics(
    const anolis_provider_sim::ProviderConfig &provider_config,
    const std::vector<std::string> &active_device_ids);

/** @brief Start the background ticker thread when the configured mode uses one. */
void start_physics();

/** @brief Stop the background ticker thread and release physics coordination state. */
void stop_physics();

/** @brief Replace the simulation engine implementation used by the ticker. */
void set_simulation_engine(
    std::unique_ptr<sim_engine::SimulationEngine> engine);

/**
 * @brief Non-owning pointer to the global signal registry, when initialized.
 *
 * This is exposed for device modules that need direct access to coordination
 * state during reads or control updates.
 */
extern sim_coordination::SignalRegistry
    *g_signal_registry;

/** @brief List currently active devices, optionally including health details. */
std::vector<Device> list_devices(bool include_health = false);

/** @brief Return the declared capability surface for one active device. */
CapabilitySet describe_device(const std::string &device_id);

/**
 * @brief Read one device's signals, honoring any physics-driven overrides.
 */
std::vector<SignalValue>
read_signals(const std::string &device_id,
             const std::vector<std::string> &signal_ids);

/**
 * @brief Execute one device function through the registered device implementation.
 */
CallResult call_function(const std::string &device_id, uint32_t function_id,
                         const std::map<std::string, Value> &args);

/**
 * @brief Resolve a function name to its numeric ID for one device.
 */
std::optional<uint32_t> resolve_function_id(const std::string &device_id,
                                            const std::string &function_name);

} // namespace sim_devices
