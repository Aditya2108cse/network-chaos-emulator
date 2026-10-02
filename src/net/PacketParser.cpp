#include "PacketParser.h"
#include <cstring>
#include <netinet/in.h> // ntohs/ntohl
#include <netinet/ip.h> // IPPROTO_TCP/UDP, struct iphdr layout reference
#include <algorithm>
#include <cctype>

namespace chaos {

bool parseProtocolFilter(const std::string& text, ProtocolFilter& out) {
    std::string t = text;
    std::transform(t.begin(), t.end(), t.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (t == "all") out = ProtocolFilter::All;
    else if (t == "tcp") out = ProtocolFilter::Tcp;
    else if (t == "udp") out = ProtocolFilter::Udp;
    else if (t == "icmp") out = ProtocolFilter::Icmp;
    else return false;
    return true;
}

const char* protocolFilterName(ProtocolFilter f) {
    switch (f) {
        case ProtocolFilter::Tcp: return "TCP";
        case ProtocolFilter::Udp: return "UDP";
        case ProtocolFilter::Icmp: return "ICMP";
        default: return "ALL";
    }
}

bool PacketParser::matchesProtocol(ProtocolFilter f, const uint8_t* data, size_t len) {
    if (f == ProtocolFilter::All) return true;
    if (ipHeaderLength(data, len) == 0) return false; // not IPv4
    uint8_t proto = data[9];
    switch (f) {
        case ProtocolFilter::Tcp: return proto == IPPROTO_TCP;
        case ProtocolFilter::Udp: return proto == IPPROTO_UDP;
        case ProtocolFilter::Icmp: return proto == IPPROTO_ICMP;
        default: return true;
    }
}

size_t PacketParser::ipHeaderLength(const uint8_t* data, size_t len) {
    if (len < 20) return 0; // shorter than a minimal IPv4 header
    uint8_t version = (data[0] >> 4) & 0x0F;
    if (version != 4) return 0; // IPv6 handling omitted for this milestone
    uint8_t ihl = data[0] & 0x0F; // header length in 32-bit words
    size_t hdr_len = static_cast<size_t>(ihl) * 4;
    if (hdr_len < 20 || hdr_len > len) return 0;
    return hdr_len;
}

std::optional<FlowKey> PacketParser::parseFlowKey(const uint8_t* data, size_t len) {
    size_t ip_hdr_len = ipHeaderLength(data, len);
    if (ip_hdr_len == 0) return std::nullopt;

    FlowKey key;
    key.protocol = data[9];

    // IPv4 header fields are big-endian on the wire; the host CPU may be
    // little-endian (x86/most ARM) so these conversions are not optional.
    uint32_t src_be, dst_be;
    std::memcpy(&src_be, data + 12, 4);
    std::memcpy(&dst_be, data + 16, 4);
    key.src_ip = ntohl(src_be);
    key.dst_ip = ntohl(dst_be);

    // TCP and UDP both put source/dest port as the first two 16-bit fields
    // immediately after the IP header, so we can read them identically.
    if ((key.protocol == IPPROTO_TCP || key.protocol == IPPROTO_UDP) &&
        len >= ip_hdr_len + 4) {
        const uint8_t* l4 = data + ip_hdr_len;
        uint16_t sport_be, dport_be;
        std::memcpy(&sport_be, l4 + 0, 2);
        std::memcpy(&dport_be, l4 + 2, 2);
        key.src_port = ntohs(sport_be);
        key.dst_port = ntohs(dport_be);
    }

    return key;
}

uint16_t PacketParser::onesComplementChecksum(const uint8_t* data, size_t len) {
    uint32_t sum = 0;
    size_t i = 0;
    for (; i + 1 < len; i += 2) {
        uint16_t word = (static_cast<uint16_t>(data[i]) << 8) | data[i + 1];
        sum += word;
    }
    if (i < len) {
        sum += static_cast<uint16_t>(data[i]) << 8; // odd trailing byte, zero-padded
    }
    // Fold carries from the upper 16 bits back into the lower 16, repeatedly,
    // until it fits -- the standard one's-complement checksum fold.
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return static_cast<uint16_t>(~sum);
}

void PacketParser::fixIpChecksum(uint8_t* data, size_t len) {
    size_t ip_hdr_len = ipHeaderLength(data, len);
    if (ip_hdr_len == 0) return;

    // Checksum field lives at bytes [10,11] and must be zeroed before
    // recomputation -- it can't include itself in the sum.
    data[10] = 0;
    data[11] = 0;

    uint16_t csum = onesComplementChecksum(data, ip_hdr_len);
    uint16_t csum_be = htons(csum);
    std::memcpy(data + 10, &csum_be, 2);
}

} // namespace chaos
