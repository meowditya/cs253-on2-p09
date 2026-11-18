// app/simulator.cpp — see simulator.h. Owner: Architect.
#include "simulator.h"

#include <algorithm>
#include <limits>
#include <vector>

#include "canonical.h"
#include "rng.h"

namespace sim {

using eq::Event;
using eq::Handle;
using eq::Time;

namespace {
constexpr Time kTimeMax = std::numeric_limits<Time>::max();
constexpr std::uint64_t kMillion = 1000000;
std::string u(std::uint64_t v) { return std::to_string(v); }
std::string s(std::int64_t v) { return std::to_string(v); }
}  // namespace

std::string validate(const Config& c) {
    if (c.flows < 1 || c.flows > std::numeric_limits<std::uint32_t>::max()) return "--flows must be in 1..4294967295";
    if (c.loss_ppm > kMillion) return "--loss-ppm must be in 0..1000000";
    if (c.rtt_min_us <= 0) return "--rtt-min must be positive";
    if (c.rtt_max_us < c.rtt_min_us) return "--rtt-max must be >= --rtt-min";
    if (c.rto_us <= c.rtt_max_us) return "--rto must be greater than --rtt-max (an ACK must beat its own timer)";
    if (c.idle_us <= 0) return "--idle must be positive";
    if (c.monitor_us < 0) return "--monitor must be >= 0 (0 disables it)";
    // Each flow inserts at most 3 events at start-up (+1 monitor) and each dispatch at most 3
    // (an ACK inserts IDLE, ACK, TIMEOUT). Keep the total below the sequence limit, without overflow.
    const std::uint64_t startup = 3 * c.flows + 1;          // flows < 2^32, cannot overflow
    if (startup > eq::kSeqLimit || c.events > (eq::kSeqLimit - startup) / 3) return "--events too large: sequence numbers would run out";
    return "";
}

Result run(eq::EventQueue& q, const Config& c, std::string* trace) {
    const std::string why = validate(c);
    if (!why.empty()) throw Error(2, why);
    if (!q.empty() || q.issued() != 0) throw Error(3, "simulation needs a fresh queue");

    Result r;
    eq::Rng rng(c.seed);
    eq::Fnv64 events_hash, monitor_hash;
    const std::uint32_t flows = static_cast<std::uint32_t>(c.flows);
    std::vector<Handle> rto_handle(flows), idle_handle(flows);
    Time now = 0;

    // Every insertion goes through here: checked time addition, counting, peak live size.
    auto schedule = [&](Time delay, std::uint32_t type, std::uint32_t data) {
        if (delay < 0 || delay > kTimeMax - now) throw Error(4, "simulated time would overflow int64");
        const Handle h = q.insert(now + delay, type, data);
        ++r.inserted;
        r.peak_live = std::max<std::uint64_t>(r.peak_live, q.size());
        return h;
    };
    // One transmission attempt: always one loss draw; one RTT draw only if delivered.
    auto transmit = [&](std::uint32_t f) {
        ++r.transmissions;
        if (rng.below(kMillion) >= c.loss_ppm) {
            const Time rtt = c.rtt_min_us + static_cast<Time>(rng.below(static_cast<std::uint64_t>(c.rtt_max_us - c.rtt_min_us) + 1));
            schedule(rtt, ACK, f);
        }
        rto_handle[f] = schedule(c.rto_us, TIMEOUT, f);
    };
    auto require = [](bool ok, const char* what) { if (!ok) throw Error(3, what); };

    for (std::uint32_t f = 0; f < flows; ++f) {             // start-up at now = 0
        transmit(f);
        idle_handle[f] = schedule(c.idle_us, IDLE, f);
    }
    if (c.monitor_us > 0) schedule(c.monitor_us, MONITOR, 0);

    Time previous = 0;
    for (std::uint64_t i = 0; i < c.events; ++i) {
        Event e{};
        require(q.pop_min(e), "queue empty before the event budget was reached");
        require(e.key.time >= now, "time went backwards (D3 violated)");
        if (r.processed > 0 && e.key.time == previous) ++r.ties;
        now = previous = e.key.time;
        ++r.processed;
        events_hash.event(e);
        if (trace) { *trace += "E "; *trace += eq::event_text(e); *trace += '\n'; }

        const std::uint32_t f = e.data;
        switch (e.type) {
            case ACK: {
                require(f < flows, "ACK for an unknown flow");
                const bool rto_ok = q.cancel(rto_handle[f]);       // never inside assert(): the
                require(rto_ok, "ACK could not cancel its RTO");   // cancel itself must always run
                ++r.rto_cancelled;
                const bool idle_ok = q.cancel(idle_handle[f]);
                require(idle_ok, "ACK could not cancel the watchdog");
                ++r.idle_cancelled;
                idle_handle[f] = schedule(c.idle_us, IDLE, f);
                ++r.delivered;
                transmit(f);                                         // next packet
                break;
            }
            case TIMEOUT:
                require(f < flows && e.key == rto_handle[f], "TIMEOUT is not the flow's current timer");
                ++r.retransmissions;
                transmit(f);                                         // same packet again
                break;
            case IDLE:
                require(f < flows && e.key == idle_handle[f], "IDLE is not the flow's current watchdog");
                ++r.idle_fired;
                idle_handle[f] = schedule(c.idle_us, IDLE, f);
                break;
            case MONITOR: {
                require(f == 0, "MONITOR with data != 0");
                ++r.monitors;
                if (c.rto_us > kTimeMax - now) throw Error(4, "monitor threshold would overflow int64");
                const Time threshold = now + c.rto_us;
                Event next{};
                const bool found = q.successor(threshold, next);    // query before re-arming
                monitor_hash.i64(threshold);
                monitor_hash.byte(found ? 1 : 0);
                if (found) monitor_hash.event(next);
                if (trace) {
                    *trace += "M " + s(threshold) + (found ? " 1 " + eq::event_text(next) : std::string(" 0")) + "\n";
                }
                schedule(c.monitor_us, MONITOR, 0);
                break;
            }
            default:
                throw Error(3, "unknown event type");
        }
        require(r.inserted == r.processed + r.cancelled() + q.size(), "conservation violated: inserted != processed + cancelled + pending");
    }
    r.pending = q.size();
    r.end_time_us = now;
    r.checksum = events_hash.h;
    r.monitor_hash = monitor_hash.h;
    return r;
}

std::string params_text(const Config& c) {
    return "format=project9-v2\nparams flows=" + u(c.flows) + " events=" + u(c.events) + " loss_ppm=" + u(c.loss_ppm) +
           " seed=" + u(c.seed) + " rtt_min_us=" + s(c.rtt_min_us) + " rtt_max_us=" + s(c.rtt_max_us) +
           " rto_us=" + s(c.rto_us) + " idle_us=" + s(c.idle_us) + " monitor_us=" + s(c.monitor_us) + "\n";
}

std::string summary_text(const Result& r) {
    return "inserted=" + u(r.inserted) + " processed=" + u(r.processed) + " cancelled=" + u(r.cancelled()) +
           " pending=" + u(r.pending) + "\n" +
           "rto_cancelled=" + u(r.rto_cancelled) + " idle_cancelled=" + u(r.idle_cancelled) + "\n" +
           "transmissions=" + u(r.transmissions) + " retransmissions=" + u(r.retransmissions) +
           " delivered=" + u(r.delivered) + " idle_fired=" + u(r.idle_fired) + " monitors=" + u(r.monitors) + "\n" +
           "ties=" + u(r.ties) + " peak_live=" + u(r.peak_live) + " end_time_us=" + s(r.end_time_us) + "\n" +
           "checksum=" + eq::hex16(r.checksum) + "\n" +
           "monitor_successors=" + eq::hex16(r.monitor_hash) + "\n";
}

}  // namespace sim
