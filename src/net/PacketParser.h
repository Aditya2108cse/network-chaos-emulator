#pragma once
#include <cstdint>
#include <cstddef>
#include <optional>
#include <string>

namespace chaos {

// A minimal 5-tuple, enough to key per-flow chaos rules (e.g. "only drop
// packets between this src/dst port pair").
struct FlowKey {
    uint32_t src_ip = 0;
    uint32_t dst_ip = 0;
    uint16_t src_port = 0;
    uint16_t dst_port = 0;
    uint8_t  protocol = 0; // IPPROTO_TCP / IPPROTO_UDP / etc.

    bool operator==(const FlowKey& o) const {
        return src_ip == o.src_ip && dst_ip == o.dst_ip &&
               src_port == o.src_port && dst_port == o.dst_port &&
               protocol == o.protocol;
    }
};

struct FlowKeyHash {
    size_t operator()(const FlowKey& k) const noexcept {
        // Simple order-dependent combine; sufficient for an
        // unordered_map key, not intended to be cryptographic.
        size_t h = std::hash<uint32_t>{}(k.src_ip);
        h ^= std::hash<uint32_t>{}(k.dst_ip) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<uint16_t>{}(k.src_port) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<uint16_t>{}(k.dst_port) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<uint8_t>{}(k.protocol) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

// Which packets an experiment targets. Packets that don't match are
// forwarded untouched (no loss, delay or shaping).
enum class ProtocolFilter { All, Tcp, Udp, Icmp };

// Parses "tcp" / "udp" / "icmp" / "all" (case-insensitive). Returns false
// if the string isn't recognised.
bool parseProtocolFilter(const std::string& text, ProtocolFilter& out);
const char* protocolFilterName(ProtocolFilter f); // "TCP", "UDP", "ICMP", "ALL"

class PacketParser {
public:
    // Parses the 5-tuple out of a raw IPv4 packet buffer. All multi-byte
    // header fields on the wire are big-endian ("network byte order"), so
    // this function is where ntohs()/ntohl() actually matter -- a direct,
    // hands-on example of the endianness concept from computer architecture.
    static std::optional<FlowKey> parseFlowKey(const uint8_t* data, size_t len);

    // True if the raw packet matches the filter. Non-IPv4 packets only
    // match ProtocolFilter::All.
    static bool matchesProtocol(ProtocolFilter f, const uint8_t* data, size_t len);

    // Returns the IHL-derived header length in bytes, or 0 if the buffer
    // doesn't look like a sane IPv4 header.
    static size_t ipHeaderLength(const uint8_t* data, size_t len);

    // Recomputes and patches in the IPv4 header checksum in-place.
    // Necessary because on a real NIC this is normally done by hardware
    // checksum offload; since we mutate packets in software (e.g. bit
    // flips for corruption injection, or just because TUN delivers
    // packets without it), we must redo the kernel/hardware's job by hand
    // or the receiving stack will silently discard the packet.
    static void fixIpChecksum(uint8_t* data, size_t len);

    // Generic one's-complement checksum over a buffer (RFC 1071), the
    // primitive both IP and TCP/UDP checksums are built from.
    static uint16_t onesComplementChecksum(const uint8_t* data, size_t len);
};

} // namespace chaos
