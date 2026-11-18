// oracle/difftest_main.cpp — bin/difftest: the differential test (one seeded history).
// Owner: Oracle owner.
//
//   difftest [--seed S] [--ops N] [--max-live L] [--check-every K] [--save-dir DIR]
//
// Runs the same history on oracle (reference), heap and AVL in lockstep. On the first
// disagreement or invariant violation it shrinks the history and saves it as a literal .ops
// file (DIR/seed<S>.ops) that bin/replay reproduces. Exit 0 = pass, 3 = failure, 2 = bad args.
#include <chrono>
#include <cstdio>
#include <exception>
#include <new>
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "difftest_core.h"
#include "queue_factory.h"

static int run_main(int argc, char** argv) {
    difftest::GenConfig cfg;
    std::size_t check_every = 1000;
    std::string save_dir = "failures";
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        std::uint64_t v = 0;
        auto num = [&](std::uint64_t& out) { return i + 1 < argc && eq::detail::parse_number(std::string(argv[++i]), out); };
        if (a == "--seed") { if (!num(v)) return 2; cfg.seed = v; }
        else if (a == "--ops") { if (!num(v)) return 2; cfg.ops = v; }
        else if (a == "--max-live") { if (!num(v) || v == 0) return 2; cfg.max_live = v; }
        else if (a == "--check-every") { if (!num(v)) return 2; check_every = v; }
        else if (a == "--save-dir" && i + 1 < argc) save_dir = argv[++i];
        else { std::fprintf(stderr, "usage: difftest [--seed S] [--ops N] [--max-live L] [--check-every K] [--save-dir DIR]\n"); return 2; }
    }
    const std::vector<difftest::Factory> qs = {
        [] { return app::make_queue("oracle"); },
        [] { return app::make_queue("heap"); },
        [] { return app::make_queue("avl"); },
    };
    const auto t0 = std::chrono::steady_clock::now();
    const auto ops = difftest::generate(cfg);
    const auto f = difftest::run(ops, qs, check_every, 10000);
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    if (!f.failed) {
        std::printf("difftest seed=%llu ops=%zu max_live=%zu: PASS (oracle = heap = avl on every result; checkers passed)\n",
                    (unsigned long long)cfg.seed, ops.size(), cfg.max_live);
        std::fprintf(stderr, "difftest wall_s=%.2f\n", secs);
        return 0;
    }
    std::printf("difftest seed=%llu: FAIL [%s] at op %zu: %s\n", (unsigned long long)cfg.seed,
                f.category.c_str(), f.index, f.detail.c_str());
    std::vector<eq::Op> prefix(ops.begin(), ops.begin() + static_cast<std::ptrdiff_t>(f.index + 1));
    const auto shrunk = difftest::shrink(prefix, qs, f.category);
    const auto& small = shrunk.ops;
    const std::size_t replays = shrunk.replays;
    const char* status = shrunk.one_minimal ? "1-minimal" : "replay budget exhausted, not proven minimal";
    std::error_code ec;
    std::filesystem::create_directories(save_dir, ec);
    std::string path = save_dir + "/seed" + std::to_string(cfg.seed) + ".ops";
    std::ofstream out(path);
    if (out) {
        out << "# difftest seed " << cfg.seed << " category " << f.category << ", shrunk from "
            << prefix.size() << " to " << small.size() << " ops (" << replays << " replays, " << status << ")\n";
        eq::write_ops(out, small);
        std::printf("shrunk to %zu ops (%s), saved %s (reproduce: replay --queue <name> %s)\n", small.size(), status, path.c_str(), path.c_str());
    } else {
        std::printf("shrunk to %zu ops (could not write %s; create the directory)\n", small.size(), path.c_str());
        eq::write_ops(std::cout, small);
    }
    return 3;
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
