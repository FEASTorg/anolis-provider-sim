// src/transport/framed_stdio.hpp
#pragma once

#include <cstdint>
#include <istream>
#include <ostream>
#include <string>
#include <vector>

namespace transport
{

    // v0 sim limit: 1 MiB frames
    constexpr uint32_t kMaxFrameBytes = 1024u * 1024u;

    // Reads exactly n bytes into buf. Returns false on EOF or stream failure before n bytes.
    bool read_exact(std::istream &in, uint8_t *buf, size_t n);

    // Reads one length-prefixed frame (uint32_le + payload bytes).
    // Returns:
    //  - true  => frame read successfully into out
    //  - false => EOF (err empty) or fatal protocol/IO error (err non-empty)
    bool read_frame(std::istream &in, std::vector<uint8_t> &out, std::string &err,
                    uint32_t max_len = kMaxFrameBytes);

    // Writes one length-prefixed frame (uint32_le + payload bytes) and flushes.
    // Returns false on error and sets err.
    bool write_frame(std::ostream &out, const uint8_t *data, size_t len, std::string &err,
                     uint32_t max_len = kMaxFrameBytes);

} // namespace transport
