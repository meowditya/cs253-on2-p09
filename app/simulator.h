// app/simulator.h — the application: a discrete-event simulation of stop-and-wait senders with
// retransmission timers, runnable on any EventQueue. Owner: Architect. Derivation: D3.
//
// Model. F independent flows, each with exactly one packet outstanding. A transmission is lost
// with probability loss_ppm / 10^6 (packet or its ACK). If it gets through, an ACK arrives after a
// uniform round-trip time in [rtt_min, rtt_max]. Every transmission arms a retransmission timer
// (RTO); because rto > rtt_max, an ACK always arrives before its own timer and cancels it. Each
// flow also has an ACK-inactivity watchdog ("IDLE") that is cancelled and re-armed on every ACK.
// An optional MONITOR event asks the queue for successor(now + rto) every `monitor` microseconds.
//
// Events: ACK(f), TIMEOUT(f), IDLE(f), MONITOR. The run performs exactly `events` dispatches
// after start-up; events still pending at the end are reported, not drained.
// Everything is integer arithmetic, so the output is identical on every correct queue (D3).
#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>

#include "event_queue.h"

namespace sim {

enum EventType : std::uint32_t { ACK = 0, TIMEOUT = 1, IDLE = 2, MONITOR = 3 };

struct Config {
    std::uint64_t events   = 1000000;   // successful dispatches after start-up
    std::uint64_t flows    = 20000;     // 1 .. 2^32 - 1
    std::uint64_t loss_ppm = 50000;     // 0 .. 1,000,000 (parts per million)
    std::uint64_t seed     = 1;
    eq::Time rtt_min_us    = 10000;     // > 0
    eq::Time rtt_max_us    = 50000;     // >= rtt_min
    eq::Time rto_us        = 200000;    // > rtt_max, so an ACK always beats its own timer
    eq::Time idle_us       = 5000000;   // > 0
    eq::Time monitor_us    = 100000;    // 0 disables the monitor
};

struct Result {
    std::uint64_t inserted = 0, processed = 0, pending = 0;
    std::uint64_t rto_cancelled = 0, idle_cancelled = 0;
    std::uint64_t transmissions = 0, retransmissions = 0, delivered = 0, idle_fired = 0, monitors = 0;
    std::uint64_t ties = 0, peak_live = 0;
    eq::Time end_time_us = 0;
    std::uint64_t checksum = 0;          // FNV-1a over every dispatched event (canonical bytes)
    std::uint64_t monitor_hash = 0;      // FNV-1a over every monitor query and its answer
    std::uint64_t cancelled() const { return rto_cancelled + idle_cancelled; }
};

// A simulation-level failure; code is the process exit code (3 = check failed, 4 = resource/overflow).
struct Error : std::runtime_error {
    int code;
    Error(int c, const std::string& what) : std::runtime_error(what), code(c) {}
};

// "" if valid, otherwise the reason (exit code 2).
std::string validate(const Config& c);

// Runs the simulation on an empty queue. If trace is non-null, one line per dispatched event
// ("E <time> <seq> <type> <data>") and per monitor query ("M <threshold> 0" or
// "M <threshold> 1 <time> <seq> <type> <data>") is appended to it. Throws sim::Error.
Result run(eq::EventQueue& q, const Config& c, std::string* trace);

std::string params_text(const Config& c);    // "format=..." and "params ..." lines
std::string summary_text(const Result& r);   // the remaining canonical lines

}  // namespace sim
