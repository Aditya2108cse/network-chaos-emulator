#pragma once
#include <chrono>
#include <cstddef>

namespace chaos {

// Token-bucket bandwidth shaper.
//
// Tokens (bytes of credit) refill continuously at `rate` bytes/second up to
// `burst` bytes. Sending a packet spends tokens equal to its size. If not
// enough tokens are available the bucket goes into "debt", and the packet
// must wait until the debt has been repaid by the refill rate -- that wait
// is what caps long-run throughput at the configured rate, exactly like a
// physical link that can only serialise so many bits per second.
//
// Rather than sleeping, the bucket *computes* the departure time and lets
// the DelayQueue hold the packet until then. Not thread-safe: only the
// capture thread calls it.
class TokenBucket {
public:
    using Clock = std::chrono::steady_clock;

    TokenBucket(double rate_bytes_per_sec, double burst_bytes);

    // Reserves capacity for a packet of `bytes`. On success returns true and
    // sets `depart` to the earliest time the packet may leave. Returns false
    // (tail drop, nothing consumed) if the packet would have to wait longer
    // than `max_wait` -- i.e. the shaper's queue is full.
    bool tryReserve(size_t bytes, Clock::time_point now,
                    std::chrono::microseconds max_wait, Clock::time_point& depart);

    double rate() const { return rate_; }

private:
    double rate_;
    double burst_;
    double tokens_;
    Clock::time_point last_;
    bool initialised_ = false;
};

} // namespace chaos
