#pragma once

/**
 * @file config_schema.hpp
 * @brief The provider's declare-once config schema (SDK config toolkit).
 *
 * The single source of truth for the config contract: `--config-schema` emits
 * it (executable profile v1 §2) and `load_config` validates against it, so the
 * advertised schema and the enforced validation cannot drift. One rule is NOT
 * declarable and stays in load_config: `devices[].physics_bindings` is only
 * valid when `simulation.mode` is `sim` (conditionals see sibling keys only —
 * see the SDK's docs/config-schema.md boundary note).
 */

#include "anolis/provider_sdk/config.hpp"

namespace anolis_provider_sim {

/** @brief The provider's config schema (built once, cached). */
const anolis::provider_sdk::config::Schema &provider_schema();

}  // namespace anolis_provider_sim
