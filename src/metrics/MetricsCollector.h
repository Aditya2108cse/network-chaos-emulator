#pragma once
#include <atomic>
#include <cstdint>
#include <vector>
#include <mutex>
#include <string>

namespace chaos {

// Latency statistics for packets that traversed the emulator.
// count/min/max/mean/stddev are exact (Welford's online algorithm);
// percentiles come from a bounded ring buffer of recent samples.
struct LatencyStats {
    uint64_t count = 0;
    long min_us = 0;
    long max_us = 0;
    double mean_us = 0.0;
    double stddev_us = 0.0; // reported as "jitter"
    long p50_us = 0;
    long p95_us = 0;
    long p99_us = 0;
};

// Counters use std::atomic so capture/scheduler threads update them
// without locks; latency stats use a small mutex.
class MetricsCollector {
public:
    // "matched" = packet matched the experiment's protocol filter.
    void recordIn(size_t bytes, bool matched);
    void recordOut(size_t bytes, bool matched);
    void recordLossDrop();     // dropped by the loss model
    void recordShaperDrop();   // dropped because the bandwidth queue was full
    void recordCorrupted();
    void recordLatencyMicros(long us);

    // Clears latency statistics (called at the start of each phase).
    void resetLatency();

    struct Snapshot {
        uint64_t packets_in = 0, packets_out = 0;
        uint64_t bytes_in = 0, bytes_out = 0;
        uint64_t matched_in = 0, matched_out = 0;
        uint64_t matched_bytes_out = 0;
        uint64_t dropped_loss = 0, dropped_shaper = 0;
        uint64_t corrupted = 0;
        LatencyStats latency;

        uint64_t dropped_total() const { return dropped_loss + dropped_shaper; }
    };

    Snapshot snapshot() const;

    void printReport() const;               // compact live status line block
    std::string toPrometheusText() const;   // Prometheus text exposition

private:
    std::atomic<uint64_t> packets_in_{0}, packets_out_{0};
    std::atomic<uint64_t> bytes_in_{0}, bytes_out_{0};
    std::atomic<uint64_t> matched_in_{0}, matched_out_{0};
    std::atomic<uint64_t> matched_bytes_out_{0};
    std::atomic<uint64_t> dropped_loss_{0}, dropped_shaper_{0};
    std::atomic<uint64_t> corrupted_{0};

    mutable std::mutex latency_mutex_;
    uint64_t lat_count_ = 0;
    double lat_mean_ = 0.0;
    double lat_m2_ = 0.0;
    long lat_min_ = 0, lat_max_ = 0;
    std::vector<long> ring_;
    size_t ring_next_ = 0;
};

} // namespace chaos
