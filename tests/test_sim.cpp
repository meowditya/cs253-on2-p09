// tests/test_sim.cpp — simulator scenarios on all three queues. Owner: Architect.
// Every scenario must give identical traces and summaries on heap, AVL and oracle; the
// simulator itself checks conservation and handle consistency after every dispatch.
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "queue_factory.h"
#include "simulator.h"
#include "test_util.h"

struct Outcome {
    bool ok = false;
    int code = 0;
    sim::Result r;
    std::string trace;
};

static Outcome run_on(const std::string& queue, const sim::Config& c, bool with_trace = true) {
    Outcome o;
    auto q = app::make_queue(queue);
    try {
        o.r = sim::run(*q, c, with_trace ? &o.trace : nullptr);
        o.ok = true;
    } catch (const sim::Error& e) {
        o.code = e.code;
    }
    return o;
}

// Same trace and summary on all three structures; returns the heap's outcome.
static Outcome same_everywhere(const sim::Config& c, const char* label) {
    const Outcome h = run_on("heap", c), a = run_on("avl", c), o = run_on("oracle", c);
    const bool same = h.ok && a.ok && o.ok && h.trace == a.trace && h.trace == o.trace &&
                      sim::summary_text(h.r) == sim::summary_text(a.r) && sim::summary_text(h.r) == sim::summary_text(o.r);
    testutil::check(same, std::string("identical on heap/avl/oracle: ") + label);
    return h;
}

static sim::Config small(std::uint64_t seed) {
    sim::Config c;
    c.flows = 300;
    c.events = 5000;
    c.seed = seed;
    return c;
}

int main() {
    for (std::uint64_t seed : {1, 2, 3}) {
        const Outcome d = same_everywhere(small(seed), "defaults");
        CHECK(d.r.processed == 5000 && d.r.inserted == d.r.processed + d.r.cancelled() + d.r.pending);
        CHECK(2 * d.r.cancelled() > d.r.inserted);                 // most scheduled events cancelled
        CHECK(d.r.ties > 0 && d.r.monitors > 0);

        sim::Config c = small(seed);
        c.loss_ppm = 0;                                            // nothing lost: no retransmissions
        const Outcome z = same_everywhere(c, "loss 0");
        CHECK(z.r.retransmissions == 0 && z.r.rto_cancelled == z.r.delivered);

        c = small(seed);
        c.loss_ppm = 1000000;                                      // everything lost: only timers fire
        c.idle_us = 300000;
        const Outcome all = same_everywhere(c, "loss 100%");
        CHECK(all.r.delivered == 0 && all.r.cancelled() == 0 && all.r.idle_fired > 0);

        c = small(seed);
        c.loss_ppm = 500000;                                       // heavy loss + short watchdog
        c.idle_us = 250000;
        const Outcome hl = same_everywhere(c, "loss 50%, short watchdog");
        CHECK(hl.r.idle_fired > 0 && hl.r.retransmissions > 0);

        c = small(seed);
        c.monitor_us = 0;                                          // monitor disabled
        const Outcome nm = same_everywhere(c, "monitor off");
        CHECK(nm.r.monitors == 0 && nm.trace.find("\nM ") == std::string::npos);

        c = small(seed);
        c.rtt_min_us = c.rtt_max_us = 20000;                       // fixed RTT
        c.idle_us = 20000;                                         // watchdog ties with ACKs
        c.rto_us = 20001;
        same_everywhere(c, "fixed RTT, watchdog tied with ACK");
    }
    {   // one flow, zero budget, tiny budget
        sim::Config c;
        c.flows = 1;
        c.events = 0;
        const Outcome z = same_everywhere(c, "zero budget");
        CHECK(z.r.processed == 0 && z.r.inserted == z.r.pending && z.r.peak_live == z.r.inserted);
        c.events = 1;
        const Outcome one = same_everywhere(c, "one dispatch");
        CHECK(one.r.processed == 1);
    }
    {   // successor that finds nothing: monitor threshold beyond every pending event
        sim::Config c = small(4);
        c.loss_ppm = 0;
        c.idle_us = 30000;
        c.rtt_min_us = 1000;
        c.rtt_max_us = 2000;
        c.rto_us = 50000000;      // every pending event is at most now + rto, so successor(now + rto)
        c.monitor_us = 1000;      // (strictly greater) finds nothing
        const Outcome o = same_everywhere(c, "successor none");
        std::size_t m_lines = 0, m_none = 0, pos = 0;
        while (pos < o.trace.size()) {                             // every "M <threshold> <found>" line
            const std::size_t end = o.trace.find('\n', pos);
            const std::string line = o.trace.substr(pos, end - pos);
            if (line.rfind("M ", 0) == 0) { ++m_lines; if (line.size() >= 2 && line.compare(line.size() - 2, 2, " 0") == 0 && line.find(' ', 2) == line.size() - 2) ++m_none; }
            pos = end + 1;
        }
        CHECK(o.r.monitors > 0 && m_lines == o.r.monitors && m_none == m_lines);
    }
    {   // trace on/off: identical summary
        const Outcome a = run_on("heap", small(9), true), b = run_on("heap", small(9), false);
        CHECK(a.ok && b.ok && sim::summary_text(a.r) == sim::summary_text(b.r));
    }
    {   // validation
        sim::Config c;
        c.flows = 0;                         CHECK(!sim::validate(c).empty());
        c = sim::Config{}; c.loss_ppm = 1000001; CHECK(!sim::validate(c).empty());
        c = sim::Config{}; c.rtt_min_us = 0;     CHECK(!sim::validate(c).empty());
        c = sim::Config{}; c.rtt_max_us = 5000;  CHECK(!sim::validate(c).empty());   // < rtt_min
        c = sim::Config{}; c.rto_us = 50000;     CHECK(!sim::validate(c).empty());   // == rtt_max
        c = sim::Config{}; c.idle_us = 0;        CHECK(!sim::validate(c).empty());
        c = sim::Config{}; c.monitor_us = -1;    CHECK(!sim::validate(c).empty());
        c = sim::Config{}; c.flows = 4294967296ULL; CHECK(!sim::validate(c).empty());
        c = sim::Config{}; c.events = eq::kSeqLimit / 2; CHECK(!sim::validate(c).empty());
        CHECK(sim::validate(sim::Config{}).empty());
    }
    {   // overflow guard: a watchdog period near INT64_MAX overflows on its first re-arm
        sim::Config c = small(1);
        c.idle_us = std::numeric_limits<eq::Time>::max() - 10;
        const Outcome o = run_on("heap", c);
        CHECK(!o.ok && o.code == 4);
    }
    return testutil::finish("test_sim");
}
