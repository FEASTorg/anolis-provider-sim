#pragma once

/**
 * @file fault_injection.hpp
 * @brief Process-wide fault injection state used by the chaos control device.
 */

#include <chrono>
#include <map>
#include <string>

namespace sim_devices {
namespace fault_injection {

/** @brief Temporary device-unavailable fault with an absolute expiry time. */
struct DeviceUnavailableFault {
    std::chrono::steady_clock::time_point expires_at;
};

/** @brief Temporary per-signal fault with an absolute expiry time. */
struct SignalFault {
    std::string signal_id;
    std::chrono::steady_clock::time_point expires_at;
};

/** @brief Fixed added call latency for all functions on one device. */
struct CallLatencyFault {
    int64_t latency_ms;
};

/** @brief Probabilistic call-failure rule for one device/function pair. */
struct CallFailureFault {
    std::string function_id;
    double failure_rate;  // 0.0 to 1.0
};

/** @brief Initialize the fault injection subsystem singleton. */
void init();

/** @brief Clear every injected fault across every device. */
void clear_all_faults();

/** @brief Make one device unavailable until the duration expires. */
void inject_device_unavailable(const std::string &device_id, int64_t duration_ms);
/** @brief Report whether one device is currently faulted unavailable. */
bool is_device_unavailable(const std::string &device_id);

/** @brief Make one signal report faulted state until the duration expires. */
void inject_signal_fault(const std::string &device_id, const std::string &signal_id, int64_t duration_ms);
/** @brief Report whether one device signal is currently faulted. */
bool is_signal_faulted(const std::string &device_id, const std::string &signal_id);

/** @brief Add artificial call latency for one device. */
void inject_call_latency(const std::string &device_id, int64_t latency_ms);
/** @brief Return the currently configured artificial call latency for one
 * device. */
int64_t get_call_latency(const std::string &device_id);

/** @brief Inject or replace a probabilistic call failure rule for one function.
 */
void inject_call_failure(const std::string &device_id, const std::string &function_id, double failure_rate);
/** @brief Sample whether a call should fail under the current fault rules. */
bool should_call_fail(const std::string &device_id, const std::string &function_id);

}  // namespace fault_injection
}  // namespace sim_devices
