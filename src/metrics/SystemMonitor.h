#pragma once
#include <chrono>

namespace chaos {

// Point-in-time reading of this process's resource usage, taken from the
// Linux /proc filesystem (a virtual filesystem the kernel exposes as
// files -- no special API needed, just read text).
struct SystemSample {
    std::chrono::steady_clock::time_point at;
    double cpu_seconds = 0.0; // user + system CPU time consumed so far
    double rss_mb = 0.0;      // resident memory (VmRSS from /proc/self/status)
    int threads = 0;          // thread count (Threads from /proc/self/status)
};

struct SystemUsage {
    double cpu_percent = 0.0;
    double memory_mb = 0.0;
    int threads = 0;
};

class SystemMonitor {
public:
    // Reads /proc/self/stat and /proc/self/status.
    static SystemSample sample();

    // CPU % over the interval [a, b]; memory and threads taken from b.
    static SystemUsage between(const SystemSample& a, const SystemSample& b);
};

} // namespace chaos
