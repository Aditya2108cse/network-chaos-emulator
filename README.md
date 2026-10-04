# Network Latency & Packet-Loss Chaos Emulator

A software-only network chaos testing engine built with **C++17 for
Linux**. The emulator uses a Linux **TUN virtual network interface** to
capture IPv4 packets, apply controlled network impairments, schedule
delayed packets, and reinject packets into the Linux networking stack.

The project is designed for controlled testing and academic study of
network reliability, Linux networking, C++ systems programming,
concurrency, probability, and performance measurement.

------------------------------------------------------------------------

## 1. Project Overview

Applications can behave very differently when network conditions change.
Latency, jitter, packet loss, bandwidth limitations, and packet
reordering can affect video calls, games, APIs, cloud services, and
distributed systems.

Testing these conditions on real networks is difficult because
congestion and wireless conditions are not always repeatable. This
project provides a software-based environment where network impairments
can be configured and measured without dedicated network-emulation
hardware.

### Main capabilities

-   Linux TUN interface for Layer-3 packet processing
-   IPv4 packet parsing and protocol identification
-   TCP, UDP, and ICMP filtering
-   Bernoulli packet-loss model
-   Gilbert-Elliott burst-loss model
-   Gaussian and correlated AR(1) jitter
-   Configurable base latency
-   Token-bucket bandwidth shaping
-   Thread-safe minimum-heap delay queue
-   Concurrent packet capture and scheduling
-   IPv4 checksum handling
-   Runtime latency and packet statistics
-   CPU, memory, and thread monitoring
-   Baseline-versus-chaos experiment reporting

------------------------------------------------------------------------

## 2. System Architecture

``` text
Linux Kernel Routing
        |
        v
    TUN tun0
        |
        v
  Capture Thread
        |
        v
  Packet Parser
        |
        v
 Protocol Filter
        |
        v
    Loss Model
        |
        v
 Token-Bucket Shaper
        |
        v
 Delay / Jitter Model
        |
        v
 Thread-Safe DelayQueue
        |
        v
 Scheduler Thread
        |
        v
 IPv4 Checksum Update
        |
        v
   TUN / Kernel
        |
        v
Destination / Test Application
```

Packets enter through the TUN interface, are parsed and processed by the
configured impairment models, placed into the delay queue when required,
and are released by the scheduler at their calculated release time.

------------------------------------------------------------------------

## 3. Project Structure

``` text
network-chaos-emulator/
│
├── CMakeLists.txt
├── README.md
├── config/
│   └── profile.json
│
└── src/
    ├── main.cpp
    ├── chaos/
    │   ├── DelayJitterModel.cpp
    │   ├── DelayJitterModel.h
    │   ├── ImpairmentModel.h
    │   ├── LossModel.cpp
    │   ├── LossModel.h
    │   ├── TokenBucket.cpp
    │   └── TokenBucket.h
    ├── engine/
    │   ├── ChaosEngine.cpp
    │   └── ChaosEngine.h
    ├── metrics/
    │   ├── MetricsCollector.cpp
    │   ├── MetricsCollector.h
    │   ├── SystemMonitor.cpp
    │   └── SystemMonitor.h
    ├── net/
    │   ├── Packet.h
    │   ├── PacketIO.h
    │   ├── PacketParser.cpp
    │   ├── PacketParser.h
    │   ├── TunInterface.cpp
    │   └── TunInterface.h
    ├── report/
    │   ├── ReportGenerator.cpp
    │   └── ReportGenerator.h
    ├── scheduler/
    │   ├── DelayQueue.cpp
    │   └── DelayQueue.h
    └── tests/
        └── test_models.cpp
```

------------------------------------------------------------------------

## 4. Technology Stack

  Technology                  Purpose
  --------------------------- ------------------------------
  C++17                       Core emulator
  Linux / WSL2                Runtime environment
  TUN/TAP                     Virtual network interface
  `ioctl()`                   TUN configuration
  `poll()`                    Packet-read waiting
  `read()` / `write()`        Packet I/O
  `std::thread`               Concurrent processing
  `std::mutex`                Queue synchronization
  `std::condition_variable`   Scheduler notification
  `<random>`                  Loss and jitter models
  `/proc`                     CPU/memory/thread monitoring
  CMake                       Build system
  Git/GitHub                  Version control

------------------------------------------------------------------------

# 5. How to Clone, Build, and Run

The recommended demonstration uses **two WSL/Ubuntu terminals**.

## Terminal 1 --- Clone, Build, and Start the Emulator

If an old copy exists and you want a fresh copy from GitHub:

``` bash
cd ~
rm -rf network-chaos-emulator
git clone https://github.com/Aditya2108cse/network-chaos-emulator.git
cd ~/network-chaos-emulator
```

Install the required build tools if necessary:

``` bash
sudo apt update
sudo apt install -y build-essential cmake
```

### Build using CMake

``` bash
rm -rf build
mkdir build
cd build
cmake ..
cmake --build . -j$(nproc)
```

Start the emulator:

``` bash
sudo ./chaos_emulator
```

For the current project configuration, the emulator starts the TUN
interface and runs a two-phase experiment:

``` text
TUN interface tun0 up at 10.8.0.1
Phase 1/2: BASELINE (30s) -- chaos disabled, generate traffic now
Phase 2/2: CHAOS (30s) -- impairment active, keep sending traffic
```

Keep Terminal 1 running during the experiment.

------------------------------------------------------------------------

## Terminal 2 --- Generate Test Traffic

Open a second WSL/Ubuntu terminal and run:

``` bash
for i in $(seq 1 600); do ping -c 1 -W 1 10.8.0.2 > /dev/null; done
```

This generates ICMP traffic while the emulator is running.

The traffic is used to observe the difference between the baseline and
impaired phases.

------------------------------------------------------------------------

## 6. Direct g++ Build

If you prefer to compile without CMake, run from the repository root:

``` bash
cd ~/network-chaos-emulator

g++ -std=c++17 -Wall -Wextra -O2 -Isrc \
src/main.cpp \
src/net/TunInterface.cpp \
src/net/PacketParser.cpp \
src/chaos/LossModel.cpp \
src/chaos/DelayJitterModel.cpp \
src/chaos/TokenBucket.cpp \
src/scheduler/DelayQueue.cpp \
src/metrics/MetricsCollector.cpp \
src/metrics/SystemMonitor.cpp \
src/report/ReportGenerator.cpp \
src/engine/ChaosEngine.cpp \
-o chaos_emulator -lpthread
```

Then start it:

``` bash
sudo ./chaos_emulator
```

------------------------------------------------------------------------

## 7. Running a Configured Experiment

A configured experiment can use parameters such as:

``` bash
sudo ./chaos_emulator \
--dev tun0 \
--ip 10.8.0.1 \
--duration 60 \
--protocol icmp \
--latency-ms 100 \
--jitter-ms 20 \
--loss-p 5 \
--bandwidth-mbps 10
```

The intended experiment configuration is:

  Parameter            Value
  ------------- ------------
  Duration        60 seconds
  Baseline        30 seconds
  Chaos           30 seconds
  Protocol              ICMP
  Latency             100 ms
  Jitter               20 ms
  Packet loss             5%
  Bandwidth          10 Mbps
  TUN IP            10.8.0.1

Use the command-line options supported by the current executable/build
when changing experiment parameters.

------------------------------------------------------------------------

## 8. Results and Report

After the experiment finishes, the emulator generates:

``` text
chaos_report.txt
```

View it with:

``` bash
cd ~/network-chaos-emulator
cat chaos_report.txt
```

The report contains:

-   Experiment configuration
-   Baseline results
-   Chaos results
-   Packets received/transmitted
-   Packets dropped
-   Packet-loss percentage
-   Average latency
-   Minimum and maximum latency
-   p50, p95, and p99 latency
-   Jitter
-   CPU usage
-   Memory usage
-   Thread count
-   Experiment completion status

A representative live experiment produced approximately:

  Metric               Baseline      Chaos
  ------------------ ---------- ----------
  Average RTT            0.1 ms    96.6 ms
  Minimum RTT            0.1 ms    67.8 ms
  Maximum RTT            0.3 ms   124.9 ms
  Jitter                 0.0 ms    16.2 ms
  Packet Loss              0.0%       3.3%
  Packets Sent               24         30
  Packets Received           24         29
  Packets Lost                0          1
  CPU Usage                0.1%       0.2%
  Memory Usage           3.5 MB     3.6 MB
  Threads                     3          3

The observed values depend on the test environment, traffic volume,
routing, and timing, so they should be treated as representative rather
than fixed results.

------------------------------------------------------------------------

## 9. Network Impairment Models

### Bernoulli Loss

Each packet is independently given a probability of being dropped.

``` text
P(loss) = configured loss probability
```

This is useful for simple random packet-loss experiments.

### Gilbert-Elliott Loss

The model uses good and bad network states to produce correlated or
bursty packet loss.

``` text
       +---------+
       |  GOOD   |
       +---------+
          |   ^
       bad|   |recovery
          v   |
       +---------+
       |   BAD   |
       +---------+
```

### Gaussian Jitter

Generates random delay variation around a configured base delay.

### Correlated AR(1) Jitter

Uses the previous jitter value when generating the next value, allowing
smoother and more correlated changes in network delay.

### Token-Bucket Shaping

Controls the transmission rate by allowing packets to consume tokens at
a configured rate. This provides a software-based way to reproduce
bandwidth limitations.

------------------------------------------------------------------------

## 10. Packet Processing

The general processing pipeline is:

``` text
Packet arrives
      |
      v
Parse IPv4 packet
      |
      v
Check protocol / flow
      |
      v
Apply loss model
      |
      +---- Drop ----> Loss metric
      |
      v
Apply bandwidth shaping
      |
      +---- Excessive wait ----> Shaper metric
      |
      v
Apply delay + jitter
      |
      v
Calculate release time
      |
      v
Insert into DelayQueue
      |
      v
Scheduler releases packet
      |
      v
Update IPv4 checksum
      |
      v
Write packet to TUN
      |
      v
Update metrics
```

------------------------------------------------------------------------

## 11. Metrics and Monitoring

### Network Metrics

-   Packets in/out
-   Bytes in/out
-   Packet loss
-   Model drops
-   Shaper drops
-   Minimum latency
-   Maximum latency
-   Average latency
-   p50 / p95 / p99 latency
-   Jitter

### System Metrics

-   CPU utilization
-   Memory usage
-   Thread count

The system monitor uses Linux `/proc` information, while latency
statistics are accumulated online using Welford's algorithm.

------------------------------------------------------------------------

## 12. Testing and Validation

The project contains independent model tests for important components,
including:

-   Bernoulli loss behavior
-   Gilbert-Elliott loss behavior
-   Gaussian jitter
-   Correlated jitter
-   IPv4 checksum calculation
-   Network byte-order parsing
-   DelayQueue ordering
-   Packet reordering behavior

The live implementation was also tested in WSL2/Linux with the TUN
interface and ICMP traffic.

------------------------------------------------------------------------

## 13. Academic Concepts Demonstrated

### Linux

-   TUN interfaces
-   `ioctl()`
-   `poll()`
-   Linux routing
-   `/proc`
-   Process and thread monitoring

### C++

-   C++17
-   Classes and interfaces
-   RAII
-   Smart pointers
-   STL containers
-   `std::thread`
-   `std::mutex`
-   `std::condition_variable`
-   `std::atomic`
-   `<chrono>`
-   `<random>`

### Computer Networks

-   IPv4 packet structure
-   Protocol identification
-   Network byte order
-   Packet loss
-   Latency
-   Jitter
-   Bandwidth
-   Packet scheduling
-   Packet reordering
-   Checksums

### Probability and Statistics

-   Bernoulli distribution
-   Gaussian distribution
-   Markov-state modeling
-   Correlated random processes
-   Mean and variance
-   Percentiles
-   Statistical validation

------------------------------------------------------------------------

## 14. Limitations

-   The current implementation is based on a TUN interface.
-   Transparent NFQUEUE-based impairment is not implemented.
-   Packet corruption and duplication are not part of the current active
    pipeline.
-   Runtime configuration is primarily command-line based.
-   WSL2 networking can behave differently from native Linux.
-   Live experiments with very small traffic samples provide limited
    statistical confidence.

------------------------------------------------------------------------

## 15. Future Enhancements

Possible extensions include:

-   NFQUEUE support
-   IPv6 support
-   Packet corruption and duplication
-   Per-flow impairment policies
-   JSON-based experiment profiles
-   Prometheus/Grafana monitoring
-   PCAP recording and replay
-   Automated HTTP/TCP/UDP benchmarks
-   Real-time visualization dashboard
-   Containerized deployment

------------------------------------------------------------------------

## 16. Safety Note

This project is intended for **controlled network testing and
educational use**.

Run experiments in WSL2, a virtual machine, container, or isolated
laboratory network. Avoid changing production routing or sending
experimental traffic through production interfaces.

------------------------------------------------------------------------

## 17. Repository

**GitHub:**\
https://github.com/Aditya2108cse/network-chaos-emulator

## Author

**Aditya Krishna**\
B.Tech -- Computer Science & Engineering

**Project Area:** Networking • Linux • C++ • Systems Programming •
Network Resilience

------------------------------------------------------------------------

## Quick Start

### Terminal 1

``` bash
cd ~
rm -rf network-chaos-emulator
git clone https://github.com/Aditya2108cse/network-chaos-emulator.git
cd ~/network-chaos-emulator

rm -rf build
mkdir build
cd build
cmake ..
cmake --build . -j$(nproc)

sudo ./chaos_emulator
```

### Terminal 2

``` bash
for i in $(seq 1 600); do ping -c 1 -W 1 10.8.0.2 > /dev/null; done
```

### View the report

``` bash
cat ~/network-chaos-emulator/chaos_report.txt
```
