#include "TokenBucket.h"
#include <algorithm>

namespace chaos {

TokenBucket::TokenBucket(double rate_bytes_per_sec, double burst_bytes)
    : rate_(rate_bytes_per_sec), burst_(burst_bytes), tokens_(burst_bytes) {}

bool TokenBucket::tryReserve(size_t bytes, Clock::time_point now,
                             std::chrono::microseconds max_wait,
                             Clock::time_point& depart) {
    if (!initialised_) {
        last_ = now;
        initialised_ = true;
    }

    // Refill for the time elapsed since the last call.
    double elapsed = std::chrono::duration<double>(now - last_).count();
    if (elapsed > 0) {
        tokens_ = std::min(burst_, tokens_ + elapsed * rate_);
        last_ = now;
    }

    double after = tokens_ - static_cast<double>(bytes);
    if (after >= 0) {
        tokens_ = after;
        depart = now;
        return true;
    }

    // Not enough credit: the deficit must be repaid at `rate_`.
    double wait_sec = -after / rate_;
    auto wait = std::chrono::microseconds(static_cast<long long>(wait_sec * 1e6));
    if (wait > max_wait) return false; // queue full -> tail drop

    tokens_ = after; // go into debt; later packets queue behind this one
    depart = now + wait;
    return true;
}

} // namespace chaos
