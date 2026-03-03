#include "core/runtime_state.hpp"

#include <mutex>

namespace sim_runtime {

namespace {

std::mutex g_runtime_state_mutex;
RuntimeState g_runtime_state;

} // namespace

void reset() {
  std::lock_guard<std::mutex> lock(g_runtime_state_mutex);
  g_runtime_state = {};
}

void set_startup_report(
    const anolis_provider_sim::DeviceInitializationReport &report) {
  std::lock_guard<std::mutex> lock(g_runtime_state_mutex);
  g_runtime_state.startup_report = report;
  g_runtime_state.startup_report_available = true;
}

RuntimeState snapshot() {
  std::lock_guard<std::mutex> lock(g_runtime_state_mutex);
  return g_runtime_state;
}

} // namespace sim_runtime
