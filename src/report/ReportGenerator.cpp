#include "ReportGenerator.h"
#include <fstream>
#include <iomanip>
#include <sstream>

namespace chaos {

namespace {
constexpr int kLabelW  = 22; // "Average RTT" etc, including trailing space before value
constexpr int kColW    = 15; // width of each BASELINE / CHAOS value cell
constexpr int kKvLabelW = 20; // width of "Experiment ID" style labels before " : "

std::string line(char c, int n) { return std::string(n, c); }

std::string num1(double v) {
    std::ostringstream o; o << std::fixed << std::setprecision(1) << v; return o.str();
}
std::string num2(double v) {
    std::ostringstream o; o << std::fixed << std::setprecision(2) << v; return o.str();
}

// "Experiment ID       : EXP-001"
void kv(std::ostringstream& o, const std::string& label, const std::string& value) {
    o << std::left << std::setw(kKvLabelW) << label << ": " << value << "\n";
}

// One row of the two-column BASELINE/CHAOS tables, each value already
// carrying its unit (e.g. "99.1 ms"), padded to a fixed column width so
// every row lines up regardless of digit count.
void row(std::ostringstream& o, const std::string& label,
         const std::string& baseline_val, const std::string& chaos_val) {
    o << std::left << std::setw(kLabelW) << label
      << std::left << std::setw(kColW) << baseline_val
      << chaos_val << "\n";
}
} // namespace

std::string ReportGenerator::render(const ExperimentConfig& cfg,
                                    const PhaseStats& b, const PhaseStats& c,
                                    bool network_restored) {
    std::ostringstream o;
    o << line('=', 48) << "\n"
      << "       NETWORK CHAOS EXPERIMENT REPORT\n"
      << line('=', 48) << "\n\n";

    kv(o, "Experiment ID", cfg.experiment_id);
    kv(o, "Protocol", protocolFilterName(cfg.protocol));
    kv(o, "Interface", cfg.interface_name);
    kv(o, "Duration", std::to_string(cfg.duration_sec) + " seconds");

    o << "\n---------------- CHAOS CONFIGURATION -----------\n\n";
    kv(o, "Latency", std::to_string(cfg.latency_ms) + " ms");
    kv(o, "Jitter", std::to_string(cfg.jitter_ms) + " ms");
    kv(o, "Packet Loss", num1(cfg.packet_loss_pct) + " %");
    kv(o, "Bandwidth", cfg.bandwidth_mbps > 0 ? num1(cfg.bandwidth_mbps) + " Mbps" : std::string("unlimited"));

    o << "\n---------------- NETWORK RESULTS ---------------\n\n";
    row(o, "", "BASELINE", "CHAOS");
    row(o, "Average RTT", num1(b.avg_rtt_ms) + " ms", num1(c.avg_rtt_ms) + " ms");
    row(o, "Min RTT", num1(b.min_rtt_ms) + " ms", num1(c.min_rtt_ms) + " ms");
    row(o, "Max RTT", num1(b.max_rtt_ms) + " ms", num1(c.max_rtt_ms) + " ms");
    row(o, "Jitter", num1(b.jitter_ms) + " ms", num1(c.jitter_ms) + " ms");
    row(o, "Packet Loss", num1(b.loss_pct) + " %", num1(c.loss_pct) + " %");
    row(o, "Throughput", num2(b.throughput_mbps) + " Mbps", num2(c.throughput_mbps) + " Mbps");
    o << "\n";
    row(o, "Packets Sent", std::to_string(b.packets_sent), std::to_string(c.packets_sent));
    row(o, "Packets Received", std::to_string(b.packets_received), std::to_string(c.packets_received));
    row(o, "Packets Lost", std::to_string(b.packets_sent - b.packets_received),
        std::to_string(c.packets_sent - c.packets_received));

    o << "\n---------------- SYSTEM RESULTS ----------------\n\n";
    row(o, "CPU Usage", num1(b.system.cpu_percent) + " %", num1(c.system.cpu_percent) + " %");
    row(o, "Memory Usage", num1(b.system.memory_mb) + " MB", num1(c.system.memory_mb) + " MB");
    row(o, "Threads", std::to_string(b.system.threads), std::to_string(c.system.threads));

    o << "\n---------------- EXPERIMENT --------------------\n\n";
    kv(o, "Status", "COMPLETED");
    kv(o, "Duration", std::to_string(cfg.duration_sec) + " sec");
    kv(o, "Network Restored", network_restored ? "YES" : "NO");

    o << "\n" << line('=', 48) << "\n";
    return o.str();
}

void ReportGenerator::writeToFile(const std::string& text, const std::string& path) {
    std::ofstream f(path);
    f << text;
}

} // namespace chaos
