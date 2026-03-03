#include "core/health.hpp"

#include <cstddef>
#include <set>
#include <string>

#include "devices/common/device_manager.hpp"

namespace sim_health {

std::string startup_policy_name(StartupPolicy policy) {
  switch (policy) {
  case StartupPolicy::Strict:
    return "strict";
  case StartupPolicy::Degraded:
    return "degraded";
  }
  return "unknown";
}

ProviderHealth make_provider_health(const DeviceInitializationReport &report) {
  ProviderHealth health;
  const std::size_t initialized = report.successful_device_ids.size();
  const std::size_t explicit_failed = report.failed_devices.size();
  const std::size_t inferred_failed =
      report.configured_device_count > initialized
          ? (report.configured_device_count - initialized)
          : 0U;
  const std::size_t failed =
      explicit_failed > inferred_failed ? explicit_failed : inferred_failed;

  if (failed > 0U) {
    health.set_state(ProviderHealth::STATE_DEGRADED);
    health.set_message("startup degraded: " + std::to_string(failed) + " of " +
                       std::to_string(report.configured_device_count) +
                       " devices failed to initialize");
  } else {
    health.set_state(ProviderHealth::STATE_OK);
    health.set_message("ok");
  }

  (*health.mutable_metrics())["impl"] = "sim";
  (*health.mutable_metrics())["startup_policy"] =
      startup_policy_name(report.startup_policy);
  (*health.mutable_metrics())["startup_configured_devices"] =
      std::to_string(report.configured_device_count);
  (*health.mutable_metrics())["startup_initialized_devices"] =
      std::to_string(initialized);
  (*health.mutable_metrics())["startup_failed_devices"] =
      std::to_string(failed);

  return health;
}

static DeviceHealth make_ok_device_health(const std::string &device_id) {
  DeviceHealth health;
  health.set_device_id(device_id);
  health.set_state(DeviceHealth::STATE_OK);
  health.set_message("ok");
  (*health.mutable_metrics())["impl"] = "sim";
  return health;
}

std::vector<DeviceHealth>
make_list_devices_health(const std::vector<Device> &devices) {
  std::vector<DeviceHealth> out;
  out.reserve(devices.size());

  for (const auto &device : devices) {
    out.push_back(make_ok_device_health(device.device_id()));
  }

  return out;
}

std::vector<DeviceHealth>
make_get_health_devices(const DeviceInitializationReport &report) {
  std::vector<DeviceHealth> out;
  std::set<std::string> seen_device_ids;
  std::set<std::string> success_ids(report.successful_device_ids.begin(),
                                    report.successful_device_ids.end());

  const auto listed_devices = sim_devices::list_devices(false);
  out.reserve(listed_devices.size() + report.failed_devices.size() +
              report.configured_device_ids.size());

  for (const auto &device : listed_devices) {
    out.push_back(make_ok_device_health(device.device_id()));
    seen_device_ids.insert(device.device_id());
  }

  for (const auto &failed : report.failed_devices) {
    if (seen_device_ids.find(failed.device_id) != seen_device_ids.end()) {
      continue;
    }

    DeviceHealth health;
    health.set_device_id(failed.device_id);
    health.set_state(DeviceHealth::STATE_UNREACHABLE);
    health.set_message("startup initialization failed: " + failed.reason);
    (*health.mutable_metrics())["impl"] = "sim";
    (*health.mutable_metrics())["startup_failure"] = "true";
    (*health.mutable_metrics())["device_type"] = failed.type;
    out.push_back(health);
    seen_device_ids.insert(failed.device_id);
  }

  for (const auto &configured_id : report.configured_device_ids) {
    if (seen_device_ids.find(configured_id) != seen_device_ids.end()) {
      continue;
    }
    if (success_ids.find(configured_id) != success_ids.end()) {
      continue;
    }

    DeviceHealth health;
    health.set_device_id(configured_id);
    health.set_state(DeviceHealth::STATE_UNREACHABLE);
    health.set_message("startup initialization failed");
    (*health.mutable_metrics())["impl"] = "sim";
    (*health.mutable_metrics())["startup_failure"] = "true";
    out.push_back(health);
  }

  return out;
}

} // namespace sim_health
