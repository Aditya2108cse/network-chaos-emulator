#pragma once
#include "../net/Packet.h"
#include <queue>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <optional>

namespace chaos {

// Thread-safe priority queue of packets ordered by release_at (earliest
// release time first). One thread (the chaos engine) pushes packets in
// arbitrary order as it decides delays; a separate scheduler thread pops
// whichever packet is due next and reinjects it.
//
// Note this data structure is also what makes packet *reordering* fall out
// almost for free: if packet A gets a longer delay than packet B pushed
// after it, B's earlier release_at means B comes out of the heap first --
// i.e. out of original order, exactly like real network reordering.
class DelayQueue {
public:
    // Adds a packet to the queue; wakes up any thread blocked in pop().
    void push(Packet packet);

    // Blocks until the earliest-due packet's release time has arrived,
    // then returns it. Returns std::nullopt if stop() was called while
    // waiting (used for clean shutdown).
    std::optional<Packet> pop();

    // Unblocks any waiting pop() calls and causes them to return nullopt.
    void stop();

    size_t size() const;

private:
    struct Compare {
        // std::priority_queue is a max-heap by default; flipping the
        // comparison here gives us a min-heap ordered by release_at,
        // i.e. the *earliest* release time is always at the top.
        bool operator()(const Packet& a, const Packet& b) const {
            return a.release_at > b.release_at;
        }
    };

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::priority_queue<Packet, std::vector<Packet>, Compare> heap_;
    bool stopped_ = false;
};

} // namespace chaos
