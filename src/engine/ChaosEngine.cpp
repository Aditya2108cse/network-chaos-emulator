#include "ChaosEngine.h"
#include <algorithm>
#include <iostream>

namespace chaos {

ChaosEngine::ChaosEngine(IPacketIO& io,
                         std::unique_ptr<ILossModel> loss_model,
                         std::unique_ptr<IDelayModel> delay_model,
                         MetricsCollector& metrics,
                         Options options)
    : io_(io),
      loss_model_(std::move(loss_model)),
      delay_model_(std::move(delay_model)),
      metrics_(metrics),
      options_(options) {
    if (options_.bandwidth_mbps > 0.0) {
        double rate_bytes = options_.bandwidth_mbps * 1e6 / 8.0;
        // Burst = 10 ms worth of traffic (at least two full-size packets):
        // small enough that the cap bites quickly, big enough to absorb
        // packet-sized granularity.
        double burst = std::max(rate_bytes * 0.010, 3000.0);
        bucket_.emplace(rate_bytes, burst);
    }
}

ChaosEngine::~ChaosEngine() { stop(); }

void ChaosEngine::start() {
    if (started_.exchange(true)) return;
    capturing_.store(true, std::memory_order_relaxed);
    capture_thread_ = std::thread(&ChaosEngine::captureLoop, this);
    scheduler_thread_ = std::thread(&ChaosEngine::schedulerLoop, this);
}

void ChaosEngine::stop() {
    if (!started_.exchange(false)) return;

    // 1. Stop reading new packets.
    capturing_.store(false, std::memory_order_relaxed);
    if (capture_thread_.joinable()) capture_thread_.join();

    // 2. Drain: give packets already in flight time to be released.
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (queue_.size() > 0 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    // 3. Stop the scheduler.
    queue_.stop();
    if (scheduler_thread_.joinable()) scheduler_thread_.join();
}

void ChaosEngine::captureLoop() {
    std::vector<uint8_t> rx(65536); // one reusable receive buffer

    while (capturing_.load(std::memory_order_relaxed)) {
        // poll() with a timeout so we notice shutdown promptly.
        if (!io_.waitReadable(100)) continue;

        ssize_t n = io_.readPacket(rx.data(), rx.size());
        if (n <= 0) continue;

        auto now = std::chrono::steady_clock::now();
        bool matched = PacketParser::matchesProtocol(options_.protocol, rx.data(), static_cast<size_t>(n));
        metrics_.recordIn(static_cast<size_t>(n), matched);

        Packet pkt;
        pkt.data.assign(rx.begin(), rx.begin() + n);
        pkt.captured_at = now;
        pkt.matched = matched;

        // Not targeted, or baseline phase: forward untouched.
        if (!matched || !chaos_enabled_.load(std::memory_order_relaxed)) {
            pkt.release_at = now;
            queue_.push(std::move(pkt));
            continue;
        }

        // 1. Loss model.
        if (loss_model_ && loss_model_->shouldDrop()) {
            metrics_.recordLossDrop();
            continue;
        }

        // 2. Bandwidth shaper: when may this packet leave the "link"?
        auto depart = now;
        if (bucket_) {
            if (!bucket_->tryReserve(static_cast<size_t>(n), now,
                                     std::chrono::milliseconds(options_.max_queue_ms), depart)) {
                metrics_.recordShaperDrop(); // shaper queue overflow
                continue;
            }
        }

        // 3. Latency + jitter (propagation delay after serialisation).
        long delay_us = delay_model_ ? delay_model_->nextDelayMicros() : 0;
        pkt.release_at = depart + std::chrono::microseconds(delay_us);
        queue_.push(std::move(pkt));
    }
}

void ChaosEngine::schedulerLoop() {
    for (;;) {
        auto maybe_pkt = queue_.pop();
        if (!maybe_pkt.has_value()) break; // queue stopped

        Packet& pkt = *maybe_pkt;
        PacketParser::fixIpChecksum(pkt.data.data(), pkt.data.size());

        ssize_t written = io_.writePacket(pkt.data.data(), pkt.data.size());
        if (written > 0) {
            metrics_.recordOut(static_cast<size_t>(written), pkt.matched);
            if (pkt.matched) {
                auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - pkt.captured_at).count();
                metrics_.recordLatencyMicros(static_cast<long>(us));
            }
        }
    }
}

} // namespace chaos
