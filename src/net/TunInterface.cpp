#include "TunInterface.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

#include <fcntl.h>
#include <poll.h>
#include <cerrno>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/if_tun.h>

namespace chaos {

TunInterface::~TunInterface() {
    close();
}

TunInterface::TunInterface(TunInterface&& other) noexcept
    : fd_(other.fd_), dev_name_(std::move(other.dev_name_)) {
    other.fd_ = -1;
}

TunInterface& TunInterface::operator=(TunInterface&& other) noexcept {
    if (this != &other) {
        close();
        fd_ = other.fd_;
        dev_name_ = std::move(other.dev_name_);
        other.fd_ = -1;
    }
    return *this;
}

bool TunInterface::open(const std::string& dev_name) {
    // /dev/net/tun is the clone device: every TUN/TAP interface is created
    // by opening this same node and then telling the kernel, via ioctl(),
    // which interface name and mode (TUN = L3/IP packets, no Ethernet
    // header, vs TAP = L2 frames) we want.
    fd_ = ::open("/dev/net/tun", O_RDWR);
    if (fd_ < 0) {
        std::perror("open(/dev/net/tun)");
        return false;
    }

    struct ifreq ifr;
    std::memset(&ifr, 0, sizeof(ifr));
    // IFF_TUN: we want raw IP packets, not Ethernet frames.
    // IFF_NO_PI: don't prefix each packet with the 4-byte protocol info
    // header the kernel would otherwise add -- keeps our parsing simple
    // since we only handle IPv4/IPv6 payloads directly.
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
    std::strncpy(ifr.ifr_name, dev_name.c_str(), IFNAMSIZ - 1);

    if (ioctl(fd_, TUNSETIFF, &ifr) < 0) {
        std::perror("ioctl(TUNSETIFF)");
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    dev_name_ = ifr.ifr_name; // kernel may adjust/confirm the actual name
    return true;
}

bool TunInterface::configure(const std::string& local_ip) {
    if (fd_ < 0) return false;

    std::string cmd1 = "ip addr add " + local_ip + "/24 dev " + dev_name_ + " 2>&1";
    std::string cmd2 = "ip link set " + dev_name_ + " up 2>&1";

    // Deliberately shelling out rather than hand-rolling netlink calls --
    // configuring routes/addresses via `ip` (iproute2) is standard practice
    // and keeps this class focused on the packet I/O path.
    int rc1 = std::system(cmd1.c_str());
    int rc2 = std::system(cmd2.c_str());
    return rc1 == 0 && rc2 == 0;
}

bool TunInterface::waitReadable(int timeout_ms) {
    if (fd_ < 0) return false;
    struct pollfd pfd;
    pfd.fd = fd_;
    pfd.events = POLLIN;
    pfd.revents = 0;
    int rc = ::poll(&pfd, 1, timeout_ms);
    return rc > 0 && (pfd.revents & POLLIN);
}

ssize_t TunInterface::readPacket(uint8_t* buf, size_t cap) {
    if (fd_ < 0) return -1;
    return ::read(fd_, buf, cap);
}

ssize_t TunInterface::writePacket(const uint8_t* buf, size_t len) {
    if (fd_ < 0) return -1;
    return ::write(fd_, buf, len);
}

void TunInterface::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

} // namespace chaos
