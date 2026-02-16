#include "tempctl_device.hpp"

#include <cmath>
#include <set>

#include "device_manager.hpp" // For g_signal_registry

namespace sim_devices {
namespace tempctl {

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
static constexpr uint32_t kFnSetMode = 1;
static constexpr uint32_t kFnSetSetpoint = 2;
static constexpr uint32_t kFnSetRelay = 3;

// Signal IDs
static constexpr const char *kSigTc1Temp = "tc1_temp";
static constexpr const char *kSigTc2Temp = "tc2_temp";
static constexpr const char *kSigRelay1State = "relay1_state";
static constexpr const char *kSigRelay2State = "relay2_state";
static constexpr const char *kSigControlMode = "control_mode";
static constexpr const char *kSigSetpoint = "setpoint";

// -----------------------------
// State
// -----------------------------

struct State {
  // Two thermocouples
  double tc1_c = 25.0;
  double tc2_c = 25.0;

  // Two relays
  bool relay1 = false;
  bool relay2 = false;

  // Modes: "open" | "closed"
  std::string mode = "open";

  // Closed-loop setpoint
  double setpoint_c = 60.0;
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

  // Apply initial_temp if provided
  if (config.initial_temp.has_value()) {
    double temp = config.initial_temp.value();

    // Validate against temp_range if provided
    if (config.temp_range.has_value()) {
      double min_temp = config.temp_range.value().first;
      double max_temp = config.temp_range.value().second;
      if (temp < min_temp || temp > max_temp) {
        throw std::runtime_error(
            "[TempCtl] initial_temp " + std::to_string(temp) +
            " out of valid range [" + std::to_string(min_temp) + ", " +
            std::to_string(max_temp) + "]");
      }
    }

    s.tc1_c = temp;
    s.tc2_c = temp;
  }

  g_device_states[device_id] = s;
}

// -----------------------------
// Physics
// -----------------------------

void update_physics(const std::string &device_id, double dt) {
  State &s = get_state(device_id);

  // Ambient temperature
  const double ambient = 23.0;

  // Heating strength: each relay adds heating power.
  const int relays_on = (s.relay1 ? 1 : 0) + (s.relay2 ? 1 : 0);

  // Target temperature depends on mode
  double target = ambient;

  if (s.mode == "closed") {
    // In closed-loop, pretend relays/power are modulated to move toward
    // setpoint. We still keep relay states meaningful by modeling a simple
    // "effective heating".
    target = s.setpoint_c;
  } else {
    // Open loop: target depends on relays being on. (crude but useful)
    // 0 relays: ambient
    // 1 relay: ambient + 45C
    // 2 relays: ambient + 75C
    target = ambient + (relays_on == 0 ? 0.0 : (relays_on == 1 ? 45.0 : 75.0));
  }

  // First-order response
  const double tau = 6.0; // seconds (time constant)
  const double alpha = 1.0 - std::exp(-dt / tau);

  // Add slight sensor offset to make channels distinct
  s.tc1_c += alpha * (target - s.tc1_c);
  s.tc2_c += alpha * ((target - 1.5) - s.tc2_c);
}

// -----------------------------
// Device Info
// -----------------------------

Device get_device_info(const std::string &device_id, bool /*include_health*/) {
  Device d;
  d.set_device_id(device_id);
  d.set_provider_name(kProviderName);
  d.set_type_id("sim.temp_control_card");
  d.set_type_version("1.0");
  d.set_label("Sim Temp Control Card (2TC + 2Relay)");
  d.set_address("sim://" + device_id);
  (*d.mutable_tags())["family"] = "sim";
  (*d.mutable_tags())["kind"] = "temp_control";
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
    s.set_signal_id(kSigTc1Temp);
    s.set_name("TC1 Temperature");
    s.set_description("Thermocouple channel 1");
    s.set_value_type(ValueType::VALUE_TYPE_DOUBLE);
    s.set_unit("C");
    s.set_poll_hint_hz(2.0);
    s.set_stale_after_ms(1500);
    *caps.add_signals() = s;
  }
  {
    SignalSpec s;
    s.set_signal_id(kSigTc2Temp);
    s.set_name("TC2 Temperature");
    s.set_description("Thermocouple channel 2");
    s.set_value_type(ValueType::VALUE_TYPE_DOUBLE);
    s.set_unit("C");
    s.set_poll_hint_hz(2.0);
    s.set_stale_after_ms(1500);
    *caps.add_signals() = s;
  }
  {
    SignalSpec s;
    s.set_signal_id(kSigRelay1State);
    s.set_name("Relay 1 State");
    s.set_description("Relay output channel 1");
    s.set_value_type(ValueType::VALUE_TYPE_BOOL);
    s.set_unit("");
    s.set_poll_hint_hz(2.0);
    s.set_stale_after_ms(1500);
    *caps.add_signals() = s;
  }
  {
    SignalSpec s;
    s.set_signal_id(kSigRelay2State);
    s.set_name("Relay 2 State");
    s.set_description("Relay output channel 2");
    s.set_value_type(ValueType::VALUE_TYPE_BOOL);
    s.set_unit("");
    s.set_poll_hint_hz(2.0);
    s.set_stale_after_ms(1500);
    *caps.add_signals() = s;
  }
  {
    SignalSpec s;
    s.set_signal_id(kSigControlMode);
    s.set_name("Control Mode");
    s.set_description("open or closed");
    s.set_value_type(ValueType::VALUE_TYPE_STRING);
    s.set_unit("");
    s.set_poll_hint_hz(0.5);
    s.set_stale_after_ms(3000);
    *caps.add_signals() = s;
  }
  {
    SignalSpec s;
    s.set_signal_id(kSigSetpoint);
    s.set_name("Setpoint");
    s.set_description("Closed-loop temperature setpoint");
    s.set_value_type(ValueType::VALUE_TYPE_DOUBLE);
    s.set_unit("C");
    s.set_poll_hint_hz(0.5);
    s.set_stale_after_ms(3000);
    *caps.add_signals() = s;
  }

  // Functions
  {
    FunctionSpec f;
    f.set_function_id(kFnSetMode);
    f.set_name("set_mode");
    f.set_description("Set control mode: open or closed");
    *f.mutable_policy() = make_policy(FunctionPolicy::CATEGORY_CONFIG);
    *f.add_args() =
        make_arg("mode", ValueType::VALUE_TYPE_STRING, true, "open or closed");
    *caps.add_functions() = f;
  }
  {
    FunctionSpec f;
    f.set_function_id(kFnSetSetpoint);
    f.set_name("set_setpoint");
    f.set_description("Set closed-loop setpoint (C)");
    *f.mutable_policy() = make_policy(FunctionPolicy::CATEGORY_CONFIG);
    auto a = make_arg("value", ValueType::VALUE_TYPE_DOUBLE, true,
                      "Temperature setpoint", "C");
    a.set_min_double(-50.0);
    a.set_max_double(400.0);
    *f.add_args() = a;
    *caps.add_functions() = f;
  }
  {
    FunctionSpec f;
    f.set_function_id(kFnSetRelay);
    f.set_name("set_relay");
    f.set_description("Set relay state in open-loop mode");
    *f.mutable_policy() = make_policy(FunctionPolicy::CATEGORY_ACTUATE);

    auto idx =
        make_arg("relay_index", ValueType::VALUE_TYPE_INT64, true, "1 or 2");
    idx.set_min_int64(1);
    idx.set_max_int64(2);
    *f.add_args() = idx;

    *f.add_args() = make_arg("state", ValueType::VALUE_TYPE_BOOL, true,
                             "true=on false=off");
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
      kSigTc1Temp,     kSigTc2Temp,     kSigRelay1State,
      kSigRelay2State, kSigControlMode, kSigSetpoint};
  return *kKnownSignals;
}

static std::vector<std::string> default_signals() {
  return {kSigTc1Temp, kSigTc2Temp, kSigRelay1State, kSigRelay2State};
}

std::vector<SignalValue>
read_signals(const std::string &device_id,
             const std::vector<std::string> &signal_ids) {
  State &s = get_state(device_id);

  // In closed-loop mode, run bang-bang controller to set relay states
  if (s.mode == "closed") {
    // Simple bang-bang with hysteresis: turn on if below setpoint-2, off if
    // above setpoint+2

    // Read TC1 temperature (check physics registry first)
    double temp = s.tc1_c; // Default to internal state
    if (g_signal_registry) {
      std::string tc1_path = device_id + "/tc1_temp";
      if (g_signal_registry->is_physics_driven(tc1_path)) {
        auto phys_val = g_signal_registry->read_signal(tc1_path);
        if (phys_val) {
          temp = *phys_val;
        }
      }
    }

    double error = s.setpoint_c - temp;

    if (error > 10.0) {
      // Far below setpoint: both relays on
      s.relay1 = true;
      s.relay2 = true;
    } else if (error > 2.0) {
      // Moderately below: one relay on
      s.relay1 = true;
      s.relay2 = false;
    } else if (error < -2.0) {
      // Above setpoint: both off
      s.relay1 = false;
      s.relay2 = false;
    }
    // Else: in dead band (-2 to +2), keep current state
  }

  std::vector<std::string> ids = signal_ids;
  if (ids.empty()) {
    ids = default_signals();
  }

  std::vector<SignalValue> out;

  for (const auto &id : ids) {
    if (get_known_signals().count(id) == 0) {
      // Omit unknown signals
      continue;
    }

    if (id == kSigTc1Temp) {
      // Check if physics is driving this signal
      double value = s.tc1_c; // Default to internal state
      if (g_signal_registry) {
        std::string full_path = device_id + "/tc1_temp";
        if (g_signal_registry->is_physics_driven(full_path)) {
          auto phys_val = g_signal_registry->read_signal(full_path);
          if (phys_val) {
            value = *phys_val;
          }
        }
      }
      out.push_back(make_signal_value(id, make_double(value)));
    } else if (id == kSigTc2Temp) {
      // Check if physics is driving this signal
      double value = s.tc2_c; // Default to internal state
      if (g_signal_registry) {
        std::string full_path = device_id + "/tc2_temp";
        if (g_signal_registry->is_physics_driven(full_path)) {
          auto phys_val = g_signal_registry->read_signal(full_path);
          if (phys_val) {
            value = *phys_val;
          }
        }
      }
      out.push_back(make_signal_value(id, make_double(value)));
    } else if (id == kSigRelay1State)
      out.push_back(make_signal_value(id, make_bool(s.relay1)));
    else if (id == kSigRelay2State)
      out.push_back(make_signal_value(id, make_bool(s.relay2)));
    else if (id == kSigControlMode)
      out.push_back(make_signal_value(id, make_string(s.mode)));
    else if (id == kSigSetpoint)
      out.push_back(make_signal_value(id, make_double(s.setpoint_c)));
  }

  return out;
}

// -----------------------------
// Function Calls
// -----------------------------

CallResult call_function(const std::string &device_id, uint32_t function_id,
                         const std::map<std::string, Value> &args) {
  State &s = get_state(device_id);

  if (function_id == kFnSetMode) {
    std::string mode;
    if (!get_arg_string(args, "mode", mode)) {
      return bad("missing/invalid arg: mode (string)");
    }
    if (mode != "open" && mode != "closed") {
      return bad("mode must be 'open' or 'closed'");
    }
    s.mode = mode;
    return ok();
  }

  if (function_id == kFnSetSetpoint) {
    double sp = 0.0;
    if (!get_arg_double(args, "value", sp)) {
      return bad("missing/invalid arg: value (double)");
    }
    if (sp < -50.0 || sp > 400.0) {
      return bad("setpoint out of range");
    }
    s.setpoint_c = sp;
    return ok();
  }

  if (function_id == kFnSetRelay) {
    // Enforce "open loop" rule as a realistic precondition
    if (s.mode != "open") {
      return precond("set_relay only allowed in open mode");
    }

    int64_t idx = 0;
    bool st = false;
    if (!get_arg_int64(args, "relay_index", idx)) {
      return bad("missing/invalid arg: relay_index (int64)");
    }
    if (idx != 1 && idx != 2) {
      return bad("relay_index must be 1 or 2");
    }
    if (!get_arg_bool(args, "state", st)) {
      return bad("missing/invalid arg: state (bool)");
    }

    if (idx == 1)
      s.relay1 = st;
    else
      s.relay2 = st;

    return ok();
  }

  return nf("unknown function_id for " + device_id);
}

} // namespace tempctl
} // namespace sim_devices
