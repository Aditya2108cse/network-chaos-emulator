#include "SystemMonitor.h"
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>

namespace chaos {

SystemSample SystemMonitor::sample() {
    SystemSample s;
    s.at = std::chrono::steady_clock::now();

    // /proc/self/stat: fields 14 (utime) and 15 (stime), in clock ticks.
    // The 2nd field (process name) is wrapped in parentheses and may itself
    // contain spaces, so parse from the last ')' onwards.
    {
        std::ifstream f("/proc/self/stat");
        std::string line;
        std::getline(f, line);
        size_t close = line.rfind(')');
        if (close != std::string::npos) {
            std::istringstream rest(line.substr(close + 2));
            std::string tok;
            unsigned long long utime = 0, stime = 0;
            // After ')' the first token is field 3 (state). utime is field 14.
            for (int field = 3; rest >> tok; ++field) {
                if (field == 14) utime = std::stoull(tok);
                if (field == 15) { stime = std::stoull(tok); break; }
            }
            long ticks = sysconf(_SC_CLK_TCK);
            if (ticks > 0) s.cpu_seconds = static_cast<double>(utime + stime) / ticks;
        }
    }

    // /proc/self/status: "VmRSS:  1234 kB" and "Threads:  4"
    {
        std::ifstream f("/proc/self/status");
        std::string line;
        while (std::getline(f, line)) {
            if (line.rfind("VmRSS:", 0) == 0) {
                s.rss_mb = std::stod(line.substr(6)) / 1024.0;
            } else if (line.rfind("Threads:", 0) == 0) {
                s.threads = std::stoi(line.substr(8));
            }
        }
    }
    return s;
}

SystemUsage SystemMonitor::between(const SystemSample& a, const SystemSample& b) {
    SystemUsage u;
    double wall = std::chrono::duration<double>(b.at - a.at).count();
    if (wall > 0) u.cpu_percent = 100.0 * (b.cpu_seconds - a.cpu_seconds) / wall;
    u.memory_mb = b.rss_mb;
    u.threads = b.threads;
    return u;
}

} // namespace chaos
