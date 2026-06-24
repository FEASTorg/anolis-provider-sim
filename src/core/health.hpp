#pragma once

/**
 * @file health.hpp
 * @brief Health projection helpers derived from the sim provider startup
 * report.
 */

#include <string>
#include <vector>

#include "core/startup_report.hpp"
#include "protocol.pb.h"

namespace anolis_provider_sim::health {

using Device = anolis::deviceprovider::v1::Device;
using DeviceHealth = anolis::deviceprovider::v1::DeviceHealth;
using ProviderHealth = anolis::deviceprovider::v1::ProviderHealth;
using DeviceInitializationReport = anolis_provider_sim::DeviceInitializationReport;
using StartupPolicy = anolis_provider_sim::StartupPolicy;

/** @brief Convert a startup policy enum to its stable diagnostics string. */
std::string startup_policy_name(StartupPolicy policy);

/** @brief Build provider-level health from the recorded startup outcome. */
ProviderHealth make_provider_health(const DeviceInitializationReport &report);

/** @brief Mark the currently listed devices healthy for `ListDevices` health
 * output. */
std::vector<DeviceHealth> make_list_devices_health(const std::vector<Device> &devices);

/**
 * @brief Build `GetHealth` device entries from live roster plus startup
 * failures.
 */
std::vector<DeviceHealth> make_get_health_devices(const DeviceInitializationReport &report);

}  // namespace anolis_provider_sim::health
