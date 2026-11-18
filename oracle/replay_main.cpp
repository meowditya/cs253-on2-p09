// oracle/replay_main.cpp — bin/replay: run a .ops file on one queue (INTERFACE.md §7).
// Owner: Oracle owner.
//
//   replay --queue heap|avl|oracle|heap-nopurge [--check N] [--stats] file.ops
//
// stdout: one canonical line per operation, then "END size=… issued=…" (identical for every
// correct queue). --check N runs the invariant checker every N operations (exit 3 on violation).
// --stats prints structure statistics to stderr. Exit codes: 0 ok, 2 bad input, 3 check failed.
#include <cstdio>
#include <exception>
#include <new>
#include <stdexcept>
#include <cstring>
#include <exception>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "avl_queue.h"
#include "checkers.h"
#include "lazy_heap.h"
#include "opstream.h"
#include "queue_factory.h"
#include "replay_core.h"

static int usage() {
    std::fprintf(stderr, "usage: replay --queue heap|avl|oracle|heap-nopurge [--check N] [--stats] file.ops\n");
    return 2;
}

static int run_main(int argc, char** argv) {
    std::string queue = "oracle", file;
    std::size_t check_every = 0;
    bool stats = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--queue" && i + 1 < argc) queue = argv[++i];
        else if (a == "--check" && i + 1 < argc) {
            std::uint64_t n = 0;
            if (!eq::detail::parse_number(std::string(argv[++i]), n) || n == 0) return usage();
            check_every = static_cast<std::size_t>(n);
        } else if (a == "--stats") stats = true;
        else if (!a.empty() && a[0] != '-' && file.empty()) file = a;
        else return usage();
    }
    if (file.empty()) return usage();
    auto q = app::make_queue(queue);
    if (!q) { std::fprintf(stderr, "replay: unknown queue '%s'\n", queue.c_str()); return 2; }

    std::ifstream in(file);
    if (!in) { std::fprintf(stderr, "replay: cannot open %s\n", file.c_str()); return 2; }
    std::vector<eq::Op> ops;
    std::string err;
    if (!eq::read_ops(in, file, ops, err)) { std::fprintf(stderr, "replay: %s\n", err.c_str()); return 2; }

    try {
        replay::State st;
        std::string out;
        out.reserve(ops.size() * 24);
        for (std::size_t i = 0; i < ops.size(); ++i) {
            out += replay::apply(*q, ops[i], st);
            out += '\n';
            if (check_every && (i + 1) % check_every == 0) {
                const std::string msg = checkers::check_queue(*q);
                if (!msg.empty()) { std::fprintf(stderr, "replay: invariant violated after op %zu: %s\n", i, msg.c_str()); return 3; }
            }
        }
        if (check_every) {
            const std::string msg = checkers::check_queue(*q);
            if (!msg.empty()) { std::fprintf(stderr, "replay: invariant violated at end: %s\n", msg.c_str()); return 3; }
        }
        out += replay::end_line(*q);
        out += '\n';
        std::fwrite(out.data(), 1, out.size(), stdout);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "replay: %s\n", e.what());
        return 4;
    }
    if (stats) {
        if (auto* h = dynamic_cast<structA::LazyHeap*>(q.get())) {
            std::fprintf(stderr, "stats queue=%s live=%zu physical=%zu peak_physical=%zu purges=%llu purged=%llu stale_discards=%llu bytes=%zu\n",
                         h->name(), h->live(), h->physical(), h->peak_physical(),
                         (unsigned long long)h->purge_count(), (unsigned long long)h->purged_entries(),
                         (unsigned long long)h->stale_roots_discarded(), h->allocated_bytes());
        } else if (auto* a = dynamic_cast<structB::AvlQueue*>(q.get())) {
            std::fprintf(stderr, "stats queue=avl size=%zu height=%d rebalance_single=%llu rebalance_double=%llu bytes=%zu\n",
                         a->size(), a->height(), (unsigned long long)a->rebalance_single(),
                         (unsigned long long)a->rebalance_double(), a->allocated_bytes());
        } else {
            std::fprintf(stderr, "stats queue=%s size=%zu\n", q->name(), q->size());
        }
    }
    return 0;
}

int main(int argc, char** argv) {
    try {
        return run_main(argc, argv);
    } catch (const std::bad_alloc&) {
        std::fprintf(stderr, "%s: out of memory\n", argv[0]);
        return 4;
    } catch (const std::length_error& e) {
        std::fprintf(stderr, "%s: %s\n", argv[0], e.what());
        return 4;
    } catch (const std::overflow_error& e) {
        std::fprintf(stderr, "%s: %s\n", argv[0], e.what());
        return 4;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s: %s\n", argv[0], e.what());
        return 3;
    }
}
