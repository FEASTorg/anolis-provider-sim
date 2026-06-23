#pragma once

/**
 * @file protocol_adapter.hpp
 * @brief External simulation protocol boundary used by provider-sim backends.
 */

#include <chrono>
#include <map>
#include <string>
#include <vector>

#include "simulation/simulation_engine.hpp"

namespace sim_adapters {

/**
 * @brief Abstract adapter between provider-sim and an external simulation
 * service.
 *
 * Implementations translate provider actuator snapshots into protocol-specific
 * updates and translate remote sensor/command outputs back into the generic
 * simulation engine model.
 */
class ProtocolAdapter {
public:
    virtual ~ProtocolAdapter() = default;

    /** @brief Connect the adapter to its remote or local endpoint. */
    virtual void connect(const std::string &address) = 0;
    /** @brief Load adapter-specific protocol or graph configuration. */
    virtual void load_config(const std::string &config_path) = 0;
    /** @brief Register the provider identity and active devices with the backend.
     */
    virtual void register_provider(const std::string &provider_name, const std::vector<std::string> &device_ids) = 0;

    /** @brief Publish the current actuator snapshot to the external simulation.
     */
    virtual bool update_signals(const std::map<std::string, double> &actuators, const std::string &unit,
                                std::chrono::milliseconds timeout) = 0;

    /** @brief Read the requested signal paths from the external simulation. */
    virtual std::map<std::string, double> read_signals(const std::vector<std::string> &signal_paths) = 0;

    /** @brief Drain any backend-emitted commands queued for provider execution.
     */
    virtual std::vector<sim_engine::Command> drain_commands() = 0;

    /** @brief List signal paths known to the adapter, when supported. */
    virtual std::vector<std::string> list_signals() { return {}; }
};

}  // namespace sim_adapters
