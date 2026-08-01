/**
 * @file config.cpp
 * @brief Config loading for anolis-provider-sim, driven by the declare-once
 * schema (config_schema.cpp).
 *
 * Validation runs the SDK validator against the SAME schema `--config-schema`
 * advertises, and value extraction uses the SDK's typed helpers (the same
 * scalar resolver as the validator) — so the advertised contract, the enforced
 * validation, and the parsed values cannot drift apart. The one rule the
 * schema cannot express (conditionals see sibling keys only) stays here:
 * `devices[].physics_bindings` is only valid when `simulation.mode` is `sim`.
 */

#include "config.hpp"

#include <filesystem>
#include <format>
#include <stdexcept>

#include "anolis/provider_sdk/config_validate.hpp"
#include "config_schema.hpp"

namespace anolis_provider_sim {

namespace fs = std::filesystem;
namespace sdkcfg = anolis::provider_sdk::config;

SimulationMode parse_simulation_mode(const std::string &mode_str) {
    if (mode_str == "non_interacting") {
        return SimulationMode::NonInteracting;
    } else if (mode_str == "inert") {
        return SimulationMode::Inert;
    } else if (mode_str == "sim") {
        return SimulationMode::Sim;
    } else {
        throw std::runtime_error("Invalid simulation.mode: '" + mode_str +
                                 "'. Valid values: non_interacting, inert, sim");
    }
}

StartupPolicy parse_startup_policy(const std::string &policy_str) {
    if (policy_str == "strict") {
        return StartupPolicy::Strict;
    }
    if (policy_str == "degraded") {
        return StartupPolicy::Degraded;
    }
    throw std::runtime_error("Invalid startup_policy: '" + policy_str + "'. Valid values: strict, degraded");
}

ProviderConfig load_config(const std::string &path) {
    YAML::Node yaml;

    try {
        yaml = YAML::LoadFile(path);
    } catch (const YAML::Exception &e) {
        throw std::runtime_error(std::format("Failed to load config file '{}': {}", path, e.what()));
    }

    // Declare-once validation: every structural/semantic error, collected at
    // once, against the schema `--config-schema` advertises.
    const auto errors = sdkcfg::validate(provider_schema(), yaml);
    if (!errors.empty()) {
        throw std::runtime_error(std::format("[CONFIG] Invalid config '{}':\n{}", path, sdkcfg::format_errors(errors)));
    }

    ProviderConfig config;
    config.config_file_path = fs::absolute(path).string();

    // Post-validation extraction with the SDK's typed helpers — validation
    // guarantees presence/type for required fields, so absent optionals are
    // the only nullopt cases here.
    if (const auto name = sdkcfg::as_string(yaml["provider"]["name"])) {
        config.provider_name = *name;
    }
    if (const auto policy = sdkcfg::as_string(yaml["startup_policy"])) {
        config.startup_policy = parse_startup_policy(*policy);
    }

    // Device entries preserve all type-specific keys so the corresponding
    // device implementation can validate and consume its own subtree.
    const YAML::Node devices = yaml["devices"];
    if (devices.IsDefined() && devices.IsSequence()) {
        for (const auto &device_node : devices) {
            DeviceSpec spec;
            if (const auto id = sdkcfg::as_string(device_node["id"])) {
                spec.id = *id;
            }
            if (const auto type = sdkcfg::as_string(device_node["type"])) {
                spec.type = *type;
            }
            for (const auto &kv : device_node) {
                const std::string key = kv.first.Scalar();
                if (key != "id" && key != "type") {
                    spec.config[key] = kv.second;
                }
            }
            config.devices.push_back(spec);
        }
    }

    const YAML::Node simulation = yaml["simulation"];
    if (const auto mode = sdkcfg::as_string(simulation["mode"])) {
        config.simulation_mode = parse_simulation_mode(*mode);
    }
    config.tick_rate_hz = sdkcfg::as_double(simulation["tick_rate_hz"]);
    if (const auto physics_config = sdkcfg::as_string(simulation["physics_config"])) {
        config.physics_config_path = *physics_config;
    }
    config.ambient_temp_c = sdkcfg::as_double(simulation["ambient_temp_c"]);
    if (const auto ambient_signal = sdkcfg::as_string(simulation["ambient_signal_path"])) {
        config.ambient_signal_path = *ambient_signal;
    }

    // Cross-scope rule the schema cannot express (conditionals see sibling
    // keys only): physics_bindings is only meaningful under mode=sim, and a
    // silently-ignored binding would mask config drift.
    if (config.simulation_mode != SimulationMode::Sim) {
        for (const auto &device : config.devices) {
            if (device.config.find("physics_bindings") != device.config.end()) {
                throw std::runtime_error(std::format(
                    "[CONFIG] Device '{}' has physics_bindings but mode!= sim (prevents silent ignored config)",
                    device.id));
            }
        }
    }

    return config;
}

}  // namespace anolis_provider_sim
