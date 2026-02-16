#include "model_registry.hpp"
#include "../config.hpp"
#include "model_interface.hpp"
#include "thermal_mass_model.hpp"
#include <memory>
#include <stdexcept>

namespace sim_physics {

// Model registry - hardcoded factory for Phase 22
std::unique_ptr<PhysicsModel> create_model(const std::string &type) {
  if (type == "thermal_mass") {
    return std::make_unique<ThermalMassModel>();
  }

  throw std::runtime_error("Unknown model type: " + type);
}

} // namespace sim_physics
