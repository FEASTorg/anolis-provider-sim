#pragma once

#include "core/startup_report.hpp"

namespace sim_runtime {

// Process-wide runtime snapshot used by handlers for diagnostics/health.
struct RuntimeState {
  anolis_provider_sim::DeviceInitializationReport startup_report;
  bool startup_report_available = false;
};

void reset();

void set_startup_report(
    const anolis_provider_sim::DeviceInitializationReport &report);

RuntimeState snapshot();

} // namespace sim_runtime
