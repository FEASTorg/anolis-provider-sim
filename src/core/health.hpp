#pragma once

#include <string>
#include <vector>

#include "core/startup_report.hpp"
#include "protocol.pb.h"

namespace sim_health {

using Device = anolis::deviceprovider::v1::Device;
using DeviceHealth = anolis::deviceprovider::v1::DeviceHealth;
using ProviderHealth = anolis::deviceprovider::v1::ProviderHealth;
using DeviceInitializationReport =
    anolis_provider_sim::DeviceInitializationReport;
using StartupPolicy = anolis_provider_sim::StartupPolicy;

std::string startup_policy_name(StartupPolicy policy);

ProviderHealth make_provider_health(const DeviceInitializationReport &report);

std::vector<DeviceHealth>
make_list_devices_health(const std::vector<Device> &devices);

std::vector<DeviceHealth>
make_get_health_devices(const DeviceInitializationReport &report);

} // namespace sim_health
