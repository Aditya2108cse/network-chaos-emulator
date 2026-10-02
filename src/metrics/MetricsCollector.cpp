#include "MetricsCollector.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>

namespace chaos {

namespace {
constexpr size_t kRingCapacity = 100000; // bounds memory under sustained load

long percentile(const std::vector<long>& sorted, double pct) {
    if (sorted.empty()) return 0;
    return sorted[static_cast<size_t>(pct * (sorted.size() - 1))];
}
} // namespace

void MetricsCollector::recordIn(size_t bytes, bool matched) {
    packets_in_.fetch_add(1, std::memory_order_relaxed);
    bytes_in_.fetch_add(bytes, std::memory_order_relaxed);
    if (matched) matched_in_.fetch_add(1, std::memory_order_relaxed);
}

void MetricsCollector::recordOut(size_t bytes, bool matched) {
    packets_out_.fetch_add(1, std::memory_order_relaxed);
    bytes_out_.fetch_add(bytes, std::memory_order_relaxed);
    if (matched) {
        matched_out_.fetch_add(1, std::memory_order_relaxed);
        matched_bytes_out_.fetch_add(bytes, std::memory_order_relaxed);
    }
}

void MetricsCollector::recordLossDrop() { dropped_loss_.fetch_add(1, std::memory_order_relaxed); }
void MetricsCollector::recordShaperDrop() { dropped_shaper_.fetch_add(1, std::memory_order_relaxed); }
void MetricsCollector::recordCorrupted() { corrupted_.fetch_add(1, std::memory_order_relaxed); }

void MetricsCollector::recordLatencyMicros(long us) {
    std::lock_guard<std::mutex> lock(latency_mutex_);
    if (lat_count_ == 0) {
        lat_min_ = lat_max_ = us;
    } else {
        lat_min_ = std::min(lat_min_, us);
        lat_max_ = std::max(lat_max_, us);
    }
    // Welford's online mean/variance.
    ++lat_count_;
    double delta = static_cast<double>(us) - lat_mean_;
    lat_mean_ += delta / static_cast<double>(lat_count_);
    lat_m2_ += delta * (static_cast<double>(us) - lat_mean_);

    if (ring_.size() < kRingCapacity) {
        ring_.push_back(us);
    } else {
        ring_[ring_next_] = us;
        ring_next_ = (ring_next_ + 1) % kRingCapacity;
    }
}

void MetricsCollector::resetLatency() {
    std::lock_guard<std::mutex> lock(latency_mutex_);
    lat_count_ = 0;
    lat_mean_ = lat_m2_ = 0.0;
    lat_min_ = lat_max_ = 0;
    ring_.clear();
    ring_next_ = 0;
}

MetricsCollector::Snapshot MetricsCollector::snapshot() const {
    Snapshot s;
    s.packets_in = packets_in_.load(std::memory_order_relaxed);
    s.packets_out = packets_out_.load(std::memory_order_relaxed);
    s.bytes_in = bytes_in_.load(std::memory_order_relaxed);
    s.bytes_out = bytes_out_.load(std::memory_order_relaxed);
    s.matched_in = matched_in_.load(std::memory_order_relaxed);
    s.matched_out = matched_out_.load(std::memory_order_relaxed);
    s.matched_bytes_out = matched_bytes_out_.load(std::memory_order_relaxed);
    s.dropped_loss = dropped_loss_.load(std::memory_order_relaxed);
    s.dropped_shaper = dropped_shaper_.load(std::memory_order_relaxed);
    s.corrupted = corrupted_.load(std::memory_order_relaxed);

    std::vector<long> samples;
    {
        std::lock_guard<std::mutex> lock(latency_mutex_);
        s.latency.count = lat_count_;
        s.latency.min_us = lat_min_;
        s.latency.max_us = lat_max_;
        s.latency.mean_us = lat_mean_;
        s.latency.stddev_us = lat_count_ > 1
            ? std::sqrt(lat_m2_ / static_cast<double>(lat_count_ - 1)) : 0.0;
        samples = ring_;
    }
    std::sort(samples.begin(), samples.end());
    s.latency.p50_us = percentile(samples, 0.50);
    s.latency.p95_us = percentile(samples, 0.95);
    s.latency.p99_us = percentile(samples, 0.99);
    return s;
}

void MetricsCollector::printReport() const {
    Snapshot s = snapshot();
    uint64_t lost = s.dropped_total();
    double loss_pct = s.matched_in ? 100.0 * lost / s.matched_in : 0.0;
    std::cout << "[live] in=" << s.packets_in << " out=" << s.packets_out
              << " matched=" << s.matched_in << " dropped=" << lost
              << " (loss-model=" << s.dropped_loss << ", shaper=" << s.dropped_shaper << ")"
              << " loss=" << loss_pct << "%"
              << " latency avg/p95/p99=" << s.latency.mean_us / 1000.0 << "/"
              << s.latency.p95_us / 1000.0 << "/" << s.latency.p99_us / 1000.0 << " ms\n";
}

std::string MetricsCollector::toPrometheusText() const {
    Snapshot s = snapshot();
    std::ostringstream out;
    out << "# TYPE chaos_packets_in counter\nchaos_packets_in " << s.packets_in << "\n"
        << "# TYPE chaos_packets_out counter\nchaos_packets_out " << s.packets_out << "\n"
        << "# TYPE chaos_packets_dropped_loss counter\nchaos_packets_dropped_loss " << s.dropped_loss << "\n"
        << "# TYPE chaos_packets_dropped_shaper counter\nchaos_packets_dropped_shaper " << s.dropped_shaper << "\n"
        << "# TYPE chaos_latency_microseconds_p99 gauge\nchaos_latency_microseconds_p99 " << s.latency.p99_us << "\n";
    return out.str();
}

} // namespace chaos
