#pragma once
#include "PacketIO.h"
#include <string>
#include <cstdint>
#include <vector>

namespace chaos {

// RAII wrapper around a Linux TUN device (/dev/net/tun).
//
// A TUN device is a *software-emulated network interface*: as far as the
// kernel's routing/IP stack is concerned it behaves like a real NIC, but
// every frame that would normally go out to hardware instead lands in this
// process as a plain read() on a file descriptor, and every frame we write()
// is injected back into the kernel's network stack as if it had arrived on
// a physical link. This is the hardware/software boundary the whole
// project sits on.
//
// Systems-programming concepts exercised here:
//  - character device I/O via ioctl() (TUNSETIFF)
//  - RAII fd ownership (no leaked descriptors on exceptions)
//  - CAP_NET_ADMIN privilege requirement instead of full root
class TunInterface : public IPacketIO {
public:
    TunInterface() = default;
    ~TunInterface() override;

    // Non-copyable, movable (owns a raw OS fd).
    TunInterface(const TunInterface&) = delete;
    TunInterface& operator=(const TunInterface&) = delete;
    TunInterface(TunInterface&& other) noexcept;
    TunInterface& operator=(TunInterface&& other) noexcept;

    // Opens /dev/net/tun and registers a TUN device with the requested
    // name (e.g. "tun0"). Requires CAP_NET_ADMIN. Returns false on failure
    // (check errno / stderr for the reason).
    bool open(const std::string& dev_name);

    // Brings the interface up and assigns it an IPv4 address + /24 mask,
    // equivalent to:
    //   ip addr add <local_ip>/24 dev <dev_name>
    //   ip link set <dev_name> up
    // Shells out to `ip` deliberately -- reimplementing netlink route
    // configuration is out of scope for this project.
    bool configure(const std::string& local_ip);

    // poll()-based wait so the capture thread can notice shutdown requests
    // instead of blocking forever inside read().
    bool waitReadable(int timeout_ms) override;

    // Read/write one raw IP packet (IPacketIO interface).
    ssize_t readPacket(uint8_t* buf, size_t cap) override;
    ssize_t writePacket(const uint8_t* buf, size_t len) override;

    int fd() const { return fd_; }
    const std::string& name() const { return dev_name_; }

    void close();

private:
    int fd_ = -1;
    std::string dev_name_;
};

} // namespace chaos
