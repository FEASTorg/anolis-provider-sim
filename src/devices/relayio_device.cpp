#include "relayio_device.hpp"

namespace sim_devices {
namespace relayio {

using anolis::deviceprovider::v0::ArgSpec;
using anolis::deviceprovider::v0::FunctionPolicy;
using anolis::deviceprovider::v0::FunctionSpec;
using anolis::deviceprovider::v0::SignalSpec;
using anolis::deviceprovider::v0::ValueType;

// -----------------------------
// Constants
// -----------------------------

static constexpr const char *kProviderName = "anolis-provider-sim";

// Function IDs
static constexpr uint32_t kFnSetRelayCh1 = 1;
static constexpr uint32_t kFnSetRelayCh2 = 2;

// Signal IDs
static constexpr const char *kSigRelayCh1State = "relay_ch1_state";
static constexpr const char *kSigRelayCh2State = "relay_ch2_state";
static constexpr const char *kSigGpioInput1 = "gpio_input_1";
static constexpr const char *kSigGpioInput2 = "gpio_input_2";

// -----------------------------
// State
// -----------------------------

struct State {
  // Relay channel states
  bool relay_ch1 = false;
  bool relay_ch2 = false;

  // GPIO input states (simulated as toggle on relay state for demo purposes)
  bool gpio_input_1 = false;
  bool gpio_input_2 = false;
};

static State &state() {
  static State s;
  return s;
}

// -----------------------------
// Initialization
// -----------------------------

void init() {
  // Already initialized via static
}

// -----------------------------
// Physics (simple simulation)
// -----------------------------

void update_physics(double dt) {
  (void)dt;
  State &s = state();

  // Simulate GPIO inputs mirroring relay states (simple demo behavior)
  // In a real system, these would be independent
  s.gpio_input_1 = s.relay_ch1;
  s.gpio_input_2 = !s.relay_ch2; // Inverted for variety
}

// -----------------------------
// Device info
// -----------------------------

Device get_device_info(bool /*include_health*/) {
  Device d;
  d.set_device_id(kDeviceId);
  d.set_provider_name(kProviderName);
  d.set_type_id("sim.relay_io_module");
  d.set_type_version("1.0");
  d.set_label("Sim Relay/IO Module (2 Relay + 2 GPIO)");
  d.set_address("sim://relayio0");
  (*d.mutable_tags())["family"] = "sim";
  (*d.mutable_tags())["kind"] = "relay_io";
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
    s.set_signal_id(kSigRelayCh1State);
    s.set_name("Relay Ch1 State");
    s.set_description("Relay channel 1 state");
    s.set_value_type(ValueType::VALUE_TYPE_BOOL);
    s.set_unit("");
    s.set_poll_hint_hz(1.0);
    s.set_stale_after_ms(2000);
    *caps.add_signals() = s;
  }
  {
    SignalSpec s;
    s.set_signal_id(kSigRelayCh2State);
    s.set_name("Relay Ch2 State");
    s.set_description("Relay channel 2 state");
    s.set_value_type(ValueType::VALUE_TYPE_BOOL);
    s.set_unit("");
    s.set_poll_hint_hz(1.0);
    s.set_stale_after_ms(2000);
    *caps.add_signals() = s;
  }
  {
    SignalSpec s;
    s.set_signal_id(kSigGpioInput1);
    s.set_name("GPIO Input 1");
    s.set_description("GPIO input channel 1");
    s.set_value_type(ValueType::VALUE_TYPE_BOOL);
    s.set_unit("");
    s.set_poll_hint_hz(1.0);
    s.set_stale_after_ms(2000);
    *caps.add_signals() = s;
  }
  {
    SignalSpec s;
    s.set_signal_id(kSigGpioInput2);
    s.set_name("GPIO Input 2");
    s.set_description("GPIO input channel 2");
    s.set_value_type(ValueType::VALUE_TYPE_BOOL);
    s.set_unit("");
    s.set_poll_hint_hz(1.0);
    s.set_stale_after_ms(2000);
    *caps.add_signals() = s;
  }

  // Functions
  {
    FunctionSpec f;
    f.set_function_id(kFnSetRelayCh1);
    f.set_name("set_relay_ch1");
    f.set_description("Set relay channel 1 state");
    *f.mutable_policy() = make_policy(FunctionPolicy::CATEGORY_ACTUATE);
    *f.add_args() = make_arg("enabled", ValueType::VALUE_TYPE_BOOL, true,
                             "Enable/disable relay channel 1");
    *caps.add_functions() = f;
  }
  {
    FunctionSpec f;
    f.set_function_id(kFnSetRelayCh2);
    f.set_name("set_relay_ch2");
    f.set_description("Set relay channel 2 state");
    *f.mutable_policy() = make_policy(FunctionPolicy::CATEGORY_ACTUATE);
    *f.add_args() = make_arg("enabled", ValueType::VALUE_TYPE_BOOL, true,
                             "Enable/disable relay channel 2");
    *caps.add_functions() = f;
  }

  return caps;
}

// -----------------------------
// Read signals
// -----------------------------

std::vector<SignalValue>
read_signals(const std::vector<std::string> &signal_ids) {
  State &s = state();
  std::vector<SignalValue> out;

  // If signal_ids empty, return all signals
  std::vector<std::string> ids = signal_ids;
  if (ids.empty()) {
    ids = {kSigRelayCh1State, kSigRelayCh2State, kSigGpioInput1,
           kSigGpioInput2};
  }

  for (const auto &id : ids) {
    if (id == kSigRelayCh1State)
      out.push_back(make_signal_value(id, make_bool(s.relay_ch1)));
    else if (id == kSigRelayCh2State)
      out.push_back(make_signal_value(id, make_bool(s.relay_ch2)));
    else if (id == kSigGpioInput1)
      out.push_back(make_signal_value(id, make_bool(s.gpio_input_1)));
    else if (id == kSigGpioInput2)
      out.push_back(make_signal_value(id, make_bool(s.gpio_input_2)));
  }

  return out;
}

// -----------------------------
// Call function
// -----------------------------

CallResult call_function(uint32_t function_id,
                         const std::map<std::string, Value> &args) {
  State &s = state();

  switch (function_id) {
  case kFnSetRelayCh1: {
    bool enabled;
    if (!get_arg_bool(args, "enabled", enabled))
      return bad("missing or invalid 'enabled' bool argument");
    s.relay_ch1 = enabled;
    return ok();
  }

  case kFnSetRelayCh2: {
    bool enabled;
    if (!get_arg_bool(args, "enabled", enabled))
      return bad("missing or invalid 'enabled' bool argument");
    s.relay_ch2 = enabled;
    return ok();
  }

  default:
    return nf("unknown function_id");
  }
}

} // namespace relayio
} // namespace sim_devices
