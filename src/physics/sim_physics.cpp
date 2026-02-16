#include "sim_physics.hpp"
#include "../devices/signal_source.hpp"
#include "model_registry.hpp"
#include <chrono>
#include <cmath>
#include <iostream>
#include <random>
#include <thread>

namespace sim_physics {

SimPhysics::SimPhysics(
    sim_coordination::ISignalSource *signal_source,
    const anolis_provider_sim::ProviderConfig &provider_config,
    const anolis_provider_sim::PhysicsConfig &physics_config)
    : signal_source_(signal_source), mode_(provider_config.simulation_mode),
      tick_rate_hz_(provider_config.tick_rate_hz.value_or(10.0)),
      physics_config_(physics_config) {

  std::cerr << "[SimPhysics] Initializing physics engine (mode=";
  switch (mode_) {
  case anolis_provider_sim::SimulationMode::NonInteracting:
    std::cerr << "non_interacting";
    break;
  case anolis_provider_sim::SimulationMode::Inert:
    std::cerr << "inert";
    break;
  case anolis_provider_sim::SimulationMode::Physics:
    std::cerr << "physics";
    break;
  }
  std::cerr << ", tick_rate=" << tick_rate_hz_ << " Hz)" << std::endl;

  // Only initialize physics engine components for physics mode
  if (mode_ != anolis_provider_sim::SimulationMode::Physics) {
    return;
  }

  // Initialize models
  for (const auto &model_spec : physics_config_.models) {
    std::cerr << "[SimPhysics] Creating model: " << model_spec.id
              << " (type: " << model_spec.type << ")" << std::endl;

    auto model = create_model(model_spec.type);
    model->init(model_spec.params);

    // Initialize output storage
    for (const auto &output_name : model->output_names()) {
      model_outputs_[model_spec.id][output_name] = 0.0;
    }

    models_[model_spec.id] = std::move(model);
  }

  // Initialize transform state
  for (const auto &edge : physics_config_.signal_graph) {
    auto edge_key = std::make_pair(edge.source, edge.target);

    if (edge.transform_type) {
      switch (*edge.transform_type) {
      case anolis_provider_sim::TransformType::FirstOrderLag: {
        FirstOrderLagState state;
        state.tau_s = edge.transform_params.at("tau_s").as<double>();
        if (edge.transform_params.find("initial_value") !=
            edge.transform_params.end()) {
          state.current_value =
              edge.transform_params.at("initial_value").as<double>();
          state.initialized = true;
        }
        first_order_lag_states_[edge_key] = state;
        break;
      }
      case anolis_provider_sim::TransformType::Noise: {
        NoiseState state;
        state.amplitude = edge.transform_params.at("amplitude").as<double>();
        state.seed = edge.transform_params.at("seed").as<int>();
        state.rng.seed(state.seed);
        noise_states_[edge_key] = state;
        break;
      }
      case anolis_provider_sim::TransformType::Saturation: {
        SaturationState state;
        state.min_val = edge.transform_params.at("min").as<double>();
        state.max_val = edge.transform_params.at("max").as<double>();
        saturation_states_[edge_key] = state;
        break;
      }
      case anolis_provider_sim::TransformType::Linear: {
        LinearState state;
        state.scale = edge.transform_params.at("scale").as<double>();
        state.offset =
            edge.transform_params.find("offset") != edge.transform_params.end()
                ? edge.transform_params.at("offset").as<double>()
                : 0.0;
        if (edge.transform_params.find("clamp_min") !=
            edge.transform_params.end()) {
          state.clamp_min = edge.transform_params.at("clamp_min").as<double>();
        }
        if (edge.transform_params.find("clamp_max") !=
            edge.transform_params.end()) {
          state.clamp_max = edge.transform_params.at("clamp_max").as<double>();
        }
        linear_states_[edge_key] = state;
        break;
      }
      case anolis_provider_sim::TransformType::Deadband: {
        DeadbandState state;
        state.threshold = edge.transform_params.at("threshold").as<double>();
        deadband_states_[edge_key] = state;
        break;
      }
      case anolis_provider_sim::TransformType::RateLimiter: {
        RateLimiterState state;
        state.max_rate_per_sec =
            edge.transform_params.at("max_rate_per_sec").as<double>();
        rate_limiter_states_[edge_key] = state;
        break;
      }
      case anolis_provider_sim::TransformType::Delay: {
        DelayState state;
        state.delay_sec = edge.transform_params.at("delay_sec").as<double>();
        delay_states_[edge_key] = state;
        break;
      }
      case anolis_provider_sim::TransformType::MovingAverage: {
        MovingAverageState state;
        state.window_size = edge.transform_params.at("window_size").as<int>();
        state.buffer.reserve(state.window_size);
        moving_average_states_[edge_key] = state;
        break;
      }
      }
    }
  }

  std::cerr << "[SimPhysics] Initialized " << models_.size() << " models, "
            << physics_config_.signal_graph.size() << " graph edges, "
            << physics_config_.rules.size() << " rules" << std::endl;
}

SimPhysics::~SimPhysics() { stop(); }

void SimPhysics::start() {
  if (mode_ == anolis_provider_sim::SimulationMode::Inert) {
    std::cerr << "[SimPhysics] Inert mode - no ticker thread" << std::endl;
    return;
  }

  if (ticker_running_.load()) {
    std::cerr << "[SimPhysics] Already running" << std::endl;
    return;
  }

  std::cerr << "[SimPhysics] Starting ticker thread" << std::endl;
  ticker_running_.store(true);
  ticker_thread_ =
      std::make_unique<std::thread>(&SimPhysics::ticker_thread, this);
}

void SimPhysics::stop() {
  if (!ticker_running_.load()) {
    return;
  }

  std::cerr << "[SimPhysics] Stopping ticker thread" << std::endl;
  ticker_running_.store(false);

  if (ticker_thread_ && ticker_thread_->joinable()) {
    ticker_thread_->join();
  }

  std::cerr << "[SimPhysics] Ticker thread stopped" << std::endl;
}

void SimPhysics::ticker_thread() {
  const double dt = 1.0 / tick_rate_hz_;
  const auto tick_duration =
      std::chrono::microseconds(static_cast<long long>(dt * 1e6));

  std::cerr << "[SimPhysics] Ticker thread started (dt=" << dt
            << "s, period=" << tick_duration.count() << "us)" << std::endl;

  while (ticker_running_.load()) {
    auto tick_start = std::chrono::steady_clock::now();

    // For physics mode, evaluate graph and update models
    if (mode_ == anolis_provider_sim::SimulationMode::Physics) {
      std::map<std::string, double>
          device_writes; // Computed device signal updates

      {
        std::lock_guard<std::mutex> lock(physics_mutex_);

        // Evaluate signal graph
        std::map<std::string, double> model_inputs; // model target -> value

        for (const auto &edge : physics_config_.signal_graph) {
          // Read source value (from device or model)
          // Pass device_writes so we can read uncommitted values from this tick
          std::optional<double> source_value =
              read_signal_value(edge.source, &device_writes);

          if (!source_value) {
            continue; // Skip edge if source not available
          }

          // Apply transform if present
          double value = *source_value;
          if (edge.transform_type) {
            value = apply_transform(edge, value, dt);
          }

          // Route to destination: check if target is model or device
          size_t slash_pos = edge.target.find('/');
          if (slash_pos != std::string::npos) {
            std::string target_id = edge.target.substr(0, slash_pos);

            // If target is a model, store in model_inputs
            if (models_.find(target_id) != models_.end()) {
              model_inputs[edge.target] = value;
            } else {
              // Target is a device signal - queue for writing after lock
              // release
              device_writes[edge.target] = value;
            }
          }
        }

        // Update each model
        for (auto &[model_id, model] : models_) {
          // Gather inputs for this model
          std::map<std::string, double> inputs;
          for (const auto &input_name : model->input_names()) {
            std::string target = model_id + "/" + input_name;
            if (model_inputs.find(target) != model_inputs.end()) {
              inputs[input_name] = model_inputs[target];
            }
          }

          // Update model
          std::map<std::string, double> outputs;
          model->update(dt, inputs, outputs);

          // Store outputs
          model_outputs_[model_id] = outputs;
        }

        // Lock released here when lock_guard goes out of scope
      }

      // Write device signals WITHOUT holding physics lock (avoid deadlocks)
      // SignalRegistry has its own mutex for thread safety
      for (const auto &[path, value] : device_writes) {
        signal_source_->write_signal(path, value);
      }

      // Call rule evaluation callback if set (external rule engine)
      if (rule_evaluation_callback_) {
        rule_evaluation_callback_();
      }
    }

    // Sleep until next tick
    auto tick_end = std::chrono::steady_clock::now();
    auto elapsed = tick_end - tick_start;
    if (elapsed < tick_duration) {
      std::this_thread::sleep_for(tick_duration - elapsed);
    }
  }

  std::cerr << "[SimPhysics] Ticker thread exiting" << std::endl;
}

double SimPhysics::apply_transform(const anolis_provider_sim::GraphEdge &edge,
                                   double input_value, double dt) {
  auto edge_key = std::make_pair(edge.source, edge.target);

  switch (*edge.transform_type) {
  case anolis_provider_sim::TransformType::FirstOrderLag: {
    auto &state = first_order_lag_states_[edge_key];
    if (!state.initialized) {
      state.current_value = input_value;
      state.initialized = true;
    }
    // First-order low-pass filter: dx/dt = (input - x) / tau
    double alpha = dt / (state.tau_s + dt);
    state.current_value =
        state.current_value + alpha * (input_value - state.current_value);
    return state.current_value;
  }

  case anolis_provider_sim::TransformType::Noise: {
    auto &state = noise_states_[edge_key];
    std::normal_distribution<double> dist(0.0, state.amplitude);
    return input_value + dist(state.rng);
  }

  case anolis_provider_sim::TransformType::Saturation: {
    auto &state = saturation_states_[edge_key];
    return std::max(state.min_val, std::min(state.max_val, input_value));
  }

  case anolis_provider_sim::TransformType::Linear: {
    auto &state = linear_states_[edge_key];
    double value = input_value * state.scale + state.offset;
    if (state.clamp_min && state.clamp_max) {
      value = std::max(*state.clamp_min, std::min(*state.clamp_max, value));
    }
    return value;
  }

  case anolis_provider_sim::TransformType::Deadband: {
    auto &state = deadband_states_[edge_key];
    if (!state.initialized) {
      state.last_output = input_value;
      state.initialized = true;
      return input_value;
    }
    // Only update output if input has moved more than threshold from last
    // output
    if (std::abs(input_value - state.last_output) > state.threshold) {
      state.last_output = input_value;
    }
    return state.last_output;
  }

  case anolis_provider_sim::TransformType::RateLimiter: {
    auto &state = rate_limiter_states_[edge_key];
    if (!state.initialized) {
      state.last_value = input_value;
      state.initialized = true;
      return input_value;
    }
    // Limit rate of change
    double max_delta = state.max_rate_per_sec * dt;
    double delta = input_value - state.last_value;
    if (delta > max_delta) {
      state.last_value += max_delta;
    } else if (delta < -max_delta) {
      state.last_value -= max_delta;
    } else {
      state.last_value = input_value;
    }
    return state.last_value;
  }

  case anolis_provider_sim::TransformType::Delay: {
    auto &state = delay_states_[edge_key];
    // Add current value to buffer with timestamp
    static double sim_time = 0.0;
    sim_time += dt;
    state.buffer.push_back({sim_time, input_value});

    // Find value from delay_sec ago
    double target_time = sim_time - state.delay_sec;

    // Remove old entries
    while (!state.buffer.empty() &&
           state.buffer.front().first < target_time - dt) {
      state.buffer.erase(state.buffer.begin());
    }

    // Return delayed value (or current if buffer too short)
    if (state.buffer.empty()) {
      return input_value;
    }

    // Find closest value to target_time
    for (const auto &[time, value] : state.buffer) {
      if (time >= target_time) {
        return value;
      }
    }

    return state.buffer.back().second;
  }

  case anolis_provider_sim::TransformType::MovingAverage: {
    auto &state = moving_average_states_[edge_key];

    // Initialize buffer if needed
    if (state.buffer.empty()) {
      state.buffer.resize(state.window_size, input_value);
      state.sum = input_value * state.window_size;
      state.buffer_index = 0;
      return input_value;
    }

    // Update circular buffer
    state.sum -= state.buffer[state.buffer_index];
    state.buffer[state.buffer_index] = input_value;
    state.sum += input_value;

    state.buffer_index = (state.buffer_index + 1) % state.window_size;
    if (state.buffer_index == 0) {
      state.filled = true;
    }

    // Return average
    return state.sum / state.window_size;
  }
  }

  return input_value;
}

std::optional<double>
SimPhysics::read_model_signal(const std::string &model_id,
                              const std::string &signal_id) {
  std::lock_guard<std::mutex> lock(physics_mutex_);

  if (model_outputs_.find(model_id) == model_outputs_.end()) {
    return std::nullopt;
  }

  auto &outputs = model_outputs_[model_id];
  if (outputs.find(signal_id) == outputs.end()) {
    return std::nullopt;
  }

  return outputs[signal_id];
}

std::optional<double> SimPhysics::read_signal_value(
    const std::string &path,
    const std::map<std::string, double> *pending_writes) {
  // Parse path: object_id/signal_id
  size_t slash_pos = path.find('/');
  if (slash_pos == std::string::npos) {
    return std::nullopt;
  }

  std::string object_id = path.substr(0, slash_pos);
  std::string signal_id = path.substr(slash_pos + 1);

  // Try model output first
  if (models_.find(object_id) != models_.end()) {
    if (model_outputs_[object_id].find(signal_id) !=
        model_outputs_[object_id].end()) {
      return model_outputs_[object_id][signal_id];
    }
  }

  // Check pending writes (uncommitted device signals from current tick)
  if (pending_writes) {
    auto it = pending_writes->find(path);
    if (it != pending_writes->end()) {
      return it->second;
    }
  }

  // Not a model, read from signal source (device or registry)
  return signal_source_->read_signal(path);
}

} // namespace sim_physics
