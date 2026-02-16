#pragma once

#include "../config.hpp"
#include "model_interface.hpp"
#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <thread>
#include <vector>

// Forward declaration to avoid circular dependency
namespace sim_coordination {
class ISignalSource;
}

namespace sim_physics {

// Transform primitive state
struct FirstOrderLagState {
  double tau_s;
  double current_value;
  bool initialized = false;
};

struct NoiseState {
  double amplitude;
  int seed;
  std::mt19937 rng;
};

struct SaturationState {
  double min_val;
  double max_val;
};

struct LinearState {
  double scale;
  double offset;
  std::optional<double> clamp_min;
  std::optional<double> clamp_max;
};

struct DeadbandState {
  double threshold;
  double last_output;
  bool initialized = false;
};

struct RateLimiterState {
  double max_rate_per_sec;
  double last_value;
  bool initialized = false;
};

struct DelayState {
  double delay_sec;
  std::vector<std::pair<double, double>> buffer; // (time, value) pairs
};

struct MovingAverageState {
  int window_size;
  std::vector<double> buffer;
  int buffer_index = 0;
  double sum = 0.0;
  bool filled = false;
};

// Core physics engine
class SimPhysics {
public:
  // Constructor takes ISignalSource interface for device/model signal access
  // NOTE: signal_source is NOT owned by SimPhysics, caller manages lifetime
  SimPhysics(sim_coordination::ISignalSource *signal_source,
             const anolis_provider_sim::ProviderConfig &provider_config,
             const anolis_provider_sim::PhysicsConfig &physics_config);
  ~SimPhysics();

  // Start/stop ticker thread
  void start();
  void stop();

  // Read model signal value (thread-safe)
  std::optional<double> read_model_signal(const std::string &model_id,
                                           const std::string &signal_id);

  // Set callback for rule evaluation (called from ticker thread)
  void set_rule_callback(std::function<void()> callback) {
    rule_evaluation_callback_ = callback;
  }

private:
  // Ticker thread function
  void ticker_thread();

  // Read signal from source (device or model) via interface
  // pending_writes: if provided, check this map first for uncommitted values
  std::optional<double> read_signal_value(
      const std::string &path,
      const std::map<std::string, double> *pending_writes = nullptr);

  // Apply transform primitive to value
  double apply_transform(const anolis_provider_sim::GraphEdge &edge,
                         double input_value, double dt);

  // Signal source interface (not owned)
  sim_coordination::ISignalSource *signal_source_;

  // Configuration
  anolis_provider_sim::SimulationMode mode_;
  double tick_rate_hz_;
  anolis_provider_sim::PhysicsConfig physics_config_;

  // Models
  std::map<std::string, std::unique_ptr<PhysicsModel>> models_;
  std::map<std::string, std::map<std::string, double>>
      model_outputs_; // model_id -> signal_id -> value

  // Transform state (indexed by source->target pair)
  std::map<std::pair<std::string, std::string>, FirstOrderLagState>
      first_order_lag_states_;
  std::map<std::pair<std::string, std::string>, NoiseState> noise_states_;
  std::map<std::pair<std::string, std::string>, SaturationState>
      saturation_states_;
  std::map<std::pair<std::string, std::string>, LinearState> linear_states_;
  std::map<std::pair<std::string, std::string>, DeadbandState>
      deadband_states_;
  std::map<std::pair<std::string, std::string>, RateLimiterState>
      rate_limiter_states_;
  std::map<std::pair<std::string, std::string>, DelayState> delay_states_;
  std::map<std::pair<std::string, std::string>, MovingAverageState>
      moving_average_states_;

  // Hook for external rule engine (called from ticker without holding locks)
  std::function<void()> rule_evaluation_callback_;

  // Ticker thread
  std::unique_ptr<std::thread> ticker_thread_;
  std::atomic<bool> ticker_running_{false};

  // Synchronization
  mutable std::mutex physics_mutex_;
};

} // namespace sim_physics
