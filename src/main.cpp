#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include "config_translator.hpp"
#include "config.hpp"
#include "flux_client.hpp"
#include "devices/device_factory.hpp"
#include "devices/device_manager.hpp"
#include "handlers.hpp"
#include "protocol.pb.h"
#include "transport/framed_stdio.hpp"

static void set_binary_mode_stdio() {
#ifdef _WIN32
  _setmode(_fileno(stdin), _O_BINARY);
  _setmode(_fileno(stdout), _O_BINARY);
#endif
}

static void log_err(const std::string &msg) {
  std::cerr << "anolis-provider-sim: " << msg << "\n";
}

int main(int argc, char **argv) {
  // Parse command-line arguments
  std::optional<std::string> config_path;
  std::optional<std::string> flux_server_address;
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
    } else if (arg == "--flux-server" && i + 1 < argc) {
      flux_server_address = argv[++i];
    }
  }

  // Require configuration file
  if (!config_path) {
    log_err("FATAL: --config argument is required");
    log_err("Usage: anolis-provider-sim --config <path/to/config.yaml> "
            "[--flux-server <host:port>]");
    return 1;
  }

  // Load configuration
  anolis_provider_sim::ProviderConfig config;
  std::unique_ptr<sim_flux::FluxGraphClient> flux_client;
  try {
    log_err("loading configuration from: " + *config_path);
    config = anolis_provider_sim::load_config(*config_path);
    int initialized =
        anolis_provider_sim::DeviceFactory::initialize_from_config(config);
    log_err("initialized " + std::to_string(initialized) +
            " devices from config");

    if (config.simulation_mode == anolis_provider_sim::SimulationMode::Physics) {
      if (!flux_server_address) {
        log_err("FATAL: mode=physics requires --flux-server <host:port>");
        return 1;
      }

      log_err("connecting to FluxGraph server at: " + *flux_server_address);
      flux_client = std::make_unique<sim_flux::FluxGraphClient>(*flux_server_address);

      std::filesystem::path config_dir =
          std::filesystem::path(config.config_file_path).parent_path();
      std::filesystem::path physics_path =
          config_dir / *config.physics_config_path;

      log_err("translating physics config for FluxGraph: " + physics_path.string());
      std::string fluxgraph_yaml =
          sim_config::translate_to_fluxgraph_format(physics_path.string());

      flux_client->load_config_content(fluxgraph_yaml);

      std::vector<std::string> device_ids;
      device_ids.reserve(config.devices.size());
      for (const auto &d : config.devices) {
        device_ids.push_back(d.id);
      }
      flux_client->register_provider("provider-sim", device_ids);

      log_err("FluxGraph client initialized and provider registered");
    } else if (flux_server_address) {
      log_err("WARNING: --flux-server ignored for non-physics mode");
    }

    sim_devices::set_flux_client(flux_client.get());

    // Initialize physics engine (but don't start ticker yet - wait for
    // WaitReady)
    sim_devices::initialize_physics(config);
  } catch (const std::exception &e) {
    log_err("FATAL: Failed to load configuration: " + std::string(e.what()));
    return 1;
  }

  set_binary_mode_stdio();
  log_err("starting (transport=stdio+uint32_le)");

  // Start crash timer if requested (for supervision testing)
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
        sim_devices::stop_physics(); // Clean shutdown
        return 0;
      }
      log_err(std::string("read_frame error: ") + io_err);
      sim_devices::stop_physics(); // Clean shutdown
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

    // Dispatch
    if (req.has_hello()) {
      handlers::handle_hello(req.hello(), resp);
    } else if (req.has_wait_ready()) {
      handlers::handle_wait_ready(req.wait_ready(), resp);
      // Start physics ticker after WaitReady completes
      sim_devices::start_physics();
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
