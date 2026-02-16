#pragma once

#include "model_interface.hpp"
#include <map>
#include <string>
#include <vector>

namespace sim_physics {

// Simple lumped thermal mass model
// Models heating/cooling with ambient exchange and internal heating
//
// Inputs:
//   - heating_power (W): Power supplied by heater
//   - ambient_temp (C): Ambient temperature
//
// Outputs:
//   - temperature (C): Current temperature of thermal mass
//
// Parameters:
//   - thermal_mass (J/K): Heat capacity of the system
//   - heat_transfer_coeff (W/K): Convective heat transfer coefficient
//   - initial_temp (C): Starting temperature
//
// Physics:
//   dT/dt = (Q_heater - Q_ambient) / C
//   Q_heater = heating_power
//   Q_ambient = h * (T - T_ambient)
//   C = thermal_mass
//   h = heat_transfer_coeff
class ThermalMassModel : public PhysicsModel {
public:
  ThermalMassModel();
  ~ThermalMassModel() override = default;

  void init(const std::map<std::string, YAML::Node> &params) override;
  void update(double dt, const std::map<std::string, double> &inputs,
              std::map<std::string, double> &outputs) override;

  const std::vector<std::string> &input_names() const override {
    return input_names_;
  }
  const std::vector<std::string> &output_names() const override {
    return output_names_;
  }

private:
  // Model parameters
  double thermal_mass_;        // J/K
  double heat_transfer_coeff_; // W/K
  double initial_temp_;        // C

  // State
  double temperature_; // C

  // Interface
  std::vector<std::string> input_names_;
  std::vector<std::string> output_names_;
};

} // namespace sim_physics
