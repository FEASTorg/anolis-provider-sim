#include "devices/motorctl/motorctl_device.hpp"

#include <cmath>
#include <set>

#include "devices/common/device_manager.hpp" // For g_signal_registry

namespace sim_devices {
namespace motorctl {

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
static constexpr uint32_t kFnSetDuty = 10;

// Signal IDs
static constexpr const char *kSigMotor1Speed = "motor1_speed";
static constexpr const char *kSigMotor2Speed = "motor2_speed";
static constexpr const char *kSigMotor1Duty = "motor1_duty";
static constexpr const char *kSigMotor2Duty = "motor2_duty";

// -----------------------------
// State
// -----------------------------

struct State {
  double duty1 = 0.0; // 0..1
  double duty2 = 0.0;

  double speed1 = 0.0; // arbitrary RPM
  double speed2 = 0.0;

  double max_rpm = 3200.0; // maximum RPM (configurable)
};

// Per-device instance state storage
static std::map<std::string, State> g_device_states;

static State &get_state(const std::string &device_id) {
  return g_device_states[device_id];
}

// -----------------------------
// Initialization
// -----------------------------

void init(const std::string &device_id, const Config &config) {
  // Initialize state for this device instance with defaults
  State s;

  // Apply max_speed if provided
  if (config.max_speed.has_value()) {
    double max_speed = config.max_speed.value();

    // Validate reasonable range (0 to 10000 RPM)
    if (max_speed <= 0 || max_speed > 10000.0) {
      throw std::runtime_error("[MotorCtl] max_speed " +
                               std::to_string(max_speed) +
                               " out of valid range (0, 10000] RPM");
    }

    s.max_rpm = max_speed;
  }

  g_device_states[device_id] = s;
}

// -----------------------------
// Physics
// -----------------------------

void update_physics(const std::string &device_id, double dt) {
  State &s = get_state(device_id);

  // Speed approaches duty * max_rpm with lag
  const double motor_tau = 0.8; // seconds
  const double motor_alpha = 1.0 - std::exp(-dt / motor_tau);

  const double tgt1 = clamp(s.duty1, 0.0, 1.0) * s.max_rpm;
  const double tgt2 = clamp(s.duty2, 0.0, 1.0) * s.max_rpm;

  s.speed1 += motor_alpha * (tgt1 - s.speed1);
  s.speed2 += motor_alpha * (tgt2 - s.speed2);
}

// -----------------------------
// Device Info
// -----------------------------

Device get_device_info(const std::string &device_id, bool /*include_health*/) {
  Device d;
  d.set_device_id(device_id);
  d.set_provider_name(kProviderName);
  d.set_type_id("sim.dual_dc_motor");
  d.set_type_version("1.0");
  d.set_label("Sim Dual DC Motor Controller");
  d.set_address("sim://" + device_id);
  (*d.mutable_tags())["family"] = "sim";
  (*d.mutable_tags())["kind"] = "motor_control";
  return d;
}

// -----------------------------
// Capabilities
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

CapabilitySet get_capabilities() {
  CapabilitySet caps;

  // Signals
  {
    SignalSpec s;
    s.set_signal_id(kSigMotor1Speed);
    s.set_name("Motor 1 Speed");
    s.set_description("Estimated speed");
    s.set_value_type(ValueType::VALUE_TYPE_DOUBLE);
    s.set_unit("rpm");
    s.set_poll_hint_hz(5.0);
    s.set_stale_after_ms(800);
    *caps.add_signals() = s;
  }
  {
    SignalSpec s;
    s.set_signal_id(kSigMotor2Speed);
    s.set_name("Motor 2 Speed");
    s.set_description("Estimated speed");
    s.set_value_type(ValueType::VALUE_TYPE_DOUBLE);
    s.set_unit("rpm");
    s.set_poll_hint_hz(5.0);
    s.set_stale_after_ms(800);
    *caps.add_signals() = s;
  }
  {
    SignalSpec s;
    s.set_signal_id(kSigMotor1Duty);
    s.set_name("Motor 1 Duty");
    s.set_description("PWM duty 0..1");
    s.set_value_type(ValueType::VALUE_TYPE_DOUBLE);
    s.set_unit("");
    s.set_poll_hint_hz(2.0);
    s.set_stale_after_ms(1500);
    *caps.add_signals() = s;
  }
  {
    SignalSpec s;
    s.set_signal_id(kSigMotor2Duty);
    s.set_name("Motor 2 Duty");
    s.set_description("PWM duty 0..1");
    s.set_value_type(ValueType::VALUE_TYPE_DOUBLE);
    s.set_unit("");
    s.set_poll_hint_hz(2.0);
    s.set_stale_after_ms(1500);
    *caps.add_signals() = s;
  }

  // Functions
  {
    FunctionSpec f;
    f.set_function_id(kFnSetDuty);
    f.set_name("set_motor_duty");
    f.set_description("Set PWM duty for a motor channel");
    *f.mutable_policy() = make_policy(FunctionPolicy::CATEGORY_ACTUATE);

    auto idx =
        make_arg("motor_index", ValueType::VALUE_TYPE_INT64, true, "1 or 2");
    idx.set_min_int64(1);
    idx.set_max_int64(2);
    *f.add_args() = idx;

    auto d = make_arg("duty", ValueType::VALUE_TYPE_DOUBLE, true, "Duty 0..1");
    d.set_min_double(0.0);
    d.set_max_double(1.0);
    *f.add_args() = d;

    *caps.add_functions() = f;
  }

  return caps;
}

// -----------------------------
// Signal Reading
// -----------------------------

// Use intentional leak pattern for process-lifetime constant.
// This avoids static destruction order issues when std::exit() is called
// by background threads (e.g., crash timer in chaos testing mode).
// The set is never destroyed, which is safe for process-lifetime constants.
static const std::set<std::string> &get_known_signals() {
  static std::set<std::string> *kKnownSignals = new std::set<std::string>{
      kSigMotor1Speed, kSigMotor2Speed, kSigMotor1Duty, kSigMotor2Duty};
  return *kKnownSignals;
}

static std::vector<std::string> default_signals() {
  return {kSigMotor1Speed, kSigMotor2Speed};
}

std::vector<SignalValue>
read_signals(const std::string &device_id,
             const std::vector<std::string> &signal_ids) {
  State &s = get_state(device_id);

  std::vector<std::string> ids = signal_ids;
  if (ids.empty()) {
    ids = default_signals();
  }

  std::vector<SignalValue> out;

  auto maybe_physics_value = [&](const std::string &signal_id)
      -> std::optional<double> {
    if (!g_signal_registry) {
      return std::nullopt;
    }
    const std::string path = device_id + "/" + signal_id;
    if (!g_signal_registry->is_physics_driven(path)) {
      return std::nullopt;
    }
    return g_signal_registry->read_signal(path);
  };

  for (const auto &id : ids) {
    if (get_known_signals().count(id) == 0) {
      // Omit unknown signals
      continue;
    }

    if (id == kSigMotor1Speed) {
      const double value = maybe_physics_value(id).value_or(s.speed1);
      out.push_back(make_signal_value(id, make_double(value)));
    } else if (id == kSigMotor2Speed) {
      const double value = maybe_physics_value(id).value_or(s.speed2);
      out.push_back(make_signal_value(id, make_double(value)));
    } else if (id == kSigMotor1Duty) {
      const double value = maybe_physics_value(id).value_or(s.duty1);
      out.push_back(make_signal_value(id, make_double(value)));
    } else if (id == kSigMotor2Duty) {
      const double value = maybe_physics_value(id).value_or(s.duty2);
      out.push_back(make_signal_value(id, make_double(value)));
    }
  }

  return out;
}

// -----------------------------
// Function Calls
// -----------------------------

CallResult call_function(const std::string &device_id, uint32_t function_id,
                         const std::map<std::string, Value> &args) {
  State &s = get_state(device_id);

  if (function_id == kFnSetDuty) {
    int64_t idx = 0;
    double duty = 0.0;
    if (!get_arg_int64(args, "motor_index", idx)) {
      return bad("missing/invalid arg: motor_index (int64)");
    }
    if (idx != 1 && idx != 2) {
      return bad("motor_index must be 1 or 2");
    }
    if (!get_arg_double(args, "duty", duty)) {
      return bad("missing/invalid arg: duty (double)");
    }
    if (duty < 0.0 || duty > 1.0) {
      return bad("duty out of range (0..1)");
    }

    if (idx == 1)
      s.duty1 = duty;
    else
      s.duty2 = duty;

    return ok();
  }

  return nf("unknown function_id for " + device_id);
}

} // namespace motorctl
} // namespace sim_devices
