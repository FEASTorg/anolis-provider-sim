#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include "anolis/provider_sdk/runtime.hpp"
#include "config.hpp"
#include "core/sim_provider_runtime.hpp"
#include "devices/common/device_factory.hpp"
#include "devices/common/device_manager.hpp"
#include "logging/logger.hpp"
#include "simulation/engines/local_engine.hpp"
#include "simulation/engines/null_engine.hpp"
#include "simulation/engines/remote_engine.hpp"
#include "simulation/simulation_engine.hpp"

#ifdef HAVE_FLUXGRAPH
#include "simulation/adapters/fluxgraph/fluxgraph_adapter.hpp"
#endif

static void set_binary_mode_stdio() {
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
}

static std::unique_ptr<sim_engine::SimulationEngine> create_engine(const anolis_provider_sim::ProviderConfig &config,
                                                                   const std::string &sim_server_address) {
    using anolis_provider_sim::SimulationMode;

    switch (config.simulation_mode) {
        case SimulationMode::Inert:
            PSIM_LOG_INFO("Main", "mode=inert (no simulation)");
            return std::make_unique<sim_engine::NullEngine>();

        case SimulationMode::NonInteracting:
            PSIM_LOG_INFO("Main", "mode=non_interacting (local physics)");
            return std::make_unique<sim_engine::LocalEngine>();

        case SimulationMode::Sim:
#ifdef HAVE_FLUXGRAPH
            if (sim_server_address.empty()) {
                throw std::runtime_error("mode=sim requires --sim-server <host:port>");
            }
            PSIM_LOG_INFO("Main", "mode=sim (external simulation at " + sim_server_address + ")");
            return std::make_unique<sim_engine::RemoteEngine>(
                std::make_unique<sim_adapters::FluxGraphAdapter>(sim_server_address),
                config.tick_rate_hz.value_or(10.0));
#else
            (void)sim_server_address;
            throw std::runtime_error(
                "mode=sim requires FluxGraph support. Rebuild with "
                "-DENABLE_FLUXGRAPH=ON");
#endif
    }

    throw std::runtime_error("Unknown simulation mode");
}

int main(int argc, char **argv) {
    anolis_provider_sim::logging::Logger::init_from_env();

    std::optional<std::string> config_path;
    std::optional<std::string> sim_server_address;
    double crash_after_sec = -1.0;
    bool check_config_only = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--version") {
            std::cout << ANOLIS_PROVIDER_SIM_VERSION << '\n';
            return 0;
        } else if (arg == "--config" && i + 1 < argc) {
            config_path = argv[++i];
        } else if (arg == "--check-config" && i + 1 < argc) {
            config_path = argv[++i];
            check_config_only = true;
        } else if (arg == "--crash-after" && i + 1 < argc) {
            try {
                crash_after_sec = std::stod(argv[++i]);
            } catch (...) {
                PSIM_LOG_ERROR("Main", "invalid --crash-after value");
                return 1;
            }
        } else if (arg == "--sim-server" && i + 1 < argc) {
            sim_server_address = argv[++i];
        } else {
            PSIM_LOG_ERROR("Main", "unknown or malformed argument: " + arg);
            PSIM_LOG_ERROR("Main",
                           "Usage: anolis-provider-sim --config <path/to/config.yaml> "
                           "[--sim-server <host:port>] [--crash-after <seconds>]");
            return 1;
        }
    }

    if (!config_path) {
        PSIM_LOG_ERROR("Main", "FATAL: --config argument is required");
        PSIM_LOG_ERROR("Main",
                       "Usage: anolis-provider-sim --config <path/to/config.yaml> "
                       "[--sim-server <host:port>]");
        return 1;
    }

    anolis_provider_sim::ProviderConfig config;
    anolis_provider_sim::DeviceInitializationReport init_report;
    try {
        PSIM_LOG_INFO("Main", "loading configuration from: " + *config_path);
        config = anolis_provider_sim::load_config(*config_path);

        init_report = anolis_provider_sim::DeviceFactory::initialize_from_config(config);
        PSIM_LOG_INFO(
            "Main", "startup_policy=" + std::string(config.startup_policy == anolis_provider_sim::StartupPolicy::Strict
                                                        ? "strict"
                                                        : "degraded"));
        PSIM_LOG_INFO("Main", "initialized " + std::to_string(init_report.successful_device_ids.size()) + " / " +
                                  std::to_string(init_report.configured_device_count) + " devices");
        if (!init_report.failed_devices.empty()) {
            for (const auto &failure : init_report.failed_devices) {
                PSIM_LOG_WARN("Main", "degraded init failure: device_id=" + failure.device_id +
                                          " type=" + failure.type + " reason=" + failure.reason);
            }
        }

        if (check_config_only) {
            const std::string mode_str =
                (config.simulation_mode == anolis_provider_sim::SimulationMode::Inert)            ? "inert"
                : (config.simulation_mode == anolis_provider_sim::SimulationMode::NonInteracting) ? "non_interacting"
                                                                                                  : "sim";
            PSIM_LOG_INFO("Main", "Config valid: " + config.provider_name.value_or("provider-sim") +
                                      ", mode=" + mode_str + ", " +
                                      std::to_string(init_report.successful_device_ids.size()) + " device(s)");
            return 0;
        }

        if (config.simulation_mode != anolis_provider_sim::SimulationMode::Sim && sim_server_address) {
            PSIM_LOG_WARN("Main", "--sim-server ignored for non-sim mode");
        }

        auto engine = create_engine(config, sim_server_address.value_or(""));
        engine->set_provider_id(config.provider_name.value_or("provider-sim"));

        if (config.simulation_mode == anolis_provider_sim::SimulationMode::Sim) {
            std::filesystem::path config_dir = std::filesystem::path(config.config_file_path).parent_path();
            std::filesystem::path physics_path = config_dir / *config.physics_config_path;
            engine->initialize(physics_path.string());
        } else {
            engine->initialize("");
        }

        engine->register_devices(init_report.successful_device_ids);

        sim_devices::set_simulation_engine(std::move(engine));
        sim_devices::initialize_physics(config, init_report.successful_device_ids);

        // Start physics automatically for non-interacting mode only.
        // For sim mode, wait_ready() will start physics after all providers
        // have registered to prevent schedule misalignment in multi-provider
        // scenarios.
        if (config.simulation_mode == anolis_provider_sim::SimulationMode::NonInteracting) {
            PSIM_LOG_INFO("Main",
                          "mode=non-interacting: auto-starting physics "
                          "ticker");
            sim_devices::start_physics();
        } else {
            std::string mode_str =
                (config.simulation_mode == anolis_provider_sim::SimulationMode::Sim) ? "sim" : "inert";
            PSIM_LOG_INFO("Main", "mode=" + mode_str + ": deferring physics ticker until wait_ready()");
        }

    } catch (const std::exception &e) {
        PSIM_LOG_ERROR("Main", "FATAL: Failed to initialize simulation: " + std::string(e.what()));
        return 1;
    }

    set_binary_mode_stdio();
    PSIM_LOG_INFO("Main", "starting (transport=stdio+uint32_le)");

    if (crash_after_sec > 0.0) {
        PSIM_LOG_WARN("Main", "CHAOS MODE: will crash after " + std::to_string(crash_after_sec) + " seconds");
        std::thread([crash_after_sec]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<long long>(crash_after_sec * 1000)));
            PSIM_LOG_ERROR("Main", "CRASHING NOW (exit 42)");
            std::exit(42);
        }).detach();
    }

    // The SDK run-loop owns the transport, the §3.2 Hello-gate, dispatch, and exit
    // codes; sim supplies the device/inventory/readiness seam + the physics
    // lifecycle hooks. on_wait_ready starts the ticker (same ordering as before);
    // on_shutdown tears it down on every loop exit (was PhysicsShutdownGuard).
    anolis_provider_sim::SimProviderRuntime sim_runtime(init_report);
    anolis::provider_sdk::LifecycleHooks hooks;
    hooks.on_wait_ready = [] {
        PSIM_LOG_INFO("Main", "waiting ready -> starting physics ticker");
        sim_devices::start_physics();
    };
    hooks.on_shutdown = [] { sim_devices::stop_physics(); };

    return anolis::provider_sdk::run_loop(std::cin, std::cout, sim_runtime, hooks);
}
