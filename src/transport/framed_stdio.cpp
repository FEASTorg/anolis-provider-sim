// src/transport/framed_stdio.cpp
#include "framed_stdio.hpp"

#include <array>
#include <cstring>

namespace transport
{

    static inline uint32_t decode_u32_le(const uint8_t b[4])
    {
        return (static_cast<uint32_t>(b[0])) |
               (static_cast<uint32_t>(b[1]) << 8) |
               (static_cast<uint32_t>(b[2]) << 16) |
               (static_cast<uint32_t>(b[3]) << 24);
    }

    static inline void encode_u32_le(uint32_t v, uint8_t b[4])
    {
        b[0] = static_cast<uint8_t>(v & 0xFF);
        b[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
        b[2] = static_cast<uint8_t>((v >> 16) & 0xFF);
        b[3] = static_cast<uint8_t>((v >> 24) & 0xFF);
    }

    bool read_exact(std::istream &in, uint8_t *buf, size_t n)
    {
        size_t got = 0;
        while (got < n)
        {
            in.read(reinterpret_cast<char *>(buf + got), static_cast<std::streamsize>(n - got));
            const std::streamsize r = in.gcount();

            if (r > 0)
            {
                got += static_cast<size_t>(r);
                continue;
            }

            // No bytes read: either EOF or error
            return false;
        }
        return true;
    }

    bool read_frame(std::istream &in, std::vector<uint8_t> &out, std::string &err, uint32_t max_len)
    {
        err.clear();

        uint8_t hdr[4] = {0, 0, 0, 0};

        // Attempt to read header; distinguish clean EOF vs truncated header.
        // We can't rely on a single read_exact call to detect "clean EOF" (0 bytes).
        // So do one byte peek-read pattern:
        in.read(reinterpret_cast<char *>(hdr), 1);
        if (in.gcount() == 0)
        {
            // Clean EOF
            return false;
        }
        // We read 1 byte; now read remaining 3 bytes
        if (!read_exact(in, hdr + 1, 3))
        {
            err = "unexpected EOF while reading frame header";
            return false;
        }

        const uint32_t len = decode_u32_le(hdr);
        if (len == 0)
        {
            err = "invalid frame length: 0";
            return false;
        }
        if (len > max_len)
        {
            err = "frame length exceeds max";
            return false;
        }

        out.assign(len, 0);
        if (!read_exact(in, out.data(), len))
        {
            err = "unexpected EOF while reading frame payload";
            return false;
        }

        return true;
    }

    bool write_frame(std::ostream &out, const uint8_t *data, size_t len, std::string &err, uint32_t max_len)
    {
        err.clear();

        if (len == 0)
        {
            err = "invalid frame length: 0";
            return false;
        }
        if (len > max_len)
        {
            err = "frame length exceeds max";
            return false;
        }
        if (len > 0xFFFFFFFFu)
        {
            err = "frame length exceeds uint32";
            return false;
        }

        uint8_t hdr[4];
        encode_u32_le(static_cast<uint32_t>(len), hdr);

        out.write(reinterpret_cast<const char *>(hdr), 4);
        if (!out.good())
        {
            err = "failed writing frame header";
            return false;
        }

        out.write(reinterpret_cast<const char *>(data), static_cast<std::streamsize>(len));
        if (!out.good())
        {
            err = "failed writing frame payload";
            return false;
        }

        out.flush();
        if (!out.good())
        {
            err = "failed flushing output";
            return false;
        }

        return true;
    }

} // namespace transport
