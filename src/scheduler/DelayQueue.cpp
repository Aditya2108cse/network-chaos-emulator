#include "DelayQueue.h"

namespace chaos {

void DelayQueue::push(Packet packet) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        heap_.push(std::move(packet));
    }
    cv_.notify_all();
}

std::optional<Packet> DelayQueue::pop() {
    std::unique_lock<std::mutex> lock(mutex_);
    for (;;) {
        if (stopped_) return std::nullopt;

        if (heap_.empty()) {
            // Nothing queued at all -- sleep until push() wakes us.
            cv_.wait(lock);
            continue;
        }

        auto release_at = heap_.top().release_at;
        auto now = std::chrono::steady_clock::now();
        if (release_at <= now) {
            // Earliest packet is due now -- pop it.
            // priority_queue::top() only gives const access, and Packet
            // contains a vector we want to move out rather than copy, so
            // we const_cast to move from the top element before popping.
            // This is safe: the heap invariant only depends on
            // release_at, which we do not modify.
            Packet pkt = std::move(const_cast<Packet&>(heap_.top()));
            heap_.pop();
            return pkt;
        }

        // Sleep until either the earliest packet's release time, or until
        // something new is pushed that might be due sooner.
        cv_.wait_until(lock, release_at);
    }
}

void DelayQueue::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopped_ = true;
    }
    cv_.notify_all();
}

size_t DelayQueue::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return heap_.size();
}

} // namespace chaos
