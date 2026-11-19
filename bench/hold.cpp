// bench/hold.cpp — bin/bench_hold: Jones's hold model. Owner: Analyst. Derivation: D6.
//
//   bench_hold --queue heap|avl|oracle|control --dist D --n N [--holds M] [--warmup W]
//              [--seed S] [--mean-us U] [--repeat R] [--order O] [--csv FILE]
//
// Procedure (D. W. Jones, CACM 29(4), 1986, section 2):
//  1. Build the queue by a random walk of enqueues and dequeues, P(enqueue) = 0.55 (forced when
//     empty), each enqueued time = time of the most recently dequeued item + increment, until the
//     size is N. Untimed. (Gives a representative shape, not just representative times.)
//  2. W warm-up holds, untimed (default max(10^6, 5N)).
//  3. Pre-generate M increments, then time M holds. One hold = pop_min, then insert at the popped
//     time + increment: two queue calls; the size is N between holds.
// "control" is the overhead control: it replays a recorded trace of popped times through the same
// loop arithmetic with no queue at all, measured as context (never subtracted automatically).
// Operation counters (count build) are reset after warm-up, so they describe the timed holds only.
// The checksum of popped times is identical for every correct queue (and the control).
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include "bench_common.h"
#include "distributions.h"
#include "opstream.h"
#include "queue_factory.h"
#include "rng.h"

namespace {
int fail(const char* m) { std::fprintf(stderr, "bench_hold: %s\n", m); return 2; }
const eq::Time kMax = std::numeric_limits<eq::Time>::max();
}

static int run_main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--profile-support") return bench::print_profile_support();
    std::string queue = "heap", dist_name = "exponential", csv, order = "0";
    std::uint64_t n = 1000, holds = 0, warmup = 0, seed = 1, mean_us = 1000, repeat = 0;
    bool holds_set = false, warmup_set = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (i + 1 >= argc) return fail("missing value");
        const std::string v = argv[++i];
        using eq::detail::parse_number;
        bool ok = true;
        if (a == "--queue") queue = v;
        else if (a == "--dist") dist_name = v;
        else if (a == "--n") ok = parse_number(v, n);
        else if (a == "--holds") { ok = parse_number(v, holds); holds_set = true; }
        else if (a == "--warmup") { ok = parse_number(v, warmup); warmup_set = true; }
        else if (a == "--seed") ok = parse_number(v, seed);
        else if (a == "--mean-us") ok = parse_number(v, mean_us);
        else if (a == "--repeat") ok = parse_number(v, repeat);
        else if (a == "--order") order = v;
        else if (a == "--csv") csv = v;
        else return fail(("unknown option " + a).c_str());
        if (!ok) return fail(("bad number for " + a).c_str());
    }
    dist::Kind dk;
    if (!dist::parse(dist_name, dk)) return fail("unknown --dist");
    if (n == 0 || n > 1000000000ULL) return fail("--n must be in 1..10^9");
    if (mean_us == 0 || mean_us > (1ULL << 40)) return fail("--mean-us must be in 1..2^40");
    if (!holds_set) holds = std::max<std::uint64_t>(1000000, n);
    if (!warmup_set) warmup = std::max<std::uint64_t>(1000000, 5 * n);
    if (holds == 0 || holds > 1000000000ULL) return fail("--holds must be in 1..10^9");
    if (warmup > 100000000000ULL) return fail("--warmup must be <= 10^11");
    const bool control = queue == "control";
    if (queue == "oracle" && n > 20000) return fail("the oracle is O(n) per insert: use --n <= 20000");
    auto q = app::make_queue(control ? "heap" : queue);
    if (!q || queue == "heap-nopurge") return fail("--queue must be heap, avl, oracle or control");

    eq::Rng rng(seed);
    std::int64_t inc = 0;
    auto next_inc = [&]() {
        if (!dist::increment_us(dk, rng, static_cast<std::int64_t>(mean_us), inc)) { std::fprintf(stderr, "bad increment\n"); std::exit(4); }
        return inc;
    };
    auto checked_add = [](eq::Time t, std::int64_t d) { return bench::checked_add(t, d, "bench_hold"); };

    // 1. Jones initialisation (untimed)
    eq::Time last = 0;
    eq::Event e{};
    while (q->size() < n) {
        if (q->size() == 0 || rng.below(100) < 55) q->insert(checked_add(last, next_inc()), 0, 0);
        else { q->pop_min(e); last = e.key.time; }
    }
    // 2. warm-up (untimed)
    for (std::uint64_t i = 0; i < warmup; ++i) {
        q->pop_min(e);
        q->insert(checked_add(e.key.time, next_inc()), 0, 0);
    }
    // 3. pre-generated increments
    std::vector<std::int64_t> incs(holds);
    std::uint64_t zeros = 0;
    for (auto& x : incs) { x = next_inc(); zeros += x == 0; }
    std::vector<eq::Time> trace;
    if (control) {                                    // record the popped times on the real heap
        trace.resize(holds);
        for (std::uint64_t i = 0; i < holds; ++i) {
            q->pop_min(e);
            trace[i] = e.key.time;
            q->insert(checked_add(e.key.time, incs[i]), 0, 0);
        }
    }
    q->reset_counters();
    if (auto* h = dynamic_cast<structA::LazyHeap*>(q.get())) h->reset_peak_physical();
    const bench::Stats before = bench::stats_of(*q);

    std::uint64_t checksum = 0;
    bench::ProfileRegion region;                      // profiler sees only the timed loop
    region.start();
    const std::uint64_t t0 = bench::now_ns();
    if (control) {
        for (std::uint64_t i = 0; i < holds; ++i) {
            const eq::Time t = trace[i];
            if (incs[i] > kMax - t) return 4;
            checksum = checksum * 31 + static_cast<std::uint64_t>(t);
            trace[i] = t + incs[i];                   // a store, like the insert's
        }
    } else {
        eq::EventQueue& Q = *q;
        for (std::uint64_t i = 0; i < holds; ++i) {
            if (!Q.pop_min(e)) return 3;
            const eq::Time t = e.key.time;
            if (incs[i] > kMax - t) return 4;
            checksum = checksum * 31 + static_cast<std::uint64_t>(t);
            Q.insert(t + incs[i], e.type, e.data);
        }
    }
    const std::uint64_t elapsed = bench::now_ns() - t0;
    region.stop();
    if (!region.ok()) { std::fprintf(stderr, "bench_hold: profiler region control failed\n"); return 3; }
    if (!control && q->size() != n) return 3;

    const bench::Stats after = bench::stats_of(*q);
    const eq::Counters& c = q->counters();
    const double nsph = static_cast<double>(elapsed) / static_cast<double>(holds);
    char nsbuf[32], zbuf[32], tbuf[32];
    std::snprintf(nsbuf, sizeof nsbuf, "%.3f", nsph);
    std::snprintf(tbuf, sizeof tbuf, "%.1f", 1e9 / nsph);           // throughput: holds per second
    std::snprintf(zbuf, sizeof zbuf, "%.6f", static_cast<double>(zeros) / static_cast<double>(holds));
    using bench::u;
    const std::string header =
        "experiment,version,build,queue,dist,n,warmup,holds,seed,repeat,order,mean_us,elapsed_ns,ns_per_hold,holds_per_sec,"
        "checksum,zero_inc_frac,peak_physical,purges,rebalance_single,rebalance_double,comparisons,moves,hash_probes,"
        "hash_rehash_probes,table_rebuilds,table_rebuild_slots,bytes";
    const std::string row = "hold,3," + std::string(bench::build_kind()) + "," + queue + "," + dist_name + "," + u(n) + "," +
        u(warmup) + "," + u(holds) + "," + u(seed) + "," + u(repeat) + "," + order + "," + u(mean_us) + "," + u(elapsed) + "," +
        nsbuf + "," + tbuf + "," + u(checksum) + "," + zbuf + "," + u(after.peak_physical) + "," + u(after.purges - before.purges) + "," +
        u(after.reb_single - before.reb_single) + "," + u(after.reb_double - before.reb_double) + "," + u(c.comparisons) + "," +
        u(c.moves) + "," + u(c.hash_probes) + "," + u(c.hash_rehash_probes) + "," +
        u(after.table_rebuilds - before.table_rebuilds) + "," + u(after.table_rebuild_slots - before.table_rebuild_slots) + "," +
        u(after.bytes);
    if (!csv.empty() && !bench::append_csv(csv, header, row)) return bench::csv_failed("bench_hold", csv);
    std::printf("%s\n", row.c_str());
    return 0;
}

int main(int argc, char** argv) {
    return bench::guarded_main("bench_hold", [&] { return run_main(argc, argv); });
}
