#include "config_translator.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

#include <yaml-cpp/yaml.h>

namespace sim_config {

std::string translate_to_fluxgraph_format(const std::string &provider_physics_yaml_path) {
  YAML::Node provider_root = YAML::LoadFile(provider_physics_yaml_path);
  if (!provider_root["physics"]) {
    throw std::runtime_error("Invalid physics config: missing 'physics' key");
  }

  YAML::Node physics = provider_root["physics"];
  YAML::Node out_root;

  // physics.models -> models
  if (physics["models"] && physics["models"].IsSequence()) {
    YAML::Node out_models(YAML::NodeType::Sequence);
    for (const auto &model : physics["models"]) {
      YAML::Node out_model;
      out_model["id"] = model["id"];
      out_model["type"] = model["type"];

      YAML::Node params =
          model["params"] ? model["params"] : YAML::Node(YAML::NodeType::Map);
      const std::string model_type = model["type"].as<std::string>();
      const std::string model_id = model["id"].as<std::string>();

      // FluxGraph thermal model expects explicit signal paths.
      if (model_type == "thermal_mass") {
        params["temp_signal"] = model_id + "/temperature";
        params["power_signal"] = model_id + "/heating_power";
        params["ambient_signal"] = "environment/ambient_temp";
      }

      out_model["params"] = params;
      out_models.push_back(out_model);
    }
    out_root["models"] = out_models;
  }

  // physics.signal_graph -> edges
  if (physics["signal_graph"] && physics["signal_graph"].IsSequence()) {
    YAML::Node out_edges(YAML::NodeType::Sequence);
    for (const auto &edge : physics["signal_graph"]) {
      YAML::Node out_edge;
      out_edge["source"] = edge["source"];
      out_edge["target"] = edge["target"];

      if (edge["transform"] && edge["transform"].IsMap()) {
        YAML::Node in_tf = edge["transform"];
        YAML::Node out_tf;
        out_tf["type"] = in_tf["type"];

        YAML::Node params(YAML::NodeType::Map);
        for (auto it = in_tf.begin(); it != in_tf.end(); ++it) {
          const std::string key = it->first.as<std::string>();
          if (key == "type") {
            continue;
          }
          params[key] = it->second;
        }

        if (params.size() > 0) {
          out_tf["params"] = params;
        }
        out_edge["transform"] = out_tf;
      }

      out_edges.push_back(out_edge);
    }
    out_root["edges"] = out_edges;
  }

  // Phase 25: rules are intentionally de-scoped.
  if (physics["rules"]) {
    std::cerr << "[ConfigTranslator] rules section ignored in Phase 25\n";
  }

  YAML::Emitter emitter;
  emitter << out_root;
  return emitter.c_str();
}

} // namespace sim_config
