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

#include "config.hpp"
#include "core/handlers.hpp"
#include "core/runtime_state.hpp"
#include "core/transport/framed_stdio.hpp"
#include "devices/common/device_factory.hpp"
#include "devices/common/device_manager.hpp"
#include "logging/logger.hpp"
#include "protocol.pb.h"
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

static std::unique_ptr<sim_engine::SimulationEngine>
create_engine(const anolis_provider_sim::ProviderConfig &config,
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
    PSIM_LOG_INFO("Main", "mode=sim (external simulation at " +
                              sim_server_address + ")");
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
  sim_runtime::reset();
  anolis_provider_sim::logging::Logger::init_from_env();

  std::optional<std::string> config_path;
  std::optional<std::string> sim_server_address;
  double crash_after_sec = -1.0;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];

    if (arg == "--config" && i + 1 < argc) {
      config_path = argv[++i];
    } else if (arg == "--crash-after" && i + 1 < argc) {
      try {
        crash_after_sec = std::stod(argv[++i]);
      } catch (...) {
        PSIM_LOG_ERROR("Main", "invalid --crash-after value");
        return 1;
      }
    } else if (arg == "--sim-server" && i + 1 < argc) {
      sim_server_address = argv[++i];
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
  try {
    PSIM_LOG_INFO("Main", "loading configuration from: " + *config_path);
    config = anolis_provider_sim::load_config(*config_path);

    const auto init_report =
        anolis_provider_sim::DeviceFactory::initialize_from_config(config);
    sim_runtime::set_startup_report(init_report);
    PSIM_LOG_INFO(
        "Main",
        "startup_policy=" +
            std::string(config.startup_policy ==
                                anolis_provider_sim::StartupPolicy::Strict
                            ? "strict"
                            : "degraded"));
    PSIM_LOG_INFO(
        "Main",
        "initialized " +
            std::to_string(init_report.successful_device_ids.size()) + " / " +
            std::to_string(init_report.configured_device_count) + " devices");
    if (!init_report.failed_devices.empty()) {
      for (const auto &failure : init_report.failed_devices) {
        PSIM_LOG_WARN(
            "Main", "degraded init failure: device_id=" + failure.device_id +
                        " type=" + failure.type + " reason=" + failure.reason);
      }
    }

    if (config.simulation_mode != anolis_provider_sim::SimulationMode::Sim &&
        sim_server_address) {
      PSIM_LOG_WARN("Main", "--sim-server ignored for non-sim mode");
    }

    auto engine = create_engine(config, sim_server_address.value_or(""));
    engine->set_provider_id(config.provider_name.value_or("provider-sim"));

    if (config.simulation_mode == anolis_provider_sim::SimulationMode::Sim) {
      std::filesystem::path config_dir =
          std::filesystem::path(config.config_file_path).parent_path();
      std::filesystem::path physics_path =
          config_dir / *config.physics_config_path;
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
    if (config.simulation_mode ==
        anolis_provider_sim::SimulationMode::NonInteracting) {
      PSIM_LOG_INFO("Main", "mode=non-interacting: auto-starting physics "
                            "ticker");
      sim_devices::start_physics();
    } else {
      std::string mode_str =
          (config.simulation_mode == anolis_provider_sim::SimulationMode::Sim)
              ? "sim"
              : "inert";
      PSIM_LOG_INFO("Main",
                    "mode=" + mode_str +
                        ": deferring physics ticker until wait_ready()");
    }

  } catch (const std::exception &e) {
    PSIM_LOG_ERROR("Main", "FATAL: Failed to initialize simulation: " +
                               std::string(e.what()));
    return 1;
  }

  set_binary_mode_stdio();
  PSIM_LOG_INFO("Main", "starting (transport=stdio+uint32_le)");

  if (crash_after_sec > 0.0) {
    PSIM_LOG_WARN("Main", "CHAOS MODE: will crash after " +
                              std::to_string(crash_after_sec) + " seconds");
    std::thread([crash_after_sec]() {
      std::this_thread::sleep_for(std::chrono::milliseconds(
          static_cast<long long>(crash_after_sec * 1000)));
      PSIM_LOG_ERROR("Main", "CRASHING NOW (exit 42)");
      std::exit(42);
    }).detach();
  }

  std::vector<uint8_t> frame;
  std::string io_err;

  while (true) {
    frame.clear();
    const bool ok = transport::read_frame(std::cin, frame, io_err);
    if (!ok) {
      if (io_err.empty()) {
        PSIM_LOG_INFO("Main", "EOF on stdin; exiting cleanly");
        sim_devices::stop_physics();
        return 0;
      }
      PSIM_LOG_ERROR("Main", std::string("read_frame error: ") + io_err);
      sim_devices::stop_physics();
      return 2;
    }

    anolis::deviceprovider::v1::Request req;
    if (!req.ParseFromArray(frame.data(), static_cast<int>(frame.size()))) {
      PSIM_LOG_ERROR("Main", "failed to parse Request protobuf");
      return 3;
    }

    anolis::deviceprovider::v1::Response resp;
    resp.set_request_id(req.request_id());
    resp.mutable_status()->set_code(
        anolis::deviceprovider::v1::Status::CODE_INTERNAL);
    resp.mutable_status()->set_message("uninitialized");

    if (req.has_hello()) {
      handlers::handle_hello(req.hello(), resp);
    } else if (req.has_wait_ready()) {
      handlers::handle_wait_ready(req.wait_ready(), resp);
      PSIM_LOG_INFO("Main", "waiting ready -> starting physics ticker");
      sim_devices::start_physics();
      PSIM_LOG_INFO("Main", "physics ticker started");
    } else if (req.has_list_devices()) {
      handlers::handle_list_devices(req.list_devices(), resp);
    } else if (req.has_describe_device()) {
      handlers::handle_describe_device(req.describe_device(), resp);
    } else if (req.has_read_signals()) {
      handlers::handle_read_signals(req.read_signals(), resp);
    } else if (req.has_call()) {
      handlers::handle_call(req.call(), resp);
    } else if (req.has_get_health()) {
      handlers::handle_get_health(req.get_health(), resp);
    } else {
      handlers::handle_unimplemented(resp);
    }

    std::string resp_bytes;
    if (!resp.SerializeToString(&resp_bytes)) {
      PSIM_LOG_ERROR("Main", "failed to serialize Response protobuf");
      return 4;
    }

    if (!transport::write_frame(
            std::cout, reinterpret_cast<const uint8_t *>(resp_bytes.data()),
            resp_bytes.size(), io_err)) {
      PSIM_LOG_ERROR("Main", std::string("write_frame error: ") + io_err);
      return 5;
    }
  }
}
