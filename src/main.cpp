// Network Latency & Packet-Loss Chaos Emulator -- entry point.
//
// Runs a timed, two-phase experiment on a TUN interface:
//   Phase 1 (baseline): chaos disabled, all packets forwarded immediately.
//   Phase 2 (chaos):    loss/delay/bandwidth impairment applied to packets
//                        matching --protocol.
// Each phase's duration is --duration / 2. At the end it prints and saves
// a NETWORK CHAOS EXPERIMENT REPORT comparing the two phases.
//
// Usage:
//   sudo ./chaos_emulator --dev tun0 --ip 10.8.0.1 --duration 60
//        --protocol tcp --latency-ms 100 --jitter-ms 20
//        --loss-p 5 --bandwidth-mbps 10
//
// Requires CAP_NET_ADMIN (run as root, or:
//   sudo setcap cap_net_admin+ep ./chaos_emulator)
//
// While it runs, generate real traffic through the interface from another
// terminal, e.g.: ping -c 200 10.8.0.2   or   iperf3 -c <target-on-subnet>

#include "net/TunInterface.h"
#include "net/PacketParser.h"
#include "chaos/LossModel.h"
#include "chaos/DelayJitterModel.h"
#include "engine/ChaosEngine.h"
#include "metrics/MetricsCollector.h"
#include "metrics/SystemMonitor.h"
#include "report/ReportGenerator.h"

#include <algorithm>
#include <csignal>
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <atomic>

namespace {

std::atomic<bool> g_shutdown{false};
void handleSignal(int) { g_shutdown.store(true, std::memory_order_relaxed); }

struct CliOptions {
    std::string dev_name = "tun0";
    std::string local_ip = "10.8.0.1";
    std::string experiment_id = "EXP-001";
    int duration_sec = 60;

    chaos::ProtocolFilter protocol = chaos::ProtocolFilter::All;

    std::string loss_model = "bernoulli"; // simpler default for reproducible reports
    double loss_pct = 5.0;                // percent, matches report units

    std::string delay_model = "gaussian";
    long latency_ms = 100;
    double jitter_ms = 20.0;

    double bandwidth_mbps = 0.0; // 0 = unlimited
    std::string report_path = "chaos_report.txt";
};

CliOptions parseArgs(int argc, char** argv) {
    CliOptions o;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto next = [&](const char* flag) -> std::string {
            if (i + 1 >= argc) { std::cerr << "Missing value for " << flag << "\n"; std::exit(1); }
            return argv[++i];
        };
        if (arg == "--dev") o.dev_name = next("--dev");
        else if (arg == "--ip") o.local_ip = next("--ip");
        else if (arg == "--id") o.experiment_id = next("--id");
        else if (arg == "--duration") o.duration_sec = std::stoi(next("--duration"));
        else if (arg == "--protocol") {
            std::string p = next("--protocol");
            if (!chaos::parseProtocolFilter(p, o.protocol)) {
                std::cerr << "Unknown --protocol '" << p << "' (use tcp|udp|icmp|all)\n";
                std::exit(1);
            }
        }
        else if (arg == "--loss-model") o.loss_model = next("--loss-model");
        else if (arg == "--loss-p") o.loss_pct = std::stod(next("--loss-p"));
        else if (arg == "--delay-model") o.delay_model = next("--delay-model");
        else if (arg == "--latency-ms") o.latency_ms = std::stol(next("--latency-ms"));
        else if (arg == "--jitter-ms") o.jitter_ms = std::stod(next("--jitter-ms"));
        else if (arg == "--bandwidth-mbps") o.bandwidth_mbps = std::stod(next("--bandwidth-mbps"));
        else if (arg == "--report") o.report_path = next("--report");
        else if (arg == "--help") { std::cout << "See usage comment at top of main.cpp\n"; std::exit(0); }
        else { std::cerr << "Unknown argument: " << arg << "\n"; std::exit(1); }
    }
    return o;
}

chaos::PhaseStats buildPhaseStats(const chaos::MetricsCollector::Snapshot& before,
                                   const chaos::MetricsCollector::Snapshot& after,
                                   const chaos::SystemSample& sys_before,
                                   const chaos::SystemSample& sys_after,
                                   double phase_seconds) {
    chaos::PhaseStats p;
    p.packets_sent = after.matched_in - before.matched_in;
    p.packets_received = after.matched_out - before.matched_out;

    auto& lat = after.latency; // resetLatency() was called at phase start
    p.avg_rtt_ms = lat.mean_us / 1000.0;
    p.min_rtt_ms = lat.min_us / 1000.0;
    p.max_rtt_ms = lat.max_us / 1000.0;
    p.jitter_ms = lat.stddev_us / 1000.0;

    p.loss_pct = p.packets_sent ? 100.0 * (p.packets_sent - p.packets_received) / p.packets_sent : 0.0;

    uint64_t bytes_out = after.matched_bytes_out - before.matched_bytes_out;
    p.throughput_mbps = phase_seconds > 0 ? (bytes_out * 8.0 / 1e6) / phase_seconds : 0.0;

    p.system = chaos::SystemMonitor::between(sys_before, sys_after);
    return p;
}

} // namespace

int main(int argc, char** argv) {
    CliOptions opts = parseArgs(argc, argv);
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    chaos::TunInterface tun;
    if (!tun.open(opts.dev_name)) {
        std::cerr << "Failed to open TUN device. Are you running with CAP_NET_ADMIN / root?\n";
        return 1;
    }
    if (!tun.configure(opts.local_ip)) {
        std::cerr << "Warning: failed to auto-configure " << tun.name() << "\n";
    }
    std::cout << "TUN interface " << tun.name() << " up at " << opts.local_ip << "\n";

    std::unique_ptr<chaos::ILossModel> loss_model;
    if (opts.loss_model == "gilbert-elliott") {
        chaos::GilbertElliottLossModel::Params p;
        p.loss_in_bad = std::min(1.0, (opts.loss_pct / 100.0) * 4.0);
        loss_model = std::make_unique<chaos::GilbertElliottLossModel>(p);
    } else {
        loss_model = std::make_unique<chaos::BernoulliLossModel>(opts.loss_pct / 100.0);
    }

    long base_delay_us = opts.latency_ms * 1000;
    double jitter_stddev_us = opts.jitter_ms * 1000.0;
    std::unique_ptr<chaos::IDelayModel> delay_model;
    if (opts.delay_model == "correlated") {
        delay_model = std::make_unique<chaos::CorrelatedJitterModel>(base_delay_us, jitter_stddev_us, 0.85);
    } else {
        delay_model = std::make_unique<chaos::GaussianJitterModel>(base_delay_us, jitter_stddev_us);
    }

    chaos::MetricsCollector metrics;
    chaos::ChaosEngine::Options eng_opts;
    eng_opts.protocol = opts.protocol;
    eng_opts.bandwidth_mbps = opts.bandwidth_mbps;

    chaos::ChaosEngine engine(tun, std::move(loss_model), std::move(delay_model), metrics, eng_opts);

    int half = std::max(1, opts.duration_sec / 2);

    std::cout << "Phase 1/2: BASELINE (" << half << "s) -- chaos disabled, generate traffic now\n";
    engine.setChaosEnabled(false);
    metrics.resetLatency();
    auto sys_before_baseline = chaos::SystemMonitor::sample();
    auto snap_before_baseline = metrics.snapshot();
    engine.start();

    auto phase_end = std::chrono::steady_clock::now() + std::chrono::seconds(half);
    while (std::chrono::steady_clock::now() < phase_end && !g_shutdown.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    auto snap_after_baseline = metrics.snapshot();
    auto sys_after_baseline = chaos::SystemMonitor::sample();

    chaos::PhaseStats baseline = buildPhaseStats(snap_before_baseline, snap_after_baseline,
                                                 sys_before_baseline, sys_after_baseline, half);

    bool aborted = g_shutdown.load(std::memory_order_relaxed);
    chaos::PhaseStats chaos_stats{};
    if (!aborted) {
        std::cout << "Phase 2/2: CHAOS (" << half << "s) -- impairment active, keep sending traffic\n";
        engine.setChaosEnabled(true);
        metrics.resetLatency();
        auto sys_before_chaos = chaos::SystemMonitor::sample();
        auto snap_before_chaos = metrics.snapshot();

        phase_end = std::chrono::steady_clock::now() + std::chrono::seconds(half);
        while (std::chrono::steady_clock::now() < phase_end && !g_shutdown.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            metrics.printReport();
        }
        auto snap_after_chaos = metrics.snapshot();
        auto sys_after_chaos = chaos::SystemMonitor::sample();
        chaos_stats = buildPhaseStats(snap_before_chaos, snap_after_chaos,
                                      sys_before_chaos, sys_after_chaos, half);
    }

    std::cout << "Stopping engine (draining in-flight packets)...\n";
    engine.stop();

    chaos::ExperimentConfig cfg;
    cfg.experiment_id = opts.experiment_id;
    cfg.interface_name = tun.name();
    cfg.protocol = opts.protocol;
    cfg.duration_sec = opts.duration_sec;
    cfg.latency_ms = opts.latency_ms;
    cfg.jitter_ms = static_cast<long>(opts.jitter_ms);
    cfg.packet_loss_pct = opts.loss_pct;
    cfg.bandwidth_mbps = opts.bandwidth_mbps;

    std::string report = chaos::ReportGenerator::render(cfg, baseline, chaos_stats, /*network_restored=*/true);
    std::cout << "\n" << report;
    chaos::ReportGenerator::writeToFile(report, opts.report_path);
    std::cout << "Report saved to " << opts.report_path << "\n";

    return aborted ? 1 : 0;
}
