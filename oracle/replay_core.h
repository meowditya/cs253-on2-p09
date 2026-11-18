// oracle/replay_core.h — apply .ops operations to a queue and print canonical result lines
// (INTERFACE.md §7). Shared by bin/replay, the differential test and the suite runner.
// Owner: Oracle owner.
#pragma once
#include <string>
#include <vector>

#include "event_queue.h"
#include "opstream.h"

namespace replay {

struct State {
    std::vector<eq::Handle> handles;   // H[k] = handle returned by the k-th insert
};

// Applies one operation and returns its result line, e.g. "P 10 3 0 7" or "C 0".
// A failed pop/peek/successor must leave its output untouched; if a structure changes it anyway,
// the line gets the suffix " !out-changed", which makes it differ from the oracle's.
std::string apply(eq::EventQueue& q, const eq::Op& op, State& st);

// "END size=<n> issued=<n>"
std::string end_line(const eq::EventQueue& q);

}  // namespace replay
