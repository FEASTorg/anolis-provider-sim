#pragma once

/**
 * @file runtime_state.hpp
 * @brief Process-wide startup-report snapshot exposed to ADPP handlers.
 */

#include "core/startup_report.hpp"

namespace sim_runtime {

/**
 * @brief Lightweight runtime snapshot used for readiness and health reporting.
 *
 * The sim provider keeps only startup diagnostics here; device state lives in
 * the device modules and physics coordination layer.
 */
struct RuntimeState {
  anolis_provider_sim::DeviceInitializationReport startup_report;
  bool startup_report_available = false;
};

/** @brief Clear the process-wide runtime snapshot. */
void reset();

/** @brief Publish the latest startup report for handler-side diagnostics. */
void set_startup_report(
    const anolis_provider_sim::DeviceInitializationReport &report);

/** @brief Return a copy of the current runtime snapshot. */
RuntimeState snapshot();

} // namespace sim_runtime
