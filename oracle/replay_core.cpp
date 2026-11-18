// oracle/replay_core.cpp — see replay_core.h. Owner: Oracle owner.
#include "replay_core.h"

#include "canonical.h"

namespace replay {

using eq::Event;

static const Event kSentinel{eq::Key{-123456789, 987654321}, 0xDEADBEEFu, 0xFEEDFACEu};

static bool same(const Event& a, const Event& b) {
    return a.key == b.key && a.type == b.type && a.data == b.data;
}

static std::string query_line(char c, bool ok, const Event& out) {
    if (ok) return std::string(1, c) + " " + eq::event_text(out);
    return std::string(1, c) + (same(out, kSentinel) ? " -" : " - !out-changed");
}

std::string apply(eq::EventQueue& q, const eq::Op& op, State& st) {
    Event out = kSentinel;
    switch (op.kind) {
        case 'I': {
            const eq::Handle h = q.insert(op.time, op.type, op.data);
            st.handles.push_back(h);
            return "I " + std::to_string(h.seq);
        }
        case 'C': {
            const bool ok = op.k < st.handles.size() && q.cancel(st.handles[static_cast<std::size_t>(op.k)]);
            return ok ? "C 1" : "C 0";
        }
        case 'F':
            return q.cancel(eq::Handle{op.time, op.seq}) ? "F 1" : "F 0";
        case 'P': { const bool ok = q.pop_min(out); return query_line('P', ok, out); }
        case 'K': { const bool ok = q.peek_min(out); return query_line('K', ok, out); }
        case 'S': { const bool ok = q.successor(op.time, out); return query_line('S', ok, out); }
        default:
            return "? bad op";
    }
}

std::string end_line(const eq::EventQueue& q) {
    return "END size=" + std::to_string(q.size()) + " issued=" + std::to_string(q.issued());
}

}  // namespace replay
