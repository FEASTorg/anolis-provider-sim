#include "sim_control_device.hpp"
#include "../fault_injection.hpp"

#include <thread>

namespace sim_devices {
namespace sim_control {

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
static constexpr uint32_t kFnInjectDeviceUnavailable = 1;
static constexpr uint32_t kFnInjectSignalFault = 2;
static constexpr uint32_t kFnInjectCallLatency = 3;
static constexpr uint32_t kFnInjectCallFailure = 4;
static constexpr uint32_t kFnClearFaults = 5;

// -----------------------------
// Initialization
// -----------------------------

void init() { fault_injection::init(); }

// -----------------------------
// Device info
// -----------------------------

Device get_device_info(bool /*include_health*/) {
  Device d;
  d.set_device_id(kDeviceId);
  d.set_provider_name(kProviderName);
  d.set_type_id("sim.control");
  d.set_type_version("1.0");
  d.set_label("Sim Fault Injection Control");
  d.set_address("sim://control");
  (*d.mutable_tags())["family"] = "sim";
  (*d.mutable_tags())["kind"] = "control";
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

  // No signals for control device

  // Functions
  {
    FunctionSpec f;
    f.set_function_id(kFnInjectDeviceUnavailable);
    f.set_name("inject_device_unavailable");
    f.set_description(
        "Make a device appear unavailable for specified duration");
    *f.add_args() = make_arg("device_id", ValueType::VALUE_TYPE_STRING, true,
                             "Target device ID");
    *f.add_args() = make_arg("duration_ms", ValueType::VALUE_TYPE_INT64, true,
                             "Duration in milliseconds", "ms");
    *f.mutable_policy() = make_policy(FunctionPolicy::CATEGORY_ACTUATE);
    *caps.add_functions() = f;
  }
  {
    FunctionSpec f;
    f.set_function_id(kFnInjectSignalFault);
    f.set_name("inject_signal_fault");
    f.set_description(
        "Make a signal report FAULT quality for specified duration");
    *f.add_args() = make_arg("device_id", ValueType::VALUE_TYPE_STRING, true,
                             "Target device ID");
    *f.add_args() = make_arg("signal_id", ValueType::VALUE_TYPE_STRING, true,
                             "Target signal ID");
    *f.add_args() = make_arg("duration_ms", ValueType::VALUE_TYPE_INT64, true,
                             "Duration in milliseconds", "ms");
    *f.mutable_policy() = make_policy(FunctionPolicy::CATEGORY_ACTUATE);
    *caps.add_functions() = f;
  }
  {
    FunctionSpec f;
    f.set_function_id(kFnInjectCallLatency);
    f.set_name("inject_call_latency");
    f.set_description(
        "Add artificial latency to all function calls on a device");
    *f.add_args() = make_arg("device_id", ValueType::VALUE_TYPE_STRING, true,
                             "Target device ID");
    *f.add_args() = make_arg("latency_ms", ValueType::VALUE_TYPE_INT64, true,
                             "Latency in milliseconds", "ms");
    *f.mutable_policy() = make_policy(FunctionPolicy::CATEGORY_ACTUATE);
    *caps.add_functions() = f;
  }
  {
    FunctionSpec f;
    f.set_function_id(kFnInjectCallFailure);
    f.set_name("inject_call_failure");
    f.set_description("Make a function fail probabilistically");
    *f.add_args() = make_arg("device_id", ValueType::VALUE_TYPE_STRING, true,
                             "Target device ID");
    *f.add_args() = make_arg("function_id", ValueType::VALUE_TYPE_STRING, true,
                             "Target function name");
    *f.add_args() = make_arg("failure_rate", ValueType::VALUE_TYPE_DOUBLE, true,
                             "Failure probability (0.0-1.0)");
    *f.mutable_policy() = make_policy(FunctionPolicy::CATEGORY_ACTUATE);
    *caps.add_functions() = f;
  }
  {
    FunctionSpec f;
    f.set_function_id(kFnClearFaults);
    f.set_name("clear_faults");
    f.set_description("Clear all injected faults");
    *f.mutable_policy() = make_policy(FunctionPolicy::CATEGORY_ACTUATE);
    *caps.add_functions() = f;
  }

  return caps;
}

// -----------------------------
// Read signals (none)
// -----------------------------

std::vector<SignalValue>
read_signals(const std::vector<std::string> & /*signal_ids*/) {
  // No signals for control device
  return {};
}

// -----------------------------
// Call function
// -----------------------------

CallResult call_function(uint32_t function_id,
                         const std::map<std::string, Value> &args) {
  // Fault injection controls

  if (function_id == kFnInjectDeviceUnavailable) {
    // Extract args
    std::string device_id;
    int64_t duration_ms;

    if (!get_arg_string(args, "device_id", device_id)) {
      return bad("missing or invalid device_id");
    }
    if (!get_arg_int64(args, "duration_ms", duration_ms)) {
      return bad("missing or invalid duration_ms");
    }

    fault_injection::inject_device_unavailable(device_id, duration_ms);
    return ok();
  }

  if (function_id == kFnInjectSignalFault) {
    // Extract args
    std::string device_id;
    std::string signal_id;
    int64_t duration_ms;

    if (!get_arg_string(args, "device_id", device_id)) {
      return bad("missing or invalid device_id");
    }
    if (!get_arg_string(args, "signal_id", signal_id)) {
      return bad("missing or invalid signal_id");
    }
    if (!get_arg_int64(args, "duration_ms", duration_ms)) {
      return bad("missing or invalid duration_ms");
    }

    fault_injection::inject_signal_fault(device_id, signal_id, duration_ms);
    return ok();
  }

  if (function_id == kFnInjectCallLatency) {
    // Extract args
    std::string device_id;
    int64_t latency_ms;

    if (!get_arg_string(args, "device_id", device_id)) {
      return bad("missing or invalid device_id");
    }
    if (!get_arg_int64(args, "latency_ms", latency_ms)) {
      return bad("missing or invalid latency_ms");
    }

    fault_injection::inject_call_latency(device_id, latency_ms);
    return ok();
  }

  if (function_id == kFnInjectCallFailure) {
    // Extract args
    std::string device_id;
    std::string function_id_str;
    double failure_rate;

    if (!get_arg_string(args, "device_id", device_id)) {
      return bad("missing or invalid device_id");
    }
    if (!get_arg_string(args, "function_id", function_id_str)) {
      return bad("missing or invalid function_id");
    }
    if (!get_arg_double(args, "failure_rate", failure_rate)) {
      return bad("missing or invalid failure_rate");
    }

    fault_injection::inject_call_failure(device_id, function_id_str,
                                         failure_rate);
    return ok();
  }

  if (function_id == kFnClearFaults) {
    fault_injection::clear_all_faults();
    return ok();
  }

  return nf("unknown function_id");
}

} // namespace sim_control
} // namespace sim_devices
