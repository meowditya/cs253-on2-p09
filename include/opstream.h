// include/opstream.h — reading and writing the .ops operation-stream format (INTERFACE.md §7).
// Owner: Architect. Header-only.
//
//   I <time> <type> <data>   insert; the k-th I line (0-based) creates handle H[k]
//   C <k>                    cancel(H[k]); if H[k] does not exist yet, the result is false
//   F <time> <seq>           cancel(Handle{time, seq}) with these literal values
//   P | K | S <t>            pop_min | peek_min | successor(t)
//
// Parsing is strict: unknown op, wrong argument count or an out-of-range number is an error that
// names the file and line. '#' starts a comment; blank lines are ignored.
#pragma once
#include <charconv>
#include <cstdint>
#include <istream>
#include <ostream>
#include <string>
#include <system_error>
#include <type_traits>
#include <vector>

#include "event_queue.h"

namespace eq {

struct Op {
    char          kind = 'P';   // 'I', 'C', 'F', 'P', 'K', 'S'
    Time          time = 0;     // I: time; F: handle time; S: t
    std::uint32_t type = 0;     // I
    std::uint32_t data = 0;     // I
    std::uint64_t k    = 0;     // C: insert index
    Seq           seq  = 0;     // F: handle seq
};

namespace detail {
template <class T>
bool parse_number(const std::string& tok, T& out) {
    if (tok.empty()) return false;
    if (!std::is_signed<T>::value && tok[0] == '-') return false;
    const char* b = tok.data();
    const char* e = tok.data() + tok.size();
    const auto r = std::from_chars(b, e, out);
    return r.ec == std::errc() && r.ptr == e;
}
}  // namespace detail

// Parses one line. Returns true and sets has_op=false for blank/comment lines.
inline bool parse_op_line(const std::string& raw, Op& op, bool& has_op, std::string& why) {
    std::string line = raw;
    const auto hash = line.find('#');
    if (hash != std::string::npos) line.erase(hash);
    std::vector<std::string> tok;
    std::string cur;
    for (char c : line) {
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            if (!cur.empty()) { tok.push_back(cur); cur.clear(); }
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) tok.push_back(cur);
    has_op = false;
    if (tok.empty()) return true;
    if (tok[0].size() != 1) { why = "unknown operation '" + tok[0] + "'"; return false; }
    op = Op{};
    op.kind = tok[0][0];
    auto need = [&](std::size_t n) {
        if (tok.size() != n + 1) { why = std::string("'") + op.kind + "' needs " + std::to_string(n) + " argument(s)"; return false; }
        return true;
    };
    using detail::parse_number;
    switch (op.kind) {
        case 'I':
            if (!need(3)) return false;
            if (!parse_number(tok[1], op.time) || !parse_number(tok[2], op.type) || !parse_number(tok[3], op.data)) {
                why = "bad number in I"; return false;
            }
            break;
        case 'C':
            if (!need(1)) return false;
            if (!parse_number(tok[1], op.k)) { why = "bad number in C"; return false; }
            break;
        case 'F':
            if (!need(2)) return false;
            if (!parse_number(tok[1], op.time) || !parse_number(tok[2], op.seq)) { why = "bad number in F"; return false; }
            break;
        case 'P':
        case 'K':
            if (!need(0)) return false;
            break;
        case 'S':
            if (!need(1)) return false;
            if (!parse_number(tok[1], op.time)) { why = "bad number in S"; return false; }
            break;
        default:
            why = std::string("unknown operation '") + op.kind + "'";
            return false;
    }
    has_op = true;
    return true;
}

// Reads a whole stream. On error returns false with "name:line: reason" in err.
inline bool read_ops(std::istream& in, const std::string& name, std::vector<Op>& out, std::string& err) {
    std::string line;
    std::size_t lineno = 0;
    while (std::getline(in, line)) {
        ++lineno;
        Op op;
        bool has = false;
        std::string why;
        if (!parse_op_line(line, op, has, why)) {
            err = name + ":" + std::to_string(lineno) + ": " + why;
            return false;
        }
        if (has) out.push_back(op);
    }
    return true;
}

inline std::string op_text(const Op& op) {
    switch (op.kind) {
        case 'I': return "I " + std::to_string(op.time) + " " + std::to_string(op.type) + " " + std::to_string(op.data);
        case 'C': return "C " + std::to_string(op.k);
        case 'F': return "F " + std::to_string(op.time) + " " + std::to_string(op.seq);
        case 'S': return "S " + std::to_string(op.time);
        default:  return std::string(1, op.kind);
    }
}

inline void write_ops(std::ostream& os, const std::vector<Op>& ops) {
    for (const Op& op : ops) os << op_text(op) << '\n';
}

}  // namespace eq
