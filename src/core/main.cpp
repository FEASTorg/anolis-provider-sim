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
#include "core/transport/framed_stdio.hpp"
#include "devices/common/device_factory.hpp"
#include "devices/common/device_manager.hpp"
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

static void log_err(const std::string &msg) {
  std::cerr << "anolis-provider-sim: " << msg << "\n";
}

static std::unique_ptr<sim_engine::SimulationEngine>
create_engine(const anolis_provider_sim::ProviderConfig &config,
              const std::string &sim_server_address) {
  using anolis_provider_sim::SimulationMode;

  switch (config.simulation_mode) {
  case SimulationMode::Inert:
    log_err("mode=inert (no simulation)");
    return std::make_unique<sim_engine::NullEngine>();

  case SimulationMode::NonInteracting:
    log_err("mode=non_interacting (local physics)");
    return std::make_unique<sim_engine::LocalEngine>();

  case SimulationMode::Sim:
#ifdef HAVE_FLUXGRAPH
    if (sim_server_address.empty()) {
      throw std::runtime_error("mode=sim requires --sim-server <host:port>");
    }
    log_err("mode=sim (external simulation at " + sim_server_address + ")");
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
        log_err("invalid --crash-after value");
        return 1;
      }
    } else if (arg == "--sim-server" && i + 1 < argc) {
      sim_server_address = argv[++i];
    }
  }

  if (!config_path) {
    log_err("FATAL: --config argument is required");
    log_err("Usage: anolis-provider-sim --config <path/to/config.yaml> "
            "[--sim-server <host:port>]");
    return 1;
  }

  anolis_provider_sim::ProviderConfig config;
  try {
    log_err("loading configuration from: " + *config_path);
    config = anolis_provider_sim::load_config(*config_path);

    int initialized =
        anolis_provider_sim::DeviceFactory::initialize_from_config(config);
    log_err("initialized " + std::to_string(initialized) +
            " devices from config");

    if (config.simulation_mode != anolis_provider_sim::SimulationMode::Sim &&
        sim_server_address) {
      log_err("WARNING: --sim-server ignored for non-sim mode");
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

    std::vector<std::string> device_ids;
    device_ids.reserve(config.devices.size());
    for (const auto &device : config.devices) {
      device_ids.push_back(device.id);
    }
    engine->register_devices(device_ids);

    sim_devices::set_simulation_engine(std::move(engine));
    sim_devices::initialize_physics(config);

    // Start physics automatically for non-interacting mode only.
    // For sim mode, wait_ready() will start physics after all providers
    // have registered to prevent phase misalignment in multi-provider
    // scenarios.
    if (config.simulation_mode ==
        anolis_provider_sim::SimulationMode::NonInteracting) {
      log_err("mode=non-interacting: auto-starting physics ticker");
      sim_devices::start_physics();
    } else {
      std::string mode_str =
          (config.simulation_mode == anolis_provider_sim::SimulationMode::Sim)
              ? "sim"
              : "inert";
      log_err("mode=" + mode_str +
              ": deferring physics ticker until wait_ready()");
    }

  } catch (const std::exception &e) {
    log_err("FATAL: Failed to initialize simulation: " + std::string(e.what()));
    return 1;
  }

  set_binary_mode_stdio();
  log_err("starting (transport=stdio+uint32_le)");

  if (crash_after_sec > 0.0) {
    log_err("CHAOS MODE: will crash after " + std::to_string(crash_after_sec) +
            " seconds");
    std::thread([crash_after_sec]() {
      std::this_thread::sleep_for(std::chrono::milliseconds(
          static_cast<long long>(crash_after_sec * 1000)));
      std::cerr << "anolis-provider-sim: CRASHING NOW (exit 42)\n"
                << std::flush;
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
        log_err("EOF on stdin; exiting cleanly");
        sim_devices::stop_physics();
        return 0;
      }
      log_err(std::string("read_frame error: ") + io_err);
      sim_devices::stop_physics();
      return 2;
    }

    anolis::deviceprovider::v1::Request req;
    if (!req.ParseFromArray(frame.data(), static_cast<int>(frame.size()))) {
      log_err("failed to parse Request protobuf");
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
      log_err("waiting ready -> starting physics ticker");
      sim_devices::start_physics();
      log_err("physics ticker started");
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
      log_err("failed to serialize Response protobuf");
      return 4;
    }

    if (!transport::write_frame(
            std::cout, reinterpret_cast<const uint8_t *>(resp_bytes.data()),
            resp_bytes.size(), io_err)) {
      log_err(std::string("write_frame error: ") + io_err);
      return 5;
    }
  }
}
