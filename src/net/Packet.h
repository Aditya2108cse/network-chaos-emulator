#pragma once
#include <vector>
#include <cstdint>
#include <chrono>

namespace chaos {

// A captured packet plus the metadata the pipeline attaches to it.
struct Packet {
    std::vector<uint8_t> data;
    std::chrono::steady_clock::time_point captured_at;
    std::chrono::steady_clock::time_point release_at;
    // True if the packet matched the experiment's protocol filter (and is
    // therefore eligible for loss/delay/bandwidth impairment and counted
    // in the experiment statistics).
    bool matched = false;

    size_t size() const { return data.size(); }
};

} // namespace chaos
