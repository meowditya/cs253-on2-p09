// app/sim_main.cpp — bin/sim: command-line front end of the simulator. Owner: Architect.
//
//   sim [--queue heap|avl|oracle] [--events E] [--flows F] [--loss-ppm P] [--seed S]
//       [--rtt-min US] [--rtt-max US] [--rto US] [--idle US] [--monitor US] [--trace]
//
// stdout is canonical and identical for every correct queue; queue name, wall time and
// structure statistics go to stderr. Exit: 0 ok, 2 bad arguments, 3 check failed,
// 4 resource / time overflow / sequence exhaustion.
#include <chrono>
#include <cstdio>
#include <exception>
#include <new>
#include <stdexcept>
#include <string>

#include "avl_queue.h"
#include "lazy_heap.h"
#include "opstream.h"   // eq::detail::parse_number
#include "queue_factory.h"
#include "simulator.h"

static int usage(const char* why) {
    if (why) std::fprintf(stderr, "sim: %s\n", why);
    std::fprintf(stderr,
                 "usage: sim [--queue heap|avl|oracle] [--events E] [--flows F] [--loss-ppm P] [--seed S]\n"
                 "           [--rtt-min US] [--rtt-max US] [--rto US] [--idle US] [--monitor US] [--trace]\n");
    return 2;
}

int main(int argc, char** argv) {
    sim::Config c;
    std::string queue = "heap";
    bool trace = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--trace") { trace = true; continue; }
        if (a == "--help" || a == "-h") return usage(nullptr);
        if (i + 1 >= argc) return usage(("missing value after " + a).c_str());
        const std::string v = argv[++i];
        using eq::detail::parse_number;
        bool ok = true;
        if (a == "--queue") queue = v;
        else if (a == "--events") ok = parse_number(v, c.events);
        else if (a == "--flows") ok = parse_number(v, c.flows);
        else if (a == "--loss-ppm") ok = parse_number(v, c.loss_ppm);
        else if (a == "--seed") ok = parse_number(v, c.seed);
        else if (a == "--rtt-min") ok = parse_number(v, c.rtt_min_us);
        else if (a == "--rtt-max") ok = parse_number(v, c.rtt_max_us);
        else if (a == "--rto") ok = parse_number(v, c.rto_us);
        else if (a == "--idle") ok = parse_number(v, c.idle_us);
        else if (a == "--monitor") ok = parse_number(v, c.monitor_us);
        else return usage(("unknown option " + a).c_str());
        if (!ok) return usage(("bad number for " + a + ": " + v).c_str());
    }
    if (queue != "heap" && queue != "avl" && queue != "oracle") return usage("--queue must be heap, avl or oracle");
    const std::string why = sim::validate(c);
    if (!why.empty()) return usage(why.c_str());

    auto q = app::make_queue(queue);
    const auto t0 = std::chrono::steady_clock::now();
    try {
        std::string trace_text;
        const std::string head = sim::params_text(c);
        std::fwrite(head.data(), 1, head.size(), stdout);
        const sim::Result r = sim::run(*q, c, trace ? &trace_text : nullptr);
        std::fwrite(trace_text.data(), 1, trace_text.size(), stdout);
        const std::string tail = sim::summary_text(r);
        std::fwrite(tail.data(), 1, tail.size(), stdout);
    } catch (const sim::Error& e) {
        std::fflush(stdout);
        std::fprintf(stderr, "sim: FAILED: %s\n", e.what());
        return e.code;
    } catch (const std::bad_alloc&) {
        std::fprintf(stderr, "sim: FAILED: out of memory\n");
        return 4;
    } catch (const std::length_error& e) {
        std::fprintf(stderr, "sim: FAILED: %s\n", e.what());
        return 4;
    } catch (const std::overflow_error& e) {
        std::fprintf(stderr, "sim: FAILED: %s\n", e.what());
        return 4;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "sim: FAILED: %s\n", e.what());
        return 3;
    }
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    std::fprintf(stderr, "queue=%s wall_ms=%.1f", q->name(), ms);
    if (auto* h = dynamic_cast<structA::LazyHeap*>(q.get()))
        std::fprintf(stderr, " peak_physical=%zu purges=%llu", h->peak_physical(), (unsigned long long)h->purge_count());
    if (auto* a = dynamic_cast<structB::AvlQueue*>(q.get()))
        std::fprintf(stderr, " height=%d rebalances=%llu", a->height(),
                     (unsigned long long)(a->rebalance_single() + a->rebalance_double()));
    std::fprintf(stderr, "\n");
    return 0;
}
