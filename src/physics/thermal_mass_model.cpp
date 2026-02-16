#include "thermal_mass_model.hpp"
#include <stdexcept>

namespace sim_physics {

ThermalMassModel::ThermalMassModel()
    : thermal_mass_(1000.0), heat_transfer_coeff_(10.0), initial_temp_(25.0),
      temperature_(25.0) {
  input_names_ = {"heating_power", "ambient_temp"};
  output_names_ = {"temperature"};
}

void ThermalMassModel::init(const std::map<std::string, YAML::Node> &params) {
  // Load parameters
  if (params.find("thermal_mass") != params.end()) {
    thermal_mass_ = params.at("thermal_mass").as<double>();
    if (thermal_mass_ <= 0.0) {
      throw std::runtime_error("ThermalMassModel: thermal_mass must be > 0.0");
    }
  }

  if (params.find("heat_transfer_coeff") != params.end()) {
    heat_transfer_coeff_ = params.at("heat_transfer_coeff").as<double>();
    if (heat_transfer_coeff_ <= 0.0) {
      throw std::runtime_error(
          "ThermalMassModel: heat_transfer_coeff must be > 0.0");
    }
  }

  if (params.find("initial_temp") != params.end()) {
    initial_temp_ = params.at("initial_temp").as<double>();
  }

  // Initialize state
  temperature_ = initial_temp_;
}

void ThermalMassModel::update(double dt,
                              const std::map<std::string, double> &inputs,
                              std::map<std::string, double> &outputs) {
  // Get inputs (default to safe values if missing)
  double heating_power = 0.0;
  double ambient_temp = 25.0;

  if (inputs.find("heating_power") != inputs.end()) {
    heating_power = inputs.at("heating_power");
  }

  if (inputs.find("ambient_temp") != inputs.end()) {
    ambient_temp = inputs.at("ambient_temp");
  }

  // Physics: dT/dt = (Q_heater - Q_ambient) / C
  // Q_heater = heating_power (W)
  // Q_ambient = h * (T - T_ambient) (W)
  double q_ambient = heat_transfer_coeff_ * (temperature_ - ambient_temp);
  double dT_dt = (heating_power - q_ambient) / thermal_mass_;

  // Integrate (Euler)
  temperature_ += dT_dt * dt;

  // Set outputs
  outputs["temperature"] = temperature_;
}

} // namespace sim_physics
