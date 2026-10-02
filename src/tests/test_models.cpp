// Lightweight, dependency-free sanity tests for the impairment models and
// checksum logic. Deliberately avoids GoogleTest/Catch2 so this compiles
// standalone with no extra fetch/install step -- swap in a real framework
// later without changing what's being asserted.
//
// These exercise exactly the pieces that do NOT require opening a TUN
// device or shelling out to `ip` (which need root/CAP_NET_ADMIN and a real
// Linux network stack), so they're safe to run in any sandboxed
// environment, including CI containers with no network privileges.

#include "../chaos/LossModel.h"
#include "../chaos/DelayJitterModel.h"
#include "../net/PacketParser.h"
#include "../scheduler/DelayQueue.h"

#include <iostream>
#include <cassert>
#include <vector>
#include <cmath>
#include <cstring>
#include <climits>
#include <algorithm>

using namespace chaos;

namespace {

int g_failures = 0;

void expect(bool cond, const std::string& msg) {
    if (!cond) {
        std::cerr << "  [FAIL] " << msg << "\n";
        ++g_failures;
    } else {
        std::cout << "  [ OK ] " << msg << "\n";
    }
}

void testBernoulliLossConvergesToConfiguredRate() {
    std::cout << "-- BernoulliLossModel --\n";
    const double p = 0.2;
    BernoulliLossModel model(p, /*seed=*/42);
    const int trials = 200000;
    int dropped = 0;
    for (int i = 0; i < trials; ++i) {
        if (model.shouldDrop()) ++dropped;
    }
    double observed = static_cast<double>(dropped) / trials;
    expect(std::fabs(observed - p) < 0.01,
           "observed drop rate " + std::to_string(observed) +
           " within 1% of configured " + std::to_string(p));
}

void testGilbertElliottProducesBurstsNotFlatNoise() {
    std::cout << "-- GilbertElliottLossModel --\n";
    GilbertElliottLossModel::Params params;
    params.p_good_to_bad = 0.05;
    params.p_bad_to_good = 0.3;
    params.loss_in_good = 0.0;
    params.loss_in_bad = 0.9;
    GilbertElliottLossModel model(params, /*seed=*/7);

    // A burst means: once we see a drop, the next packet is much more
    // likely to also drop than the unconditional average would predict.
    // We approximate this by measuring P(drop | previous packet dropped)
    // vs the overall drop rate, and expect the conditional rate to be
    // noticeably higher for a genuinely bursty model.
    const int trials = 100000;
    int total_drops = 0;
    int prev_dropped_count = 0;
    int drop_after_drop_count = 0;
    bool prev = false;
    for (int i = 0; i < trials; ++i) {
        bool dropped = model.shouldDrop();
        if (dropped) ++total_drops;
        if (prev) {
            ++prev_dropped_count;
            if (dropped) ++drop_after_drop_count;
        }
        prev = dropped;
    }
    double overall_rate = static_cast<double>(total_drops) / trials;
    double conditional_rate = prev_dropped_count > 0
        ? static_cast<double>(drop_after_drop_count) / prev_dropped_count
        : 0.0;

    std::cout << "     overall drop rate: " << overall_rate << "\n";
    std::cout << "     P(drop | prev dropped): " << conditional_rate << "\n";
    expect(conditional_rate > overall_rate * 1.5,
           "conditional drop-after-drop probability is markedly higher than "
           "the overall rate, confirming burstiness (Markov memory)");
}

void testGaussianJitterStaysNonNegativeAndCentered() {
    std::cout << "-- GaussianJitterModel --\n";
    GaussianJitterModel model(/*base_delay_us=*/50000, /*jitter_stddev_us=*/10000, /*seed=*/1);
    const int trials = 50000;
    long sum = 0;
    long min_val = LONG_MAX;
    for (int i = 0; i < trials; ++i) {
        long d = model.nextDelayMicros();
        min_val = std::min(min_val, d);
        sum += d;
    }
    double mean = static_cast<double>(sum) / trials;
    expect(min_val >= 0, "no negative delays produced across " + std::to_string(trials) + " samples");
    expect(std::fabs(mean - 50000.0) < 500.0,
           "mean delay " + std::to_string(mean) + "us close to configured base 50000us");
}

void testCorrelatedJitterIsSmootherThanIndependentNoise() {
    std::cout << "-- CorrelatedJitterModel --\n";
    CorrelatedJitterModel model(/*base_delay_us=*/50000, /*jitter_stddev_us=*/10000,
                                 /*alpha=*/0.9, /*seed=*/3);
    // "Smoother" = smaller average absolute difference between consecutive
    // samples than the raw jitter stddev would suggest for IID noise.
    long prev = model.nextDelayMicros();
    double sum_abs_diff = 0;
    const int trials = 20000;
    for (int i = 0; i < trials; ++i) {
        long d = model.nextDelayMicros();
        sum_abs_diff += std::fabs(static_cast<double>(d - prev));
        prev = d;
    }
    double mean_abs_diff = sum_abs_diff / trials;
    std::cout << "     mean |delta| between consecutive delays: " << mean_abs_diff << "us\n";
    // For alpha=0.9 the process changes slowly; with IID N(0, 10000us) jitter
    // the expected |difference| between independent samples would be on the
    // order of stddev*sqrt(2/pi)*sqrt(2) ~= 11284us. A correlated process
    // should move noticeably less than that per step.
    expect(mean_abs_diff < 8000.0,
           "consecutive delay changes are damped relative to independent-noise baseline (~11284us)");
}

void testIpChecksumRoundTrips() {
    std::cout << "-- PacketParser::fixIpChecksum --\n";
    // A minimal, valid 20-byte IPv4 header (no options), UDP protocol,
    // arbitrary but well-formed addresses. Values taken from a real
    // captured packet header layout.
    std::vector<uint8_t> pkt = {
        0x45, 0x00, 0x00, 0x1c,             // version/IHL, ToS, total length
        0x00, 0x00, 0x40, 0x00,             // id, flags/fragment offset
        0x40, 0x11, 0x00, 0x00,             // TTL, protocol=UDP(17), checksum(zeroed)
        0x0A, 0x08, 0x00, 0x01,             // src ip 10.8.0.1
        0x0A, 0x08, 0x00, 0x02              // dst ip 10.8.0.2
    };

    PacketParser::fixIpChecksum(pkt.data(), pkt.size());
    uint16_t stored_checksum = (static_cast<uint16_t>(pkt[10]) << 8) | pkt[11];
    expect(stored_checksum != 0, "checksum field was populated (non-zero) after fix-up");

    // Recomputing the checksum over the *entire* header (including the
    // now-correct checksum field) must fold to exactly zero -- that's the
    // defining property of the one's-complement checksum algorithm and is
    // what the receiving IP stack itself checks.
    uint16_t verify = PacketParser::onesComplementChecksum(pkt.data(), 20);
    expect(verify == 0, "checksum self-verifies to zero over full header (RFC 1071 property)");

    auto flow = PacketParser::parseFlowKey(pkt.data(), pkt.size());
    expect(flow.has_value(), "flow key parsed successfully");
    if (flow) {
        expect(flow->protocol == 17, "protocol correctly read as UDP(17)");
        // 10.8.0.1 -> 0x0A080001
        expect(flow->src_ip == 0x0A080001u, "src_ip endianness-converted correctly");
        expect(flow->dst_ip == 0x0A080002u, "dst_ip endianness-converted correctly");
    }
}

void testDelayQueueOrdersByReleaseTimeNotInsertionOrder() {
    std::cout << "-- DelayQueue (reordering) --\n";
    DelayQueue q;
    auto now = std::chrono::steady_clock::now();

    Packet a; a.data = {0xAA}; a.release_at = now + std::chrono::milliseconds(50);
    Packet b; b.data = {0xBB}; b.release_at = now; // due immediately, pushed second
    Packet c; c.data = {0xCC}; c.release_at = now + std::chrono::milliseconds(10);

    q.push(std::move(a)); // longest delay, pushed FIRST
    q.push(std::move(b)); // no delay, pushed SECOND
    q.push(std::move(c)); // medium delay, pushed THIRD

    // Expected pop order by release time: b (0ms), c (10ms), a (50ms) --
    // i.e. NOT insertion order (a, b, c). This is exactly the mechanism
    // that produces packet reordering in the real pipeline.
    auto first = q.pop();
    auto second = q.pop();
    auto third = q.pop();
    q.stop();

    expect(first.has_value() && first->data[0] == 0xBB, "earliest-due packet (b) pops first");
    expect(second.has_value() && second->data[0] == 0xCC, "next-due packet (c) pops second");
    expect(third.has_value() && third->data[0] == 0xAA, "latest-due packet (a) pops last -- reordered vs insertion");
}

} // namespace

int main() {
    testBernoulliLossConvergesToConfiguredRate();
    testGilbertElliottProducesBurstsNotFlatNoise();
    testGaussianJitterStaysNonNegativeAndCentered();
    testCorrelatedJitterIsSmootherThanIndependentNoise();
    testIpChecksumRoundTrips();
    testDelayQueueOrdersByReleaseTimeNotInsertionOrder();

    std::cout << "\n" << (g_failures == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED")
              << " (" << g_failures << " failure(s))\n";
    return g_failures == 0 ? 0 : 1;
}
