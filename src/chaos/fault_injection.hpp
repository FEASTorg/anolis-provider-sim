#pragma once

#include <chrono>
#include <map>
#include <string>

namespace sim_devices {
namespace fault_injection {

// Fault types
struct DeviceUnavailableFault {
  std::chrono::steady_clock::time_point expires_at;
};

struct SignalFault {
  std::string signal_id;
  std::chrono::steady_clock::time_point expires_at;
};

struct CallLatencyFault {
  int64_t latency_ms;
};

struct CallFailureFault {
  std::string function_id;
  double failure_rate; // 0.0 to 1.0
};

// Initialize fault injection system
void init();

// Clear all faults
void clear_all_faults();

// Device unavailable faults
void inject_device_unavailable(const std::string &device_id,
                               int64_t duration_ms);
bool is_device_unavailable(const std::string &device_id);

// Signal faults
void inject_signal_fault(const std::string &device_id,
                         const std::string &signal_id, int64_t duration_ms);
bool is_signal_faulted(const std::string &device_id,
                       const std::string &signal_id);

// Call latency faults
void inject_call_latency(const std::string &device_id, int64_t latency_ms);
int64_t get_call_latency(const std::string &device_id);

// Call failure faults
void inject_call_failure(const std::string &device_id,
                         const std::string &function_id, double failure_rate);
bool should_call_fail(const std::string &device_id,
                      const std::string &function_id);

} // namespace fault_injection
} // namespace sim_devices
