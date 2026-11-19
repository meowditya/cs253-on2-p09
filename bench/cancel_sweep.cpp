// bench/cancel_sweep.cpp — bin/bench_sweep: throughput as the cancellation rate rises 0% -> 95%.
// Owner: Analyst. Derivation: D6.
//
//   bench_sweep --queue heap|avl|oracle --cancel-bp B --policy uniform|latest --n N [--steps M]
//               [--warmup W] [--seed S] [--dist D] [--mean-us U] [--repeat R] [--order O] [--csv FILE]
//
// One STEP = exactly one successful removal, then one insertion (two queue calls); the size is
// back to N after every step. Of the M timed steps exactly K = round(B/10000 * M) are cancellations
// (a shuffled multiset of K cancel-steps and M-K pop-steps), so the x-axis value K/M is exact.
//   cancel step : cancel a currently pending event chosen by the victim policy
//                 (uniform = uniformly among pending events; latest = the largest pending key,
//                 which delays stale-root cleanup and stresses the heap's purge)
//   pop step    : pop the minimum; last_removed_time = its time
//   then        : insert one replacement at last_removed_time + increment
// The whole stream (handles, expected pops, insert times) is generated BEFORE timing with
// library containers (allowed for data generation), then replayed identically on A and B. The
// timed loop does only queue calls; the checksum of popped keys is verified after timing.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "bench_common.h"
#include "distributions.h"
#include "opstream.h"
#include "queue_factory.h"
#include "rng.h"

namespace {
int fail(const char* m) { std::fprintf(stderr, "bench_sweep: %s\n", m); return 2; }

struct Step {
    std::uint8_t cancel;      // 1 = cancel `victim`, 0 = pop_min
    eq::Key victim;           // for cancel steps
    eq::Time insert_time;     // replacement insert
};
}  // namespace

static int run_main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--profile-support") return bench::print_profile_support();
    std::string queue = "heap", policy = "uniform", dist_name = "exponential", csv, order = "0";
    std::uint64_t bp = 0, n = 10000, steps = 1000000, warmup = 0, seed = 1, mean_us = 1000, repeat = 0;
    bool warmup_set = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (i + 1 >= argc) return fail("missing value");
        const std::string v = argv[++i];
        using eq::detail::parse_number;
        bool ok = true;
        if (a == "--queue") queue = v;
        else if (a == "--policy") policy = v;
        else if (a == "--dist") dist_name = v;
        else if (a == "--cancel-bp") ok = parse_number(v, bp);
        else if (a == "--n") ok = parse_number(v, n);
        else if (a == "--steps") ok = parse_number(v, steps);
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
    if (bp > 10000) return fail("--cancel-bp must be in 0..10000");
    if (n == 0 || n > 1000000000ULL) return fail("--n must be in 1..10^9");
    if (steps == 0 || steps > 1000000000ULL) return fail("--steps must be in 1..10^9");
    if (mean_us == 0 || mean_us > (1ULL << 40)) return fail("--mean-us must be in 1..2^40");
    if (policy != "uniform" && policy != "latest") return fail("--policy must be uniform or latest");
    if (queue == "oracle" && n > 20000) return fail("the oracle is O(n) per insert: use --n <= 20000");
    if (!warmup_set) warmup = n;
    if (warmup > 1000000000ULL) return fail("--warmup must be <= 10^9");   // so warmup + steps cannot overflow
    auto q = app::make_queue(queue);
    if (!q || queue == "heap-nopurge") return fail("--queue must be heap, avl or oracle");

    // ---------------- generate the stream (untimed) ----------------
    eq::Rng rng(seed);
    std::int64_t inc = 0;
    auto next_inc = [&]() {
        if (!dist::increment_us(dk, rng, static_cast<std::int64_t>(mean_us), inc)) std::exit(4);
        return inc;
    };
    std::vector<eq::Time> preload(n);
    for (auto& t : preload) t = next_inc();                  // N independent times after 0

    std::set<eq::Key> live;                                  // reference pending set
    std::vector<eq::Key> pool;                               // for uniform picks
    std::unordered_map<eq::Seq, std::size_t> pos;
    auto add = [&](const eq::Key& k) { live.insert(k); pos[k.seq] = pool.size(); pool.push_back(k); };
    auto remove = [&](const eq::Key& k) {
        live.erase(k);
        const std::size_t p = pos[k.seq];
        pool[p] = pool.back();
        pos[pool[p].seq] = p;
        pool.pop_back();
        pos.erase(k.seq);
    };
    eq::Seq next_seq = 0;
    for (eq::Time t : preload) add(eq::Key{t, next_seq++});

    auto flags = [&](std::uint64_t m) {                      // exactly round(bp/10000 * m) cancels, shuffled
        // round(bp * m / 10000) without forming bp * m (which could overflow for huge m)
        const std::uint64_t k = (m / 10000) * bp + ((m % 10000) * bp + 5000) / 10000;
        std::vector<std::uint8_t> f(m, 0);
        std::fill(f.begin(), f.begin() + static_cast<std::ptrdiff_t>(k), 1);
        for (std::uint64_t i = m; i > 1; --i) std::swap(f[i - 1], f[rng.below(i)]);
        return std::make_pair(f, k);
    };
    const auto wf = flags(warmup);
    const auto tf = flags(steps);
    const std::uint64_t K = tf.second;

    std::vector<Step> stream;
    stream.reserve(warmup + steps);
    eq::Time last_removed = 0;
    std::uint64_t expected = 0;
    for (std::uint64_t i = 0; i < warmup + steps; ++i) {
        const bool cancel = i < warmup ? wf.first[i] : tf.first[i - warmup];
        Step s{};
        if (cancel) {
            s.cancel = 1;
            s.victim = policy == "uniform" ? pool[rng.below(pool.size())] : *live.rbegin();
            remove(s.victim);
        } else {
            const eq::Key k = *live.begin();
            remove(k);
            last_removed = k.time;
            if (i >= warmup) expected = expected * 31 + static_cast<std::uint64_t>(k.time) + k.seq;
        }
        s.insert_time = bench::checked_add(last_removed, next_inc(), "bench_sweep");
        add(eq::Key{s.insert_time, next_seq++});
        stream.push_back(s);
    }
    live.clear(); pool.clear(); pos.clear();

    // ---------------- replay ----------------
    for (eq::Time t : preload) q->insert(t, 0, 0);
    eq::EventQueue& Q = *q;
    eq::Event e{};
    for (std::uint64_t i = 0; i < warmup; ++i) {             // untimed warm-up, same on A and B
        const Step& s = stream[i];
        if (s.cancel ? !Q.cancel(s.victim) : !Q.pop_min(e)) { std::fprintf(stderr, "bench_sweep: warm-up step %llu failed\n", (unsigned long long)i); return 3; }
        Q.insert(s.insert_time, 0, 0);
    }
    q->reset_counters();
    if (auto* h = dynamic_cast<structA::LazyHeap*>(q.get())) h->reset_peak_physical();
    const bench::Stats before = bench::stats_of(*q);
    std::uint64_t checksum = 0;
    bench::ProfileRegion region;
    region.start();
    const std::uint64_t t0 = bench::now_ns();
    for (std::uint64_t i = warmup; i < warmup + steps; ++i) {
        const Step& s = stream[i];
        if (s.cancel) {
            if (!Q.cancel(s.victim)) return 3;               // must never fail: stop, never substitute
        } else {
            if (!Q.pop_min(e)) return 3;
            checksum = checksum * 31 + static_cast<std::uint64_t>(e.key.time) + e.key.seq;
        }
        Q.insert(s.insert_time, 0, 0);
    }
    const std::uint64_t elapsed = bench::now_ns() - t0;
    region.stop();
    if (!region.ok()) { std::fprintf(stderr, "bench_sweep: profiler region control failed\n"); return 3; }
    if (checksum != expected || q->size() != n) { std::fprintf(stderr, "bench_sweep: replay did not match the generated stream\n"); return 3; }

    const bench::Stats after = bench::stats_of(*q);
    const eq::Counters& c = q->counters();
    char nsbuf[32], tbuf[32];
    const double nsps = static_cast<double>(elapsed) / static_cast<double>(steps);
    std::snprintf(nsbuf, sizeof nsbuf, "%.3f", nsps);
    std::snprintf(tbuf, sizeof tbuf, "%.1f", 1e9 / nsps);            // throughput: steps per second
    using bench::u;
    const std::string header =
        "experiment,version,build,queue,policy,dist,n,warmup,steps,cancels,cancel_bp,seed,repeat,order,mean_us,"
        "elapsed_ns,ns_per_step,steps_per_sec,checksum,peak_physical,purges,purged,stale_discards,rebalance_single,"
        "rebalance_double,comparisons,moves,hash_probes,hash_rehash_probes,table_rebuilds,table_rebuild_slots,bytes";
    const std::string row = "sweep,3," + std::string(bench::build_kind()) + "," + queue + "," + policy + "," + dist_name + "," +
        u(n) + "," + u(warmup) + "," + u(steps) + "," + u(K) + "," + u(bp) + "," + u(seed) + "," + u(repeat) + "," + order + "," +
        u(mean_us) + "," + u(elapsed) + "," + nsbuf + "," + tbuf + "," + u(checksum) + "," + u(after.peak_physical) + "," +
        u(after.purges - before.purges) + "," + u(after.purged - before.purged) + "," + u(after.stale - before.stale) + "," +
        u(after.reb_single - before.reb_single) + "," + u(after.reb_double - before.reb_double) + "," + u(c.comparisons) + "," +
        u(c.moves) + "," + u(c.hash_probes) + "," + u(c.hash_rehash_probes) + "," +
        u(after.table_rebuilds - before.table_rebuilds) + "," + u(after.table_rebuild_slots - before.table_rebuild_slots) + "," +
        u(after.bytes);
    if (!csv.empty() && !bench::append_csv(csv, header, row)) return bench::csv_failed("bench_sweep", csv);
    std::printf("%s\n", row.c_str());
    return 0;
}

int main(int argc, char** argv) {
    return bench::guarded_main("bench_sweep", [&] { return run_main(argc, argv); });
}
