#pragma once
#include "../net/PacketIO.h"
#include "../net/PacketParser.h"
#include "../chaos/ImpairmentModel.h"
#include "../chaos/TokenBucket.h"
#include "../scheduler/DelayQueue.h"
#include "../metrics/MetricsCollector.h"

#include <atomic>
#include <memory>
#include <optional>
#include <thread>

namespace chaos {

// Pipeline:
//
//   capture thread:  read -> classify (protocol filter)
//                         -> loss model -> bandwidth shaper -> delay model
//                         -> DelayQueue (min-heap by release time)
//   scheduler thread: pop due packet -> write back -> record latency
//
// Packets that don't match the protocol filter, or all packets while chaos
// is disabled (baseline phase), skip impairment and are forwarded at once.
class ChaosEngine {
public:
    struct Options {
        ProtocolFilter protocol = ProtocolFilter::All;
        double bandwidth_mbps = 0.0;   // 0 = unlimited (no shaping)
        int max_queue_ms = 300;        // shaper queue limit before tail drop
    };

    ChaosEngine(IPacketIO& io,
                std::unique_ptr<ILossModel> loss_model,
                std::unique_ptr<IDelayModel> delay_model,
                MetricsCollector& metrics,
                Options options);
    ~ChaosEngine();

    void start();

    // Stops capturing, lets already-queued packets finish (up to a bounded
    // wait), then stops the scheduler.
    void stop();

    // When disabled, every packet is forwarded immediately (baseline mode).
    void setChaosEnabled(bool enabled) { chaos_enabled_.store(enabled, std::memory_order_relaxed); }

private:
    void captureLoop();
    void schedulerLoop();

    IPacketIO& io_;
    std::unique_ptr<ILossModel> loss_model_;
    std::unique_ptr<IDelayModel> delay_model_;
    MetricsCollector& metrics_;
    Options options_;
    std::optional<TokenBucket> bucket_;

    DelayQueue queue_;
    std::thread capture_thread_;
    std::thread scheduler_thread_;
    std::atomic<bool> started_{false};
    std::atomic<bool> capturing_{false};
    std::atomic<bool> chaos_enabled_{true};
};

} // namespace chaos
