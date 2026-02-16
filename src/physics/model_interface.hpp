#pragma once

#include <map>
#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>

namespace sim_physics {

// Abstract interface for physics models
class PhysicsModel {
public:
  virtual ~PhysicsModel() = default;

  // Initialize model with config parameters
  // Throws exception if params invalid (fail-fast)
  virtual void init(const std::map<std::string, YAML::Node> &params) = 0;

  // Update model state for time step dt
  // inputs: map of input_signal_name -> current_value
  // outputs: map of output_signal_name -> computed_value (model writes to this)
  virtual void update(double dt, const std::map<std::string, double> &inputs,
                      std::map<std::string, double> &outputs) = 0;

  // Declared I/O contract used for startup graph validation
  virtual const std::vector<std::string> &input_names() const = 0;
  virtual const std::vector<std::string> &output_names() const = 0;
};

} // namespace sim_physics
