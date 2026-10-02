#pragma once
#include <cstddef>
#include <cstdint>
#include <sys/types.h>

namespace chaos {

// Abstract packet source/sink. TunInterface implements it for real
// traffic; tests implement it with an in-memory fake so the whole engine
// can be exercised without root privileges or a real network device.
class IPacketIO {
public:
    virtual ~IPacketIO() = default;

    // Waits up to timeout_ms for a packet to become readable.
    // Returns true if a read will not block.
    virtual bool waitReadable(int timeout_ms) = 0;

    // Reads one packet into buf (capacity cap). Returns bytes read, or -1.
    virtual ssize_t readPacket(uint8_t* buf, size_t cap) = 0;

    // Writes one packet. Returns bytes written, or -1.
    virtual ssize_t writePacket(const uint8_t* buf, size_t len) = 0;
};

} // namespace chaos
