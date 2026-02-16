#include "analogsensor_device.hpp"

#include <cmath>
#include <random>

namespace sim_devices {
namespace analogsensor {

using anolis::deviceprovider::v1::ArgSpec;
using anolis::deviceprovider::v1::FunctionPolicy;
using anolis::deviceprovider::v1::FunctionSpec;
using anolis::deviceprovider::v1::SignalSpec;
using anolis::deviceprovider::v1::ValueType;

// -----------------------------
// Constants
// -----------------------------

static constexpr const char *kProviderName = "anolis-provider-sim";

// Function IDs
static constexpr uint32_t kFnCalibrateChannel = 1;
static constexpr uint32_t kFnInjectNoise = 2;

// Signal IDs
static constexpr const char *kSigVoltageCh1 = "voltage_ch1";
static constexpr const char *kSigVoltageCh2 = "voltage_ch2";
static constexpr const char *kSigVoltageCh3 = "voltage_ch3";
static constexpr const char *kSigVoltageCh4 = "voltage_ch4";
static constexpr const char *kSigSensorQuality = "sensor_quality";

// -----------------------------
// State
// -----------------------------

struct State {
  // Analog channel values (volts, 0-10V range)
  double voltage_ch1_base = 2.5;
  double voltage_ch2_base = 5.0;
  double voltage_ch3_base = 7.5;
  double voltage_ch4_base = 3.3;

  // Noise and drift state
  double drift_accumulator = 0.0;
  double noise_level = 0.01; // std dev of noise
  bool noise_enabled = false;

  // Quality state: "GOOD", "NOISY", "FAULT"
  std::string quality = "GOOD";
  double quality_timer = 0.0; // for simulating quality degradation

  // Random number generator
  std::mt19937 rng{42}; // Fixed seed for reproducibility
};

// Per-device instance state storage
static std::map<std::string, State> g_device_states;

static State &get_state(const std::string &device_id) {
  return g_device_states[device_id];
}

// -----------------------------
// Initialization
// -----------------------------

void init(const std::string &device_id) {
  // Initialize state for this device instance
  g_device_states[device_id] = State();
}

// -----------------------------
// Physics (noise/drift simulation)
// -----------------------------

void update_physics(const std::string &device_id, double dt) {
  State &s = get_state(device_id);

  // Simulate drift over time (only when noise enabled)
  if (s.noise_enabled) {
    s.drift_accumulator += dt * 0.002; // 2mV per second drift
  }

  // Update quality state machine
  s.quality_timer += dt;

  // Quality degrades over time when noise is enabled
  if (s.noise_enabled) {
    if (s.quality == "GOOD" && s.quality_timer > 30.0) {
      s.quality = "NOISY";
    } else if (s.quality == "NOISY" && s.quality_timer > 60.0) {
      s.quality = "FAULT";
    }
  }

  // Update noise level based on quality
  if (s.quality == "GOOD") {
    s.noise_level = 0.01; // 10mV noise
  } else if (s.quality == "NOISY") {
    s.noise_level = 0.05; // 50mV noise
  } else if (s.quality == "FAULT") {
    s.noise_level = 0.2; // 200mV noise
  }
}

// Helper: get noisy reading, clamped to 0-10V range
static double get_noisy_reading(State &s, double base) {
  std::normal_distribution<double> dist(0.0, s.noise_level);
  double value = base + s.drift_accumulator + dist(s.rng);
  return clamp(value, 0.0, 10.0); // 0-10V range
}

// -----------------------------
// Device info
// -----------------------------

Device get_device_info(const std::string &device_id, bool /*include_health*/) {
  Device d;
  d.set_device_id(device_id);
  d.set_provider_name(kProviderName);
  d.set_type_id("sim.analog_sensor_module");
  d.set_type_version("1.0");
  d.set_label("Sim Analog Sensor Module (4 channels)");
  d.set_address("sim://" + device_id);
  (*d.mutable_tags())["family"] = "sim";
  (*d.mutable_tags())["kind"] = "analog_input";
  return d;
}

// -----------------------------
// Helper functions
// -----------------------------

static ArgSpec make_arg(const std::string &name, ValueType type, bool required,
                        const std::string &desc = "",
                        const std::string &unit = "") {
  ArgSpec a;
  a.set_name(name);
  a.set_type(type);
  a.set_required(required);
  a.set_description(desc);
  a.set_unit(unit);
  return a;
}

static FunctionPolicy make_policy(FunctionPolicy::Category cat) {
  FunctionPolicy p;
  p.set_category(cat);
  p.set_requires_lease(false);
  p.set_is_idempotent(false);
  p.set_min_interval_ms(0);
  return p;
}

// -----------------------------
// Capabilities
// -----------------------------

CapabilitySet get_capabilities() {
  CapabilitySet caps;

  // Signals
  {
    SignalSpec s;
    s.set_signal_id(kSigVoltageCh1);
    s.set_name("Voltage Ch1");
    s.set_description("Analog input channel 1 voltage (0-10V)");
    s.set_value_type(ValueType::VALUE_TYPE_DOUBLE);
    s.set_unit("V");
    s.set_poll_hint_hz(10.0);
    s.set_stale_after_ms(500);
    *caps.add_signals() = s;
  }
  {
    SignalSpec s;
    s.set_signal_id(kSigVoltageCh2);
    s.set_name("Voltage Ch2");
    s.set_description("Analog input channel 2 voltage (0-10V)");
    s.set_value_type(ValueType::VALUE_TYPE_DOUBLE);
    s.set_unit("V");
    s.set_poll_hint_hz(10.0);
    s.set_stale_after_ms(500);
    *caps.add_signals() = s;
  }
  {
    SignalSpec s;
    s.set_signal_id(kSigVoltageCh3);
    s.set_name("Voltage Ch3");
    s.set_description("Analog input channel 3 voltage (0-10V)");
    s.set_value_type(ValueType::VALUE_TYPE_DOUBLE);
    s.set_unit("V");
    s.set_poll_hint_hz(10.0);
    s.set_stale_after_ms(500);
    *caps.add_signals() = s;
  }
  {
    SignalSpec s;
    s.set_signal_id(kSigVoltageCh4);
    s.set_name("Voltage Ch4");
    s.set_description("Analog input channel 4 voltage (0-10V)");
    s.set_value_type(ValueType::VALUE_TYPE_DOUBLE);
    s.set_unit("V");
    s.set_poll_hint_hz(10.0);
    s.set_stale_after_ms(500);
    *caps.add_signals() = s;
  }
  {
    SignalSpec s;
    s.set_signal_id(kSigSensorQuality);
    s.set_name("Sensor Quality");
    s.set_description("Overall sensor quality: GOOD, NOISY, or FAULT");
    s.set_value_type(ValueType::VALUE_TYPE_STRING);
    s.set_unit("");
    s.set_poll_hint_hz(0.5);
    s.set_stale_after_ms(3000);
    *caps.add_signals() = s;
  }

  // Functions
  {
    FunctionSpec f;
    f.set_function_id(kFnCalibrateChannel);
    f.set_name("calibrate_channel");
    f.set_description(
        "Calibrate a specific analog input channel (requires GOOD quality)");
    *f.mutable_policy() = make_policy(FunctionPolicy::CATEGORY_CONFIG);
    auto a = make_arg("channel", ValueType::VALUE_TYPE_INT64, true,
                      "Channel index (1-4)");
    a.set_min_int64(1);
    a.set_max_int64(4);
    *f.add_args() = a;
    *caps.add_functions() = f;
  }
  {
    FunctionSpec f;
    f.set_function_id(kFnInjectNoise);
    f.set_name("inject_noise");
    f.set_description("Enable or disable simulated noise injection");
    *f.mutable_policy() = make_policy(FunctionPolicy::CATEGORY_CONFIG);
    *f.add_args() = make_arg("enabled", ValueType::VALUE_TYPE_BOOL, true,
                             "Enable/disable noise");
    *caps.add_functions() = f;
  }

  return caps;
}

// -----------------------------
// Read signals
// -----------------------------

std::vector<SignalValue>
read_signals(const std::string &device_id,
             const std::vector<std::string> &signal_ids) {
  State &s = get_state(device_id);
  std::vector<SignalValue> out;

  // If signal_ids empty, return all signals
  std::vector<std::string> ids = signal_ids;
  if (ids.empty()) {
    ids = {kSigVoltageCh1, kSigVoltageCh2, kSigVoltageCh3, kSigVoltageCh4,
           kSigSensorQuality};
  }

  for (const auto &id : ids) {
    if (id == kSigVoltageCh1)
      out.push_back(make_signal_value(
          id, make_double(get_noisy_reading(s, s.voltage_ch1_base))));
    else if (id == kSigVoltageCh2)
      out.push_back(make_signal_value(
          id, make_double(get_noisy_reading(s, s.voltage_ch2_base))));
    else if (id == kSigVoltageCh3)
      out.push_back(make_signal_value(
          id, make_double(get_noisy_reading(s, s.voltage_ch3_base))));
    else if (id == kSigVoltageCh4)
      out.push_back(make_signal_value(
          id, make_double(get_noisy_reading(s, s.voltage_ch4_base))));
    else if (id == kSigSensorQuality)
      out.push_back(make_signal_value(id, make_string(s.quality)));
  }

  return out;
}

// -----------------------------
// Call function
// -----------------------------

CallResult call_function(const std::string &device_id, uint32_t function_id,
                         const std::map<std::string, Value> &args) {
  State &s = get_state(device_id);

  if (function_id == kFnCalibrateChannel) {
    // Enforce precondition: quality must be GOOD
    if (s.quality != "GOOD") {
      return precond("calibrate_channel requires sensor_quality == GOOD");
    }

    int64_t channel = 0;
    if (!get_arg_int64(args, "channel", channel))
      return bad("missing or invalid 'channel' int64 argument");

    if (channel < 1 || channel > 4)
      return bad("channel must be 1-4");

    // Calibration resets drift for all channels
    s.drift_accumulator = 0.0;
    s.quality_timer = 0.0;

    return ok();
  }

  if (function_id == kFnInjectNoise) {
    bool enabled = false;
    if (!get_arg_bool(args, "enabled", enabled))
      return bad("missing or invalid 'enabled' bool argument");

    s.noise_enabled = enabled;

    // Reset quality when disabling noise
    if (!enabled) {
      s.quality = "GOOD";
      s.quality_timer = 0.0;
      s.drift_accumulator = 0.0;
    }

    return ok();
  }

  return nf("unknown function_id");
}

} // namespace analogsensor
} // namespace sim_devices
