#pragma once

/**
 * @file framed_stdio.hpp
 * @brief Length-prefixed stdio transport helpers for provider-sim ADPP frames.
 */

#include <cstdint>
#include <istream>
#include <ostream>
#include <string>
#include <vector>

namespace anolis_provider_sim::transport {

/** @brief ADPP stdio frame size guardrail: 1 MiB maximum payload. */
constexpr uint32_t kMaxFrameBytes = 1024u * 1024u;

/** @brief Read exactly `n` bytes or fail on EOF/stream error. */
bool read_exact(std::istream &in, uint8_t *buf, size_t n);

/**
 * @brief Read one length-prefixed frame from the input stream.
 *
 * Error handling:
 * Returns `false` on clean EOF or protocol/stream failure. `err` stays empty
 * for clean EOF and contains a message for fatal frame errors.
 */
bool read_frame(std::istream &in, std::vector<uint8_t> &out, std::string &err, uint32_t max_len = kMaxFrameBytes);

/** @brief Write one length-prefixed frame and flush the output stream. */
bool write_frame(std::ostream &out, const uint8_t *data, size_t len, std::string &err,
                 uint32_t max_len = kMaxFrameBytes);

}  // namespace anolis_provider_sim::transport
