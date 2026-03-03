#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "config.hpp"

namespace anolis_provider_sim {

struct DeviceInitFailure {
  std::string device_id;
  std::string type;
  std::string reason;
};

struct DeviceInitializationReport {
  std::size_t configured_device_count = 0;
  StartupPolicy startup_policy = StartupPolicy::Strict;
  std::vector<std::string> configured_device_ids;
  std::vector<std::string> successful_device_ids;
  std::vector<DeviceInitFailure> failed_devices;
};

} // namespace anolis_provider_sim
