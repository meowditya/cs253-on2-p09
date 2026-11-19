// bench/bench_common.h — shared helpers for the benchmarks. Owner: Analyst.
#pragma once
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <limits>
#include <new>
#include <sys/stat.h>

// Cachegrind region control is optional: the header may be missing (macOS) or too old to have the
// region macros (Valgrind < 3.22). Enable it only when both macros really exist, so an incompatible
// header can never break the ordinary build. `bench_hold --profile-support` reports the result.
#if defined(__has_include)
#  if __has_include(<valgrind/cachegrind.h>)
#    include <valgrind/cachegrind.h>
#    if defined(CACHEGRIND_START_INSTRUMENTATION) && defined(CACHEGRIND_STOP_INSTRUMENTATION)
#      define EQ_HAVE_CACHEGRIND 1
#    endif
#  endif
#endif

#include "avl_queue.h"
#include "event_queue.h"
#include "lazy_heap.h"

namespace bench {

inline std::uint64_t now_ns() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

inline const char* build_kind() { return EQ_COUNTING ? "count" : "release"; }

// Profiler region control, so that ONLY the timed loop is profiled (scripts/profile.sh):
//  * cachegrind: run under `valgrind --tool=cachegrind --instr-at-start=no`; start()/stop() switch
//    instrumentation on and off (Valgrind >= 3.22 client requests; no-ops when not under Valgrind or
//    when the header is absent, e.g. on macOS).
//  * perf: run under `perf stat --control fifo:CTL,ACK --delay=-1` with EQ_PERF_CTL / EQ_PERF_ACK set
//    to the same FIFO paths; start()/stop() send "enable" / "disable" and wait for perf's ack.
// Both are outside the steady_clock measurement's critical loop and cost nothing in normal runs.
// When EQ_PERF_CTL is set, every enable/disable must be written AND acknowledged by perf ("ack");
// otherwise ok() is false and the benchmark exits 3, so a profile with an unknown scope is never used.
class ProfileRegion {
public:
    void start() {
#ifdef EQ_HAVE_CACHEGRIND
        CACHEGRIND_START_INSTRUMENTATION;
#endif
        ok_ = perf_command("enable") && ok_;
    }
    void stop() {
        ok_ = perf_command("disable") && ok_;
#ifdef EQ_HAVE_CACHEGRIND
        CACHEGRIND_STOP_INSTRUMENTATION;
#endif
    }
    bool ok() const { return ok_; }
    static bool cachegrind_regions() {
#ifdef EQ_HAVE_CACHEGRIND
        return true;
#else
        return false;
#endif
    }

private:
    bool ok_ = true;
    static bool perf_command(const char* cmd) {               // true if not under perf, or acknowledged
        const char* ctl = std::getenv("EQ_PERF_CTL");
        if (!ctl || !*ctl) return true;
        const char* ack = std::getenv("EQ_PERF_ACK");
        if (!ack || !*ack) { std::fprintf(stderr, "profiler: EQ_PERF_CTL set without EQ_PERF_ACK\n"); return false; }
        std::FILE* f = std::fopen(ctl, "w");
        if (!f) { std::fprintf(stderr, "profiler: cannot open %s\n", ctl); return false; }
        const bool wrote = std::fprintf(f, "%s\n", cmd) > 0;
        const bool closed = std::fclose(f) == 0;
        if (!wrote || !closed) { std::fprintf(stderr, "profiler: cannot send '%s'\n", cmd); return false; }
        std::FILE* a = std::fopen(ack, "r");
        if (!a) { std::fprintf(stderr, "profiler: cannot open %s\n", ack); return false; }
        char buf[16] = {0};
        const bool got = std::fgets(buf, sizeof buf, a) != nullptr && std::strncmp(buf, "ack", 3) == 0;
        std::fclose(a);
        if (!got) std::fprintf(stderr, "profiler: perf did not acknowledge '%s'\n", cmd);
        return got;
    }
};

// `--profile-support`: what this binary can do for scripts/profile.sh.
inline int print_profile_support() {
    std::printf("cachegrind_regions=%s\nperf_control=yes\n", ProfileRegion::cachegrind_regions() ? "yes" : "no");
    return 0;
}

// t + d with an int64 overflow check (exit code 4, the documented resource/overflow status).
inline std::int64_t checked_add(std::int64_t t, std::int64_t d, const char* who) {
    if ((d > 0 && t > std::numeric_limits<std::int64_t>::max() - d) ||
        (d < 0 && t < std::numeric_limits<std::int64_t>::min() - d)) {
        std::fprintf(stderr, "%s: simulated time would overflow int64\n", who);
        std::exit(4);
    }
    return t + d;
}

// Runs a tool's real main; maps resource failures to exit 4 and any other exception to exit 3.
// A result row that could not be written to stdout (e.g. a full disk) is also exit 4.
template <class F>
int guarded_main(const char* who, F body) {
    try {
        const int st = body();
        if (std::fflush(stdout) != 0 || std::ferror(stdout)) {
            std::fprintf(stderr, "%s: cannot write the result to stdout\n", who);
            return 4;
        }
        return st;
    } catch (const std::bad_alloc&) {
        std::fprintf(stderr, "%s: out of memory\n", who);
        return 4;
    } catch (const std::length_error& e) {
        std::fprintf(stderr, "%s: %s\n", who, e.what());
        return 4;
    } catch (const std::overflow_error& e) {
        std::fprintf(stderr, "%s: %s\n", who, e.what());
        return 4;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s: %s\n", who, e.what());
        return 3;
    }
}

// Appends one CSV row; writes the header first if the file is new or empty. Every write AND the
// final flush (fclose) is checked: buffered data can fail only at fclose, e.g. on a full disk.
inline bool append_csv(const std::string& path, const std::string& header, const std::string& row) {
    struct stat st{};
    const bool fresh = stat(path.c_str(), &st) != 0 || st.st_size == 0;
    std::FILE* f = std::fopen(path.c_str(), "a");
    if (!f) return false;
    bool ok = true;
    if (fresh && std::fprintf(f, "%s\n", header.c_str()) < 0) ok = false;
    if (std::fprintf(f, "%s\n", row.c_str()) < 0) ok = false;
    if (std::fclose(f) != 0) ok = false;
    return ok;
}

// The --csv failure status: 4 (resource), with a message.
inline int csv_failed(const char* who, const std::string& path) {
    std::fprintf(stderr, "%s: could not write %s (nothing usable was recorded)\n", who, path.c_str());
    return 4;
}

// Structure-specific statistics that are cheap enough to keep in timing builds.
struct Stats {
    std::uint64_t peak_physical = 0, purges = 0, purged = 0, stale = 0;
    std::uint64_t table_rebuilds = 0, table_rebuild_slots = 0;
    std::uint64_t reb_single = 0, reb_double = 0, bytes = 0;
};
inline Stats stats_of(const eq::EventQueue& q) {
    Stats s;
    if (auto* h = dynamic_cast<const structA::LazyHeap*>(&q)) {
        s.peak_physical = h->peak_physical();
        s.purges = h->purge_count();
        s.purged = h->purged_entries();
        s.stale = h->stale_roots_discarded();
        s.table_rebuilds = h->table_rebuilds();
        s.table_rebuild_slots = h->table_rebuild_slots();
        s.bytes = h->allocated_bytes();
    } else if (auto* a = dynamic_cast<const structB::AvlQueue*>(&q)) {
        s.reb_single = a->rebalance_single();
        s.reb_double = a->rebalance_double();
        s.bytes = a->allocated_bytes();
    }
    return s;
}

inline std::string u(std::uint64_t v) { return std::to_string(v); }

}  // namespace bench
