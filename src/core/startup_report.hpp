#pragma once

/**
 * @file startup_report.hpp
 * @brief Startup-report types used to project readiness and health in
 * provider-sim.
 */

#include <cstddef>
#include <string>
#include <vector>

#include "config.hpp"

namespace anolis_provider_sim {

/**
 * @brief One configured device that failed initialization at startup.
 */
struct DeviceInitFailure {
    std::string device_id;
    std::string type;
    std::string reason;
};

/**
 * @brief Summary of device initialization outcomes for one provider start.
 *
 * The report is captured once during startup and then reused by readiness and
 * health handlers to explain strict versus degraded initialization results.
 */
struct DeviceInitializationReport {
    std::size_t configured_device_count = 0;
    StartupPolicy startup_policy = StartupPolicy::Strict;
    std::vector<std::string> configured_device_ids;
    std::vector<std::string> successful_device_ids;
    std::vector<DeviceInitFailure> failed_devices;
};

}  // namespace anolis_provider_sim
