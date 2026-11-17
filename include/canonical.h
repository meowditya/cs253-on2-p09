// include/canonical.h — canonical (machine-independent) encoding of events for hashes and text.
// Owner: Architect.
//
// Hashes never read raw struct memory (padding, endianness). Every value is written byte by byte,
// little-endian, with a fixed width: time 8, seq 8, type 4, data 4 = 24 bytes per event.
#pragma once
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <string>

#include "event_queue.h"

namespace eq {

// FNV-1a, 64-bit (offset basis 14695981039346656037, prime 1099511628211).
struct Fnv64 {
    std::uint64_t h = 14695981039346656037ULL;
    void byte(std::uint8_t b) { h = (h ^ b) * 1099511628211ULL; }
    void u64(std::uint64_t v) { for (int i = 0; i < 8; ++i) byte(static_cast<std::uint8_t>(v >> (8 * i))); }
    void u32(std::uint32_t v) { for (int i = 0; i < 4; ++i) byte(static_cast<std::uint8_t>(v >> (8 * i))); }
    void i64(std::int64_t v) { u64(static_cast<std::uint64_t>(v)); }   // two's-complement bits
    void event(const Event& e) { i64(e.key.time); u64(e.key.seq); u32(e.type); u32(e.data); }
};

// "0x" + 16 lower-case hex digits.
inline std::string hex16(std::uint64_t v) {
    char buf[24];
    std::snprintf(buf, sizeof buf, "0x%016" PRIx64, v);
    return buf;
}

// "<time> <seq> <type> <data>" in plain decimal.
inline std::string event_text(const Event& e) {
    char buf[96];
    std::snprintf(buf, sizeof buf, "%" PRId64 " %" PRIu64 " %" PRIu32 " %" PRIu32,
                  e.key.time, e.key.seq, e.type, e.data);
    return buf;
}

}  // namespace eq
