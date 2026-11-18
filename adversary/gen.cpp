// adversary/gen.cpp — bin/gen: writes adversarial and edge-case operation streams (.ops) to stdout.
// Owner: Adversary. Derivation: D5.
//
//   gen ascending N            insert times 1..N            -> AVL rotates on all but floor(log2 N)+1 inserts
//   gen descending N           insert times N..1            -> the mirror attack
//   gen zigzag N               times 1, N, 2, N-1, ...      -> exploratory: report its single/double mix
//   gen fib-delete H D         keys of a height-H Fibonacci AVL tree (shorter side left) in level
//                              order (rebuilds it with 0 rotations), then D delete-mins -> cascades
//   gen bloat L STEPS          L live near-future events in a hold loop; every step also inserts a
//                              timer beyond the whole run and cancels it -> lazy heap without purge
//                              grows with every step; with purge it stays <= 2L
//   gen drain N                N inserts, cancel the latest N/2-1, pop the earliest N/2
//                              -> why the purge check must also follow pop_min
//   gen storm N                N events at ONE time, cancel every other, re-cancel stale handles,
//                              forged reserved seqs, then pop all (tie-heavy cancel storm)
//   gen regrow N               grow to N, drain, grow again (capacity / free-list / tombstone stress)
//   gen latesucc N Q           N events, then Q successor queries with late thresholds
//   gen random SEED OPS LIVE   the differential-test mix, as a file
//
// Reference bookkeeping (std::set) is data generation only, which the handout allows.
#include <cstdint>
#include <cstdio>
#include <deque>
#include <iostream>
#include <exception>
#include <limits>
#include <new>
#include <set>
#include <string>
#include <vector>

#include "difftest_core.h"
#include "opstream.h"
#include "rng.h"

using eq::Op;
using eq::Time;

namespace {

std::vector<Op> out;
std::uint64_t inserts = 0;

std::uint64_t I(Time t, std::uint32_t type = 0, std::uint32_t data = 0) {
    Op op; op.kind = 'I'; op.time = t; op.type = type; op.data = data; out.push_back(op);
    return inserts++;
}
void C(std::uint64_t k) { Op op; op.kind = 'C'; op.k = k; out.push_back(op); }
void F(Time t, eq::Seq s) { Op op; op.kind = 'F'; op.time = t; op.seq = s; out.push_back(op); }
void P() { Op op; op.kind = 'P'; out.push_back(op); }
void S(Time t) { Op op; op.kind = 'S'; op.time = t; out.push_back(op); }

bool num(const char* s, std::uint64_t& v) { return eq::detail::parse_number(std::string(s), v); }

// Fibonacci tree of height h over consecutive keys; shorter subtree on the left.
struct FNode { std::uint64_t key; int l = -1, r = -1; };
int fib(int h, std::uint64_t& next, std::vector<FNode>& nodes) {
    if (h < 0) return -1;
    if (h == 0) { nodes.push_back({next++}); return static_cast<int>(nodes.size()) - 1; }
    const int l = fib(h - 2, next, nodes);
    nodes.push_back({next++});
    const int me = static_cast<int>(nodes.size()) - 1;
    const int r = fib(h - 1, next, nodes);
    nodes[static_cast<std::size_t>(me)].l = l;
    nodes[static_cast<std::size_t>(me)].r = r;
    return me;
}

int usage() {
    std::fprintf(stderr, "usage: gen ascending N | descending N | zigzag N | fib-delete H D | bloat L STEPS |\n"
                         "           drain N | storm N | regrow N | latesucc N Q | random SEED OPS LIVE\n");
    return 2;
}

}  // namespace

// Exact argument count and value ranges per mode, checked BEFORE generating anything (exit 2).
// Ranges keep every generated time far from int64 overflow and every stream a sane size.
struct Mode { const char* name; int args; std::uint64_t lo[3]; std::uint64_t hi[3]; };
constexpr std::uint64_t kMaxN = 100000000;       // 10^8 events
constexpr std::uint64_t kMaxOps = 1000000000;    // 10^9 operations
const Mode kModes[] = {
    {"ascending", 1, {1, 0, 0}, {kMaxN, 0, 0}},
    {"descending", 1, {1, 0, 0}, {kMaxN, 0, 0}},
    {"zigzag", 1, {1, 0, 0}, {kMaxN, 0, 0}},
    {"fib-delete", 2, {0, 0, 0}, {30, kMaxOps, 0}},     // h = 30: 3.5 million nodes
    {"bloat", 2, {1, 0, 0}, {kMaxN, kMaxOps, 0}},       // L >= 1: a hold loop needs a live event
    {"drain", 1, {1, 0, 0}, {kMaxN, 0, 0}},
    {"storm", 1, {1, 0, 0}, {kMaxN, 0, 0}},
    {"regrow", 1, {1, 0, 0}, {kMaxN, 0, 0}},
    {"latesucc", 2, {1, 0, 0}, {kMaxN, kMaxOps, 0}},
    {"random", 3, {0, 0, 1}, {~std::uint64_t{0}, kMaxOps, kMaxN}},
};

int run(int argc, char** argv) {
    if (argc < 2) return usage();
    const std::string kind = argv[1];
    const Mode* mode = nullptr;
    for (const Mode& m : kModes) if (kind == m.name) mode = &m;
    if (!mode) return usage();
    if (argc != 2 + mode->args) {
        std::fprintf(stderr, "gen %s: needs exactly %d argument(s)\n", mode->name, mode->args);
        return 2;
    }
    std::uint64_t v[3] = {0, 0, 0};
    for (int i = 0; i < mode->args; ++i) {
        if (!num(argv[2 + i], v[i]) || v[i] < mode->lo[i] || v[i] > mode->hi[i]) {
            std::fprintf(stderr, "gen %s: argument %d must be an integer in [%llu, %llu]\n", mode->name, i + 1,
                         (unsigned long long)mode->lo[i], (unsigned long long)mode->hi[i]);
            return 2;
        }
    }
    const std::uint64_t a = v[0], b = v[1], c = v[2];

    if (kind == "ascending") {
        for (std::uint64_t i = 1; i <= a; ++i) I(static_cast<Time>(i));
    } else if (kind == "descending") {
        for (std::uint64_t i = a; i >= 1; --i) I(static_cast<Time>(i));
    } else if (kind == "zigzag") {
        std::uint64_t lo = 1, hi = a;
        while (lo <= hi) { I(static_cast<Time>(lo++)); if (lo <= hi) I(static_cast<Time>(hi--)); }
    } else if (kind == "fib-delete") {
        std::vector<FNode> nodes;
        std::uint64_t next = 1;
        const int root = fib(static_cast<int>(a), next, nodes);
        std::deque<int> bfs{root};
        while (!bfs.empty()) {
            const int x = bfs.front();
            bfs.pop_front();
            I(static_cast<Time>(nodes[static_cast<std::size_t>(x)].key));
            if (nodes[static_cast<std::size_t>(x)].l >= 0) bfs.push_back(nodes[static_cast<std::size_t>(x)].l);
            if (nodes[static_cast<std::size_t>(x)].r >= 0) bfs.push_back(nodes[static_cast<std::size_t>(x)].r);
        }
        for (std::uint64_t i = 0; i < b; ++i) P();
    } else if (kind == "bloat") {
        const Time horizon = Time{4} * 1000000000000000000LL;      // beyond any time this run reaches
        eq::Rng r(1);
        std::set<std::pair<Time, std::uint64_t>> live;
        for (std::uint64_t i = 0; i < a; ++i) { const Time t = 1 + static_cast<Time>(r.below(1000)); live.insert({t, I(t)}); }
        Time now = 0;
        for (std::uint64_t s = 0; s < b; ++s) {
            P();
            now = live.begin()->first;
            live.erase(live.begin());
            const Time t = now + 1 + static_cast<Time>(r.below(1000));
            live.insert({t, I(t)});
            C(I(horizon + static_cast<Time>(s)));                   // far timer, cancelled at once
        }
    } else if (kind == "drain") {
        for (std::uint64_t i = 0; i < a; ++i) I(static_cast<Time>(i));
        for (std::uint64_t k = a / 2 + 1; k < a; ++k) C(k);
        for (std::uint64_t i = 0; i < a / 2; ++i) P();
    } else if (kind == "storm") {
        for (std::uint64_t i = 0; i < a; ++i) I(7, 1, static_cast<std::uint32_t>(i));
        for (std::uint64_t k = 0; k < a; k += 2) C(k);
        for (std::uint64_t k = 0; k < a; k += 2) C(k);             // stale: all false
        F(7, std::numeric_limits<eq::Seq>::max());
        F(7, std::numeric_limits<eq::Seq>::max() - 1);
        F(8, 1);                                                    // right seq, wrong time
        S(6); S(7);
        for (std::uint64_t i = 0; i < a; ++i) P();
    } else if (kind == "regrow") {
        for (int round = 0; round < 3; ++round) {
            const std::uint64_t base = inserts;
            for (std::uint64_t i = 0; i < a; ++i) I(static_cast<Time>((i * 7919) % (a + 1)));
            for (std::uint64_t k = base; k < base + a; k += 3) C(k);
            for (std::uint64_t i = 0; i < a; ++i) P();
        }
    } else if (kind == "latesucc") {
        for (std::uint64_t i = 0; i < a; ++i) I(static_cast<Time>(i));
        for (std::uint64_t q = 0; q < b; ++q) S(static_cast<Time>(a) - 2 - static_cast<Time>(q % 3));
    } else if (kind == "random") {
        difftest::GenConfig cfg;
        cfg.seed = a;
        cfg.ops = b;
        cfg.max_live = c;
        out = difftest::generate(cfg);
    } else {
        return usage();
    }
    std::ios::sync_with_stdio(false);
    eq::write_ops(std::cout, out);
    return 0;
}

int main(int argc, char** argv) {
    try {
        return run(argc, argv);
    } catch (const std::bad_alloc&) {
        std::fprintf(stderr, "gen: out of memory\n");
        return 4;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "gen: %s\n", e.what());
        return 4;
    }
}
