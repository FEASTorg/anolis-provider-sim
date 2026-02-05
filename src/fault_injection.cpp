#include "fault_injection.hpp"

#include <algorithm>
#include <random>
#include <mutex>

namespace sim_devices
{
    namespace fault_injection
    {

        // -----------------------------
        // State storage
        // -----------------------------

        struct State
        {
            std::map<std::string, DeviceUnavailableFault> device_unavailable_faults;
            std::map<std::string, std::vector<SignalFault>> signal_faults; // device_id -> signal faults
            std::map<std::string, CallLatencyFault> call_latency_faults;
            std::map<std::string, std::vector<CallFailureFault>> call_failure_faults; // device_id -> function faults

            std::mt19937 rng{std::random_device{}()};
            std::mutex mutex;
        };

        static State &state()
        {
            static State s;
            return s;
        }

        // -----------------------------
        // Initialization
        // -----------------------------

        void init()
        {
            // Already initialized via static
        }

        // -----------------------------
        // Clear all faults
        // -----------------------------

        void clear_all_faults()
        {
            State &s = state();
            std::lock_guard<std::mutex> lock(s.mutex);
            s.device_unavailable_faults.clear();
            s.signal_faults.clear();
            s.call_latency_faults.clear();
            s.call_failure_faults.clear();
        }

        // -----------------------------
        // Device unavailable faults
        // -----------------------------

        void inject_device_unavailable(const std::string &device_id, int64_t duration_ms)
        {
            State &s = state();
            std::lock_guard<std::mutex> lock(s.mutex);
            DeviceUnavailableFault fault;
            fault.expires_at = std::chrono::steady_clock::now() + std::chrono::milliseconds(duration_ms);
            s.device_unavailable_faults[device_id] = fault;
        }

        bool is_device_unavailable(const std::string &device_id)
        {
            State &s = state();
            std::lock_guard<std::mutex> lock(s.mutex);
            auto it = s.device_unavailable_faults.find(device_id);
            if (it == s.device_unavailable_faults.end())
            {
                return false;
            }

            // Check expiration
            if (std::chrono::steady_clock::now() >= it->second.expires_at)
            {
                s.device_unavailable_faults.erase(it);
                return false;
            }

            return true;
        }

        // -----------------------------
        // Signal faults
        // -----------------------------

        void inject_signal_fault(const std::string &device_id, const std::string &signal_id, int64_t duration_ms)
        {
            State &s = state();
            std::lock_guard<std::mutex> lock(s.mutex);
            SignalFault fault;
            fault.signal_id = signal_id;
            fault.expires_at = std::chrono::steady_clock::now() + std::chrono::milliseconds(duration_ms);
            s.signal_faults[device_id].push_back(fault);
        }

        bool is_signal_faulted(const std::string &device_id, const std::string &signal_id)
        {
            State &s = state();
            std::lock_guard<std::mutex> lock(s.mutex);
            auto it = s.signal_faults.find(device_id);
            if (it == s.signal_faults.end())
            {
                return false;
            }

            // Check all signal faults for this device
            auto now = std::chrono::steady_clock::now();
            auto &faults = it->second;

            // Remove expired faults
            faults.erase(
                std::remove_if(faults.begin(), faults.end(),
                               [now](const SignalFault &f)
                               { return now >= f.expires_at; }),
                faults.end());

            // Check if signal is faulted
            for (const auto &fault : faults)
            {
                if (fault.signal_id == signal_id)
                {
                    return true;
                }
            }

            return false;
        }

        // -----------------------------
        // Call latency faults
        // -----------------------------

        void inject_call_latency(const std::string &device_id, int64_t latency_ms)
        {
            State &s = state();
            std::lock_guard<std::mutex> lock(s.mutex);
            CallLatencyFault fault;
            fault.latency_ms = latency_ms;
            s.call_latency_faults[device_id] = fault;
        }

        int64_t get_call_latency(const std::string &device_id)
        {
            State &s = state();
            std::lock_guard<std::mutex> lock(s.mutex);
            auto it = s.call_latency_faults.find(device_id);
            if (it == s.call_latency_faults.end())
            {
                return 0;
            }
            return it->second.latency_ms;
        }

        // -----------------------------
        // Call failure faults
        // -----------------------------

        void inject_call_failure(const std::string &device_id, const std::string &function_id, double failure_rate)
        {
            State &s = state();
            std::lock_guard<std::mutex> lock(s.mutex);
            CallFailureFault fault;
            fault.function_id = function_id;
            fault.failure_rate = std::clamp(failure_rate, 0.0, 1.0);

            // Replace existing fault for this function, or add new one
            auto &faults = s.call_failure_faults[device_id];
            auto it = std::find_if(faults.begin(), faults.end(),
                                   [&function_id](const CallFailureFault &f)
                                   { return f.function_id == function_id; });

            if (it != faults.end())
            {
                it->failure_rate = fault.failure_rate;
            }
            else
            {
                faults.push_back(fault);
            }
        }

        bool should_call_fail(const std::string &device_id, const std::string &function_id)
        {
            State &s = state();
            std::lock_guard<std::mutex> lock(s.mutex);
            auto it = s.call_failure_faults.find(device_id);
            if (it == s.call_failure_faults.end())
            {
                return false;
            }

            // Find fault for this function
            for (const auto &fault : it->second)
            {
                if (fault.function_id == function_id)
                {
                    // Roll the dice
                    std::uniform_real_distribution<double> dist(0.0, 1.0);
                    return dist(s.rng) < fault.failure_rate;
                }
            }

            return false;
        }

    } // namespace fault_injection
} // namespace sim_devices
