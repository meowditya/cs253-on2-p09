// oracle/difftest_core.cpp — see difftest_core.h. Owner: Oracle owner.
// Library containers (std::set, std::unordered_map) are used here only to GENERATE inputs, which
// the handout allows; the structures under test never use them.
#include "difftest_core.h"

#include <algorithm>
#include <exception>
#include <limits>
#include <set>
#include <unordered_map>
#include <utility>

#include "checkers.h"
#include "replay_core.h"
#include "rng.h"

namespace difftest {

using eq::Key;
using eq::Op;
using eq::Time;

namespace {
constexpr Time kNormal = Time{1} << 60;   // "now" only follows popped times inside +-2^60
}

std::vector<Op> generate(const GenConfig& cfg) {
    eq::Rng r(cfg.seed);
    std::vector<Op> ops;
    ops.reserve(cfg.ops);
    std::vector<Key> issued;                               // key of the k-th insert
    std::set<std::pair<Key, std::uint64_t>> live;          // reference pending set
    std::vector<std::uint64_t> live_ks;                    // for uniform random live picks
    std::unordered_map<std::uint64_t, std::size_t> pos;
    Time now = 0;

    auto add_live = [&](std::uint64_t k) {
        live.insert({issued[k], k});
        pos[k] = live_ks.size();
        live_ks.push_back(k);
    };
    auto remove_live = [&](std::uint64_t k) {
        auto it = pos.find(k);
        if (it == pos.end()) return;
        const std::size_t p = it->second;
        live_ks[p] = live_ks.back();
        pos[live_ks[p]] = p;
        live_ks.pop_back();
        pos.erase(k);
        live.erase({issued[k], k});
    };
    auto push_insert = [&](Time t) {
        Op op;
        op.kind = 'I';
        op.time = t;
        op.type = static_cast<std::uint32_t>(r.below(4));
        op.data = static_cast<std::uint32_t>(r.below(1u << 20));
        ops.push_back(op);
        issued.push_back(Key{t, issued.size()});
        add_live(issued.size() - 1);
    };
    auto push_cancel_k = [&](std::uint64_t k) {
        Op op;
        op.kind = 'C';
        op.k = k;
        ops.push_back(op);
        if (k < issued.size()) remove_live(k);
    };
    auto push_pop = [&]() {
        ops.push_back(Op{'P', 0, 0, 0, 0, 0});
        if (live.empty()) return;
        const auto first = *live.begin();
        remove_live(first.second);
        if (first.first.time > now && first.first.time < kNormal) now = first.first.time;
    };

    while (ops.size() < cfg.ops) {
        if (live.empty() && r.below(4) != 0) { push_insert(now + static_cast<Time>(r.below(2000))); continue; }
        if (live.size() >= cfg.max_live) {                 // enforce the cap
            if (r.below(10) < 6) push_pop();
            else push_cancel_k(live_ks[static_cast<std::size_t>(r.below(live_ks.size()))]);
            continue;
        }
        const std::uint64_t x = r.below(1000);
        if (x < 380) {                                     // insert
            const std::uint64_t y = r.below(100);
            Time t;
            if (y < 85) t = now + static_cast<Time>(r.below(2000));
            else if (y < 95) t = now - static_cast<Time>(r.below(50));                  // in the past
            else if (y < 98) t = now + static_cast<Time>(r.below(1000000000));          // far future
            else t = (r.below(2) ? std::numeric_limits<Time>::max() - static_cast<Time>(r.below(3))
                                 : std::numeric_limits<Time>::min() + static_cast<Time>(r.below(3)));
            push_insert(t);
        } else if (x < 480) {                              // cancel a live handle
            if (!live_ks.empty()) push_cancel_k(live_ks[static_cast<std::size_t>(r.below(live_ks.size()))]);
        } else if (x < 560) {                              // cancel any issued handle (often stale)
            if (!issued.empty()) push_cancel_k(r.below(issued.size()));
        } else if (x < 570) {                              // never-issued insert index
            push_cancel_k(issued.size() + r.below(5));
        } else if (x < 585) {                              // forged: right seq, guaranteed wrong time
            if (!issued.empty()) {
                const std::uint64_t k = r.below(issued.size());
                const Time t0 = issued[k].time;
                Op op;
                op.kind = 'F';
                op.seq = k;
                op.time = (t0 == std::numeric_limits<Time>::max()) ? t0 - 1 : t0 + 1;
                ops.push_back(op);
            }
        } else if (x < 590) {                              // forged: reserved seqs
            Op op;
            op.kind = 'F';
            op.time = r.below(2) ? 0 : now;
            op.seq = std::numeric_limits<eq::Seq>::max() - r.below(2);
            ops.push_back(op);
        } else if (x < 593 && !live_ks.empty()) {          // a "forged" value exactly equal to a
            const std::uint64_t k = live_ks[static_cast<std::size_t>(r.below(live_ks.size()))];   // pending key:
            Op op;                                          // handles are values, so this cancels it
            op.kind = 'F';
            op.time = issued[k].time;
            op.seq = k;
            ops.push_back(op);
            remove_live(k);
        } else if (x < 595) {                              // forged: never-issued seq
            Op op;
            op.kind = 'F';
            op.time = now + static_cast<Time>(r.below(100));
            op.seq = issued.size() + r.below(10);
            ops.push_back(op);
        } else if (x < 810) {
            push_pop();
        } else if (x < 870) {
            ops.push_back(Op{'K', 0, 0, 0, 0, 0});
        } else {                                           // successor
            const std::uint64_t y = r.below(100);
            Op op;
            op.kind = 'S';
            if (y < 80) op.time = now + static_cast<Time>(r.below(3000)) - 500;
            else if (y < 90 && !issued.empty()) op.time = issued[static_cast<std::size_t>(r.below(issued.size()))].time;  // exact boundary
            else if (y < 95) op.time = std::numeric_limits<Time>::max();
            else op.time = std::numeric_limits<Time>::min();
            ops.push_back(op);
        }
    }
    ops.resize(cfg.ops);
    return ops;
}

Failure run(const std::vector<Op>& ops, const std::vector<Factory>& factories,
            std::size_t check_every, std::size_t check_all_first) {
    Failure f;
    std::vector<std::unique_ptr<eq::EventQueue>> qs;
    std::vector<replay::State> st(factories.size());
    for (const auto& make : factories) qs.push_back(make());
    auto fail = [&](std::size_t i, const char* cat, std::string d) {
        f.failed = true; f.index = i; f.category = cat; f.detail = std::move(d);
        return f;
    };
    auto check_all = [&](std::size_t i) -> bool {
        for (auto& q : qs) {
            const std::string msg = checkers::check_queue(*q);
            if (!msg.empty()) { fail(i, "CHECKER", std::string(q->name()) + ": " + msg); return false; }
        }
        return true;
    };
    std::size_t cur = 0;
    try {
        for (std::size_t i = 0; i < ops.size(); ++i) {
            cur = i;
            const std::string ref = replay::apply(*qs[0], ops[i], st[0]);
            for (std::size_t j = 1; j < qs.size(); ++j) {
                const std::string got = replay::apply(*qs[j], ops[i], st[j]);
                if (got != ref || qs[j]->size() != qs[0]->size() || qs[j]->issued() != qs[0]->issued()) {
                    return fail(i, "MISMATCH", "op " + std::to_string(i) + " '" + eq::op_text(ops[i]) + "': " +
                                qs[0]->name() + " -> '" + ref + "' size " + std::to_string(qs[0]->size()) + ", " +
                                qs[j]->name() + " -> '" + got + "' size " + std::to_string(qs[j]->size()));
                }
            }
            const bool due = i < check_all_first || (check_every != 0 && (i + 1) % check_every == 0);
            if (due && !check_all(i)) return f;
        }
        if (!check_all(ops.empty() ? 0 : ops.size() - 1)) return f;
    } catch (const std::exception& e) {
        return fail(cur, "EXCEPTION", e.what());
    }
    return f;
}

ShrinkResult shrink(std::vector<Op> ops, const std::vector<Factory>& factories,
                    const std::string& category, std::size_t max_replays) {
    std::size_t replays = 0;
    bool one_minimal = false;
    auto fails = [&](const std::vector<Op>& c) {
        ++replays;
        const std::size_t every = c.size() <= 4000 ? 1 : 64;   // dense checks once small
        const Failure f = run(c, factories, every, every == 1 ? c.size() : 0);
        return f.failed && f.category == category;
    };
    std::size_t chunk = ops.size() / 2;
    if (chunk == 0) chunk = 1;
    while (chunk >= 1 && replays < max_replays) {
        bool changed = false;
        std::size_t i = 0;
        while (i < ops.size() && replays < max_replays) {
            std::vector<Op> cand;
            cand.reserve(ops.size());
            cand.insert(cand.end(), ops.begin(), ops.begin() + static_cast<std::ptrdiff_t>(i));
            const std::size_t end = std::min(ops.size(), i + chunk);
            cand.insert(cand.end(), ops.begin() + static_cast<std::ptrdiff_t>(end), ops.end());
            if (!cand.empty() && fails(cand)) { ops.swap(cand); changed = true; }
            else i += chunk;
        }
        if (!changed && replays < max_replays) {
            if (chunk == 1) { one_minimal = true; break; }   // a COMPLETE single-deletion pass removed nothing
            chunk /= 2;
        }
    }
    return ShrinkResult{ops, one_minimal, replays};
}

}  // namespace difftest
