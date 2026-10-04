# Network Latency & Packet-Loss Chaos Emulator

A software-only network chaos testing engine written in **C++17 for Linux**. It uses a Linux **TUN virtual network interface** to introduce controlled latency, jitter, packet loss, bandwidth constraints, and packet reordering.

## 1. Overview

The project provides a controlled environment for testing how network applications behave when network conditions become unreliable. Packets are captured from a TUN interface, processed according to the selected impairment models, delayed or dropped when required, and then reinjected into the Linux networking stack.

The implementation is built mainly with Linux system calls and the C++ Standard Library. It is intended for learning, experimentation, and controlled network-resilience testing rather than production routing.

## 2. Main Features

- Linux TUN interface for Layer-3 packet I/O
- IPv4 packet and flow parsing
- Bernoulli and Gilbert-Elliott packet-loss models
- Gaussian and correlated AR(1) delay/jitter models
- Token-bucket bandwidth shaping
- Thread-safe minimum-heap delay queue
- Separate packet capture and scheduler threads
- IPv4 checksum handling
- Packet, byte, loss, latency, and throughput metrics
- p50, p95, and p99 latency statistics
- CPU, memory, and thread monitoring through `/proc`
- Two-phase baseline-versus-chaos experiment reporting

## 3. Architecture

```text
Linux Kernel
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
Loss Model
     |
     v
Delay / Jitter
     |
     v
DelayQueue (thread-safe min-heap)
     |
     v
Scheduler Thread
     |
     v
Checksum + TUN Write
     |
     v
Network Stack / Test Application
```

The capture thread reads packets from the TUN interface while the scheduler releases delayed packets at their calculated release times. The shared delay queue is protected with synchronization primitives so that capture and scheduling can operate concurrently.

## 4. Technology Stack

| Technology | Purpose |
|---|---|
| C++17 | Core implementation |
| Linux / WSL2 | Runtime environment |
| TUN/TAP | Virtual network interface |
| `ioctl()` | TUN configuration |
| `poll()` | Packet-read waiting |
| `read()` / `write()` | Packet I/O |
| `std::thread` | Concurrent workers |
| `std::mutex` / `condition_variable` | Queue synchronization |
| `std::atomic` | Thread-safe counters |
| `<random>` | Loss and jitter models |
| `/proc` | Resource monitoring |
| CMake / g++ | Build |
| Git/GitHub | Version control |

## 5. Project Structure

```text
network-chaos-emulator/
├── CMakeLists.txt
├── README.md
├── .gitignore
├── config/
│   └── profile.json
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

## 6. Requirements

- Linux or WSL2 with a working Linux environment
- `g++` with C++17 support
- Linux TUN/TAP support
- pthread support
- Root privileges or suitable `CAP_NET_ADMIN` capability for TUN configuration
- CMake is optional

The project was developed and tested in **WSL2**.

## 7. Build

### Direct g++ build

```bash
g++ -std=c++17 -Wall -Wextra -O2 -Isrc \
  src/main.cpp \
  src/net/TunInterface.cpp \
  src/net/PacketParser.cpp \
  src/chaos/LossModel.cpp \
  src/chaos/DelayJitterModel.cpp \
  src/scheduler/DelayQueue.cpp \
  src/metrics/MetricsCollector.cpp \
  src/engine/ChaosEngine.cpp \
  -o chaos_emulator -lpthread
```

### CMake

```bash
mkdir -p build
cd build
cmake ..
make
```

## 8. Running the Emulator

Example configuration:

```bash
sudo ./chaos_emulator \
  --dev tun0 \
  --ip 10.8.0.1 \
  --loss-model gilbert-elliott \
  --delay-model correlated \
  --base-delay-ms 50 \
  --jitter-ms 10
```

Traffic must be routed through the test interface before the emulator can process it. For example:

```bash
sudo ip route add <test-target>/32 dev tun0
ping <test-target>
```

For throughput testing:

```bash
iperf3 -c <test-target>
```

## 9. Impairment Models

### Packet loss

**Bernoulli loss** treats packet drops as approximately independent random events. For example, `loss-p = 0.05` gives each packet an approximately 5% drop probability.

**Gilbert-Elliott loss** uses GOOD and BAD states to produce correlated, burst-like packet loss.

### Delay and jitter

The **Gaussian model** generates independent delay variation around a configured base delay.

The **AR(1) model** uses the previous jitter value when generating the next one, producing more correlated and gradual changes in latency.

### Packet scheduling

Packets are stored in a minimum-heap according to their calculated release time. If a later packet receives a shorter delay, it may be released before an earlier packet, producing packet reordering.

## 10. Metrics and Testing

The metrics subsystem records:

- Packets and bytes in/out
- Total and impairment-related drops
- Minimum, maximum, and mean latency
- Standard deviation
- p50, p95, and p99 latency
- CPU, memory, and thread count

Welford's online algorithm is used for incremental mean and variance calculation.

The model test program validates components that do not require a live TUN device:

```bash
g++ -std=c++17 -Wall -Wextra -O2 -Isrc \
  src/tests/test_models.cpp \
  src/chaos/LossModel.cpp \
  src/chaos/DelayJitterModel.cpp \
  src/net/PacketParser.cpp \
  src/scheduler/DelayQueue.cpp \
  -o test_models -lpthread

./test_models
```

The validated build reported **12 passing assertions**, covering loss models, delay models, checksum calculation, byte-order parsing, queue ordering, and packet reordering.

## 11. Example Experiment

A representative 60-second experiment used ICMP traffic with:

| Parameter | Configured | Observed |
|---|---:|---:|
| Latency / RTT | 100 ms | 104.1 ms |
| Jitter | 20 ms | 16.4 ms |
| Packet loss | 5.0% | 6.7% |
| Bandwidth cap | 10 Mbps | — |

Observed values can differ from configured impairment values because the measurement also depends on packet timing, routing, and the test environment.

## 12. Limitations

The current implementation has several known limitations:

- TUN-only operation
- No NFQUEUE mode for transparent physical-interface traffic
- Runtime configuration is currently CLI-based; `config/profile.json` documents the intended structure
- Packet corruption and duplication are not part of the active pipeline
- No HTTP endpoint for the Prometheus-style metrics output
- TUN configuration and routing require appropriate Linux privileges
- WSL2 networking can behave differently from native Linux

## 13. Academic / Training Concepts

The project brings together:

- **Linux:** TUN, `ioctl`, `poll`, `/proc`, signals, and process/thread monitoring
- **C++:** classes, interfaces, RAII, smart pointers, STL, threads, mutexes, condition variables, atomics, `chrono`, and `random`
- **Computer Networks:** IPv4, protocols, ports, latency, jitter, packet loss, throughput, reordering, and network byte order
- **Computer Architecture:** memory representation, endianness, checksums, and hardware/software boundaries
- **Probability and Statistics:** Bernoulli distributions, Gaussian distributions, Markov chains, correlated processes, variance, and percentiles
- **Concurrency:** independent capture and scheduling workers with a synchronized shared queue

## 14. Safety

This project is intended for **controlled testing and educational use**. Changes to routes or experimental network interfaces can affect connectivity. Testing should preferably be performed in WSL2, a virtual machine, a container, or an isolated laboratory network.

Avoid applying experimental routing rules to production interfaces unless their effects are fully understood.

## 15. Author

**Aditya Krishna**  
B.Tech – Computer Science & Engineering

Project area: **Networking • Linux • C++ • Systems Programming • Network Resilience**

GitHub:  
https://github.com/Aditya2108cse

Repository:  
https://github.com/Aditya2108cse/network-chaos-emulator
