// oracle/difftest_core.h — differential testing: generate a seeded operation history, run it on
// several queues in lockstep, compare every result line, run the invariant checkers, and shrink a
// failing history to a small literal .ops file. Owner: Oracle owner.
#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "event_queue.h"
#include "opstream.h"

namespace difftest {

struct GenConfig {
    std::uint64_t seed = 1;
    std::size_t ops = 1000000;
    std::size_t max_live = 10000;   // keeps the O(n) oracle fast; enforced by a generator-side reference
};

// Seeded history mixing inserts (narrow time range -> many ties, some in the past, some extreme),
// cancels of live / already-removed / never-issued handles, forged handles (right seq + wrong time,
// reserved seqs), pops, peeks and successor queries (including INT64_MIN / INT64_MAX).
std::vector<eq::Op> generate(const GenConfig& cfg);

using Factory = std::function<std::unique_ptr<eq::EventQueue>()>;

struct Failure {
    bool failed = false;
    std::size_t index = 0;          // operation index where it was detected
    std::string category;           // "MISMATCH", "CHECKER" or "EXCEPTION"
    std::string detail;
};

// factories[0] is the reference (normally the oracle). Checkers run on every queue after each of
// the first `check_all_first` operations, then every `check_every` operations, and at the end.
Failure run(const std::vector<eq::Op>& ops, const std::vector<Factory>& factories,
            std::size_t check_every, std::size_t check_all_first);

// Delta-debugging shrink: keeps a candidate only if it still fails with the same category.
// If the final single-deletion pass completes, the result is 1-minimal under single-operation
// deletion for that predicate (still not necessarily globally shortest) and one_minimal is true.
// If the replay budget runs out first, the result is an intermediate (smaller, still failing)
// history and one_minimal is false. Replays run in-process: a structure that crashes the process
// (segfault, abort) cannot be shrunk this way — rerun the saved prefix under a debugger instead.
struct ShrinkResult {
    std::vector<eq::Op> ops;
    bool one_minimal = false;
    std::size_t replays = 0;
};
ShrinkResult shrink(std::vector<eq::Op> ops, const std::vector<Factory>& factories,
                    const std::string& category, std::size_t max_replays = 20000);

}  // namespace difftest
