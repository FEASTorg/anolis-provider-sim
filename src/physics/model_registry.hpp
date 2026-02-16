#pragma once

#include <memory>
#include <string>

namespace sim_physics {

class PhysicsModel;

// Model registry - hardcoded factory for Phase 22
std::unique_ptr<PhysicsModel> create_model(const std::string &type);

} // namespace sim_physics
