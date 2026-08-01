#include "config_schema.hpp"

/**
 * @file config_schema.cpp
 * @brief The declare-once config schema for anolis-provider-sim.
 *
 * Titles, defaults, and placeholders carry the form-authoring knowledge the
 * workbench previously hardcoded in its provider catalog — a client renders
 * config forms from this schema alone (anolis-workbench#270).
 */

namespace anolis_provider_sim {

namespace cfg = anolis::provider_sdk::config;

namespace {

cfg::Schema build_schema() {
    // The provider section is optional, but when present its name is required
    // (it may appear in multi-provider simulation setups). The section stays
    // OPEN like the rest of sim's historically-lenient top level.
    cfg::Object provider(cfg::Openness::Open);
    provider.field(cfg::string_field("name")
                       .required()
                       .title("Provider name")
                       .description("Stable identifier for this provider instance.")
                       .pattern("^[A-Za-z0-9_.-]{1,64}$"));

    // Device entries carry type-specific config subtrees the loader passes
    // through verbatim (each device implementation validates its own subtree),
    // so the item object is OPEN: only the common envelope is declared.
    cfg::Object device(cfg::Openness::Open);
    device.field(cfg::string_field("id")
                     .required()
                     .unique()
                     .non_empty()
                     .title("Device id")
                     .forbid_value("chaos_control",
                                   "devices[].id 'chaos_control' is reserved and cannot be configured explicitly"));
    device.field(cfg::string_field("type")
                     .required()
                     .non_empty()
                     .title("Device type")
                     .description("A device type registered with the factory; the built-in set is "
                                  "tempctl, motorctl, relayio, analogsensor."));

    cfg::Object simulation(cfg::Openness::Closed);
    simulation.field(cfg::string_field("mode")
                         .required()
                         .title("Simulation mode")
                         .enum_value("non_interacting", "Local physics (fixed tick)")
                         .enum_value("inert", "No automatic updates")
                         .enum_value("sim", "External simulation engine"));
    simulation.field(cfg::number_field("tick_rate_hz")
                         .title("Tick rate (Hz)")
                         .description("Required for the ticking modes (non_interacting, sim).")
                         .min_number(0.1)
                         .max_number(1000.0)
                         .default_number(10.0));
    simulation.field(cfg::string_field("physics_config")
                         .non_empty()
                         .title("Physics config path")
                         .description("Path to the external physics config; required for mode sim, "
                                      "resolved relative to this config file."));
    simulation.field(cfg::number_field("ambient_temp_c").title("Ambient temperature (°C)"));
    simulation.field(cfg::string_field("ambient_signal_path")
                         .non_empty()
                         .title("Ambient signal path")
                         .description("Signal path that publishes the ambient temperature; requires "
                                      "ambient_temp_c."));
    simulation.dependent_required("ambient_signal_path", {"ambient_temp_c"});
    // Mode-dependent keys, mirroring the historical per-mode allow-lists:
    // inert accepts only `mode`; non_interacting adds a required tick;
    // sim runs the external engine (ambient pair optional).
    simulation.when("mode", "inert",
                    cfg::When()
                        .forbid("tick_rate_hz")
                        .forbid("physics_config")
                        .forbid("ambient_temp_c")
                        .forbid("ambient_signal_path"));
    simulation.when("mode", "non_interacting",
                    cfg::When()
                        .require("tick_rate_hz")
                        .forbid("physics_config")
                        .forbid("ambient_temp_c")
                        .forbid("ambient_signal_path"));
    simulation.when("mode", "sim", cfg::When().require("tick_rate_hz").require("physics_config"));
    simulation.deprecated_key("noise_enabled", "simulation.noise_enabled is no longer supported");
    simulation.deprecated_key("update_rate_hz", "simulation.update_rate_hz is no longer supported");

    // Root stays OPEN: sim has never rejected unknown top-level keys, and
    // multi-provider config files may carry sections for other tools.
    cfg::Object root(cfg::Openness::Open);
    root.child("provider", std::move(provider));
    root.field(cfg::string_field("startup_policy")
                   .title("Startup policy")
                   .enum_value("strict", "Abort on first device init failure")
                   .enum_value("degraded", "Continue with the devices that initialized")
                   .default_string("strict"));
    root.array("devices", cfg::Array::of_objects(std::move(device)));
    root.child("simulation", std::move(simulation), cfg::Presence::Required);

    return cfg::Schema(std::move(root))
        .title("anolis-provider-sim configuration")
        .description("Simulated devices for development and testing.");
}

}  // namespace

const cfg::Schema &provider_schema() {
    static const cfg::Schema schema = build_schema();
    return schema;
}

}  // namespace anolis_provider_sim
