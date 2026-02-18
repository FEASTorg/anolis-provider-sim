#pragma once

#include <string>

namespace sim_config {

// Translate provider-sim Phase 22 physics YAML format into FluxGraph YAML format.
std::string translate_to_fluxgraph_format(const std::string &provider_physics_yaml_path);

} // namespace sim_config
