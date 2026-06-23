#include "local_engine.hpp"

#include <algorithm>

#include "devices/analogsensor/analogsensor_device.hpp"
#include "devices/common/device_adapter.hpp"
#include "devices/common/device_factory.hpp"
#include "devices/motorctl/motorctl_device.hpp"
#include "devices/relayio/relayio_device.hpp"
#include "devices/tempctl/tempctl_device.hpp"

namespace sim_engine {

void LocalEngine::initialize(const std::string &) { last_update_ = std::chrono::steady_clock::now(); }

void LocalEngine::register_devices(const std::vector<std::string> &) {
    // No explicit registration required for local engine.
}

TickResult LocalEngine::tick(const std::map<std::string, double> &) {
    const auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = now - last_update_;
    double dt = elapsed.count();
    if (dt <= 0.0) {
        return TickResult{false, {}, {}};
    }

    dt = std::min(dt, 0.25);
    last_update_ = now;

    if (anolis_provider_sim::DeviceFactory::is_config_loaded()) {
        auto devices = anolis_provider_sim::DeviceFactory::get_registered_devices();
        for (const auto &dev : devices) {
            if (const auto type = sim_devices::sim_device_type_from_string(dev.type)) {
                sim_devices::adapter_for(*type).update_physics(dev.id, dt);
            }
        }
        return TickResult{true, {}, {}};
    }

    // Fallback singleton behavior (legacy path).
    sim_devices::tempctl::update_physics(sim_devices::tempctl::kDeviceId, dt);
    sim_devices::motorctl::update_physics(sim_devices::motorctl::kDeviceId, dt);
    sim_devices::relayio::update_physics(sim_devices::relayio::kDeviceId, dt);
    sim_devices::analogsensor::update_physics(sim_devices::analogsensor::kDeviceId, dt);

    return TickResult{true, {}, {}};
}

}  // namespace sim_engine
