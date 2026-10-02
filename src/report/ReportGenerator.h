#pragma once
#include "../metrics/MetricsCollector.h"
#include "../metrics/SystemMonitor.h"
#include "../net/PacketParser.h"
#include <string>

namespace chaos {

// Everything measured during one phase (baseline or chaos) of an
// experiment: network-level stats plus system resource usage sampled at
// the start and end of the phase.
struct PhaseStats {
    uint64_t packets_sent = 0;     // matched packets captured
    uint64_t packets_received = 0; // matched packets successfully reinjected
    double avg_rtt_ms = 0.0;
    double min_rtt_ms = 0.0;
    double max_rtt_ms = 0.0;
    double jitter_ms = 0.0;        // stddev of latency, i.e. RTT variation
    double loss_pct = 0.0;
    double throughput_mbps = 0.0;
    SystemUsage system;
};

struct ExperimentConfig {
    std::string experiment_id = "EXP-001";
    std::string interface_name = "tun0";
    ProtocolFilter protocol = ProtocolFilter::All;
    int duration_sec = 60;
    long latency_ms = 0;
    long jitter_ms = 0;
    double packet_loss_pct = 0.0;
    double bandwidth_mbps = 0.0; // 0 = unlimited
};

// Renders the fixed-width experiment report (matching the standard
// NETWORK CHAOS EXPERIMENT REPORT layout) and can write it to a file.
class ReportGenerator {
public:
    static std::string render(const ExperimentConfig& cfg,
                              const PhaseStats& baseline,
                              const PhaseStats& chaos,
                              bool network_restored);

    static void writeToFile(const std::string& text, const std::string& path);
};

} // namespace chaos
