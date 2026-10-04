# Network Latency & Packet-Loss Chaos Emulator

> A software-only network chaos testing engine built with **C++17 for Linux** that reproduces controlled latency, jitter, packet loss, bandwidth constraints, and packet reordering through a virtual TUN network interface.

---

## 1. Project Overview

The **Network Latency & Packet-Loss Chaos Emulator** is a systems and networking project designed to simulate unreliable network conditions in a controlled and repeatable software environment.

The emulator captures IPv4 packets through a Linux **TUN virtual network interface**, applies configurable network impairments, schedules delayed packets, and reinjects the packets into the Linux networking stack.

Instead of requiring dedicated network-emulation hardware or deliberately creating poor conditions on a physical network, the project provides a software-based environment for reproducing conditions such as:

- High network latency
- Random packet loss
- Bursty packet loss
- Network jitter
- Packet reordering
- Bandwidth limitations
- Delayed packet delivery

The implementation uses **C++17**, Linux system interfaces, multithreading, synchronization primitives, probabilistic models, packet parsing, scheduling, and runtime resource monitoring.

The system is intended for **controlled testing, experimentation, and academic learning**, rather than as a replacement for production routing infrastructure.

---

## 2. Aim of the Project

The main aim is to develop a software-based network chaos testing system that can intentionally introduce controlled network impairments and measure their effects on network traffic.

The project combines concepts from:

- Computer Networks
- Linux systems programming
- Operating Systems
- C++ programming
- Concurrent programming
- Probability and Statistics
- Computer Architecture

The emulator provides a practical environment for studying how applications and network protocols behave when network conditions become unreliable.

---

## 3. Problem Statement

Modern applications are expected to remain usable even when network conditions are unstable. However, testing application behavior under controlled network impairment is difficult.

Real-world networks are affected by factors such as:

- Network congestion
- Weak wireless signals
- Mobile-network fluctuations
- Long-distance communication delays
- Router queue overflow
- Temporary packet loss
- Variable latency
- Bandwidth limitations

These conditions can change over time and may be difficult to reproduce consistently. Dedicated network impairment hardware can provide better control, but it may be expensive or unavailable in an academic environment.

The proposed emulator addresses this problem by providing a controlled software environment where specific network conditions can be configured, reproduced, and measured.

---

## 4. Objectives

The project has the following objectives:

1. Create and configure a Linux TUN virtual network interface.
2. Capture IPv4 packets from the virtual interface.
3. Parse packet headers and identify protocol and flow information.
4. Support TCP, UDP, ICMP, and configurable traffic filtering.
5. Implement configurable packet-loss models.
6. Implement configurable latency and jitter models.
7. Apply bandwidth limitations using token-bucket shaping.
8. Schedule delayed packets using a thread-safe minimum-heap queue.
9. Separate packet capture and packet release using concurrent threads.
10. Reinject processed packets into the Linux network stack.
11. Collect packet, byte, loss, latency, and throughput statistics.
12. Monitor CPU, memory, and thread usage through `/proc`.
13. Validate statistical models independently before live network testing.
14. Generate reproducible experiment measurements and reports.

---

## 5. Key Features

### 5.1 Linux TUN Interface

The emulator uses the Linux **TUN/TAP infrastructure** and currently operates at the IP layer using a TUN interface.

A TUN interface provides a software-defined Layer-3 interface between the Linux kernel networking stack and the user-space emulator.

The implementation uses Linux system calls such as:

- `ioctl()`
- `poll()`
- `read()`
- `write()`

---

### 5.2 IPv4 Packet Parsing

The packet parser extracts information from IPv4 packets, including:

- Source IP address
- Destination IP address
- Protocol
- Source port, where available
- Destination port, where available

Network-byte-order conversion is handled using standard networking functions.

The emulator can therefore make impairment decisions based on packet protocol and flow information.

---

### 5.3 Packet-Loss Models

Two packet-loss strategies are implemented.

#### Bernoulli Loss

The Bernoulli model treats packet loss approximately as an independent random event for each packet.

For example:

```text
loss-p = 0.05
```

means that each packet has approximately a 5% probability of being dropped.

This model is useful for:

- Simple experiments
- Baseline testing
- Controlled random loss

#### Gilbert-Elliott Loss

The Gilbert-Elliott model uses two states:

```text
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

This produces correlated packet loss and can create bursts of packet drops rather than isolated random losses.

---

### 5.4 Delay and Jitter Models

Two delay/jitter models are implemented.

#### Gaussian Delay/Jitter

The Gaussian model generates delay variation around a configured base delay using a normal distribution.

This represents independently varying latency.

#### Correlated AR(1) Jitter

The AR(1) model uses the previous jitter value when generating the next value.

Conceptually:

```text
J(t) = alpha × J(t-1) + random_component
```

A higher correlation factor produces smoother changes between consecutive packets.

This model is useful for representing network conditions where latency changes gradually instead of independently for every packet.

---

### 5.5 Token-Bucket Bandwidth Shaping

The emulator includes a token-bucket bandwidth shaper.

The shaper controls the rate at which packets can continue through the processing pipeline. A configurable maximum waiting behavior prevents packets from remaining in the shaping stage indefinitely.

This allows bandwidth-constrained experiments to be performed without requiring external traffic-shaping hardware.

---

### 5.6 DelayQueue and Packet Reordering

Delayed packets are stored in a thread-safe **minimum-heap priority queue** according to their calculated release time.

For example:

```text
Packet A -> release at 150 ms
Packet B -> release at 120 ms
Packet C -> release at 180 ms
```

The scheduler releases:

```text
B -> A -> C
```

Although packet A entered the queue before packet B, packet B has an earlier release time.

Therefore, variable delay can naturally produce **packet reordering**, which is an important network behavior for applications and protocols to handle.

---

### 5.7 Multithreaded Processing

The emulator separates packet capture and packet release into concurrent workers.

Main execution components:

- Capture/processing thread
- Packet scheduler/reinjection thread

The capture thread continues reading packets while previously captured packets are waiting for their scheduled release time.

The two components communicate through a thread-safe delay queue protected using synchronization primitives.

This prevents delayed packets from unnecessarily blocking packet capture.

---

### 5.8 IPv4 Checksum Handling

The project can recompute the IPv4 header checksum after packet processing.

This demonstrates packet-level processing that is normally associated with networking hardware and checksum offloading mechanisms.

---

### 5.9 Runtime Metrics

The metrics subsystem records network and system information such as:

#### Traffic metrics

- Packets received
- Packets transmitted
- Bytes received
- Bytes transmitted

#### Loss metrics

- Total drops
- Model-based drops
- Bandwidth/shaper-related drops

#### Latency metrics

- Minimum latency
- Maximum latency
- Mean latency
- Standard deviation
- p50
- p95
- p99

#### System metrics

- CPU usage
- Memory usage
- Thread count

The implementation uses **Welford's online algorithm** for incremental calculation of mean and variance without keeping the complete set of latency measurements in memory.

---

## 6. System Architecture

The high-level processing architecture is:

```text
                    Linux Kernel Routing
                            |
                            v
                      +-----------+
                      | TUN tun0  |
                      +-----------+
                            |
                            v
                    +---------------+
                    | Capture Thread|
                    | poll() + read |
                    +---------------+
                            |
                            v
                    +---------------+
                    | Packet Parser |
                    | IPv4 / Flow   |
                    +---------------+
                            |
                            v
                    +---------------+
                    | Protocol      |
                    | Filtering     |
                    +---------------+
                            |
                            v
                    +---------------+
                    | Loss Model    |
                    | Bernoulli / GE|
                    +---------------+
                            |
                            v
                    +---------------+
                    | Token Bucket  |
                    | Bandwidth     |
                    +---------------+
                            |
                            v
                    +---------------+
                    | Delay / Jitter|
                    | Gaussian/AR(1)|
                    +---------------+
                            |
                            v
                    +---------------+
                    | DelayQueue    |
                    | Thread-safe   |
                    | Min-Heap      |
                    +---------------+
                            |
                            v
                    +---------------+
                    | Scheduler     |
                    | Thread        |
                    +---------------+
                            |
                            v
                    +---------------+
                    | IPv4 Checksum |
                    +---------------+
                            |
                            v
                    +---------------+
                    | TUN Write     |
                    | / Kernel      |
                    +---------------+
                            |
                            v
                 Destination / Test Application
```

---

## 7. Packet Processing Pipeline

For each packet, the general processing sequence is:

```text
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
      +---- Drop ----> Update loss metric
      |
      v
Apply token-bucket shaping
      |
      +---- Excessive wait ----> Update shaper-drop metric
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
Scheduler waits
      |
      v
Release packet
      |
      v
Update/check IPv4 checksum
      |
      v
Write packet to TUN
      |
      v
Update runtime metrics
```

---

## 8. Technology Stack

| Technology | Purpose |
|---|---|
| C++17 | Core emulator implementation |
| Linux | Operating-system and networking environment |
| WSL2 | Development/testing environment |
| TUN/TAP | Virtual network interface |
| `ioctl()` | TUN interface configuration |
| `poll()` | Efficient packet-read waiting |
| `read()` / `write()` | Packet input/output |
| `std::thread` | Concurrent capture and scheduling |
| `std::mutex` | Shared-data synchronization |
| `std::condition_variable` | Scheduler notification |
| `std::atomic` | Thread-safe counters |
| `<random>` | Statistical loss/jitter generation |
| `/proc` | CPU/memory/thread monitoring |
| CMake | Optional build system |
| g++ | C++ compiler |
| Git/GitHub | Version control and hosting |

---

## 9. Project Structure

```text
network-chaos-emulator/
│
├── CMakeLists.txt
├── README.md
├── .gitignore
│
├── config/
│   └── profile.json
│
└── src/
    ├── main.cpp
    │
    ├── chaos/
    │   ├── DelayJitterModel.cpp
    │   ├── DelayJitterModel.h
    │   ├── ImpairmentModel.h
    │   ├── LossModel.cpp
    │   ├── LossModel.h
    │   ├── TokenBucket.cpp
    │   └── TokenBucket.h
    │
    ├── engine/
    │   ├── ChaosEngine.cpp
    │   └── ChaosEngine.h
    │
    ├── metrics/
    │   ├── MetricsCollector.cpp
    │   ├── MetricsCollector.h
    │   ├── SystemMonitor.cpp
    │   └── SystemMonitor.h
    │
    ├── net/
    │   ├── Packet.h
    │   ├── PacketIO.h
    │   ├── PacketParser.cpp
    │   ├── PacketParser.h
    │   ├── TunInterface.cpp
    │   └── TunInterface.h
    │
    ├── report/
    │   ├── ReportGenerator.cpp
    │   └── ReportGenerator.h
    │
    ├── scheduler/
    │   ├── DelayQueue.cpp
    │   └── DelayQueue.h
    │
    └── tests/
        └── test_models.cpp
```

---

## 10. Requirements

### Minimum requirements

- Linux or WSL2 with a working Linux environment
- `g++` with C++17 support
- Linux TUN/TAP support
- pthread support
- Root privileges or appropriate `CAP_NET_ADMIN` capability for TUN configuration
- CMake is optional

### Recommended environment

The project was developed and tested in **WSL2/Linux**.

For reliable experiments, an isolated Linux, WSL2, virtual machine, container, or laboratory network is recommended.

---

# 11. How to Run the Project

## Step 1: Clone the repository

```bash
git clone git@github.com:Aditya2108cse/network-chaos-emulator.git
cd network-chaos-emulator
```

If HTTPS is preferred:

```bash
git clone https://github.com/Aditya2108cse/network-chaos-emulator.git
cd network-chaos-emulator
```

---

## Step 2: Check the Linux environment

Verify the compiler:

```bash
g++ --version
```

Check that C++17 is available:

```bash
g++ -std=c++17 --version
```

Check TUN support:

```bash
ls -l /dev/net/tun
```

If the TUN device does not exist, load the module where supported:

```bash
sudo modprobe tun
```

Then verify again:

```bash
ls -l /dev/net/tun
```

---

## Step 3: Build the emulator

### Option A: Direct g++ build

From the repository root:

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

Check that the executable was created:

```bash
ls -l chaos_emulator
```

---

## Step 4: Run the model tests

The model tests do not require a live TUN interface.

Build the test program:

```bash
g++ -std=c++17 -Wall -Wextra -O2 -Isrc \
  src/tests/test_models.cpp \
  src/chaos/LossModel.cpp \
  src/chaos/DelayJitterModel.cpp \
  src/net/PacketParser.cpp \
  src/scheduler/DelayQueue.cpp \
  -o test_models -lpthread
```

Run:

```bash
./test_models
```

The validated build contains **12 assertions**, covering model behavior, checksum calculation, byte-order parsing, queue ordering, and packet reordering.

---

## Step 5: Build using CMake

If using CMake:

```bash
mkdir -p build
cd build
cmake ..
make
```

After building:

```bash
ls
```

The executable should be available in the build directory according to the project's CMake configuration.

---

## Step 6: Run the emulator

A typical configuration is:

```bash
sudo ./chaos_emulator \
  --dev tun0 \
  --ip 10.8.0.1 \
  --loss-model gilbert-elliott \
  --delay-model correlated \
  --base-delay-ms 50 \
  --jitter-ms 10
```

Example parameters:

```text
--dev tun0
--ip 10.8.0.1
--loss-model gilbert-elliott
--delay-model correlated
--base-delay-ms 50
--jitter-ms 10
```

The exact available command-line options depend on the current implementation.

---

## Step 7: Configure the test route

Traffic must be routed through the test interface before the emulator can process it.

Example:

```bash
sudo ip route add <test-target>/32 dev tun0
```

Replace `<test-target>` with the address of the controlled test destination.

Verify the route:

```bash
ip route
```

Check the interface:

```bash
ip addr show tun0
```

---

## Step 8: Generate test traffic

### ICMP / ping

```bash
ping <test-target>
```

This is useful for testing:

- Latency
- Packet loss
- Jitter
- Basic packet capture/reinjection

### Throughput testing with iperf3

On the test server:

```bash
iperf3 -s
```

On the test client:

```bash
iperf3 -c <test-target>
```

This can be used to observe the effect of bandwidth constraints and network impairments.

---

## 12. Command-Line Parameters

Typical parameters include:

| Parameter | Example Default | Description |
|---|---:|---|
| `--dev` | `tun0` | TUN device name |
| `--ip` | `10.8.0.1` | IP assigned to TUN |
| `--loss-model` | `gilbert-elliott` | Packet-loss model |
| `--loss-p` | `0.05` | Bernoulli loss probability |
| `--delay-model` | `correlated` | Delay/jitter model |
| `--base-delay-ms` | `50` | Base delay |
| `--jitter-ms` | `10` | Jitter magnitude |
| `--alpha` | `0.85` | AR(1) correlation coefficient |
| `--report-interval` | `5` | Metrics reporting interval |

---

## 13. Example Experiment

A representative experiment can be configured with:

```text
Duration:              60 seconds
Traffic:               ICMP
Configured latency:    100 ms
Configured jitter:      20 ms
Configured loss:         5%
Bandwidth cap:          10 Mbps
```

Example observed values from project validation:

| Metric | Configured | Observed |
|---|---:|---:|
| Latency / RTT | 100 ms | 104.1 ms |
| Jitter | 20 ms | 16.4 ms |
| Packet loss | 5.0% | 6.7% |

The observed values do not necessarily equal the configured impairment values because the measurement is also affected by packet timing, routing, the operating environment, and the measurement method.

---

## 14. Testing and Validation

The project separates statistical/model testing from live network testing.

### Automated model validation

The test suite covers:

- Bernoulli loss probability
- Gilbert-Elliott burst behavior
- Gaussian delay behavior
- Correlated jitter behavior
- IPv4 checksum calculation
- Network byte-order parsing
- DelayQueue release ordering
- Packet reordering behavior

This allows core components to be tested without requiring a working TUN interface.

### Live network validation

The project was validated in WSL2/Linux by checking:

- Creation and configuration of `tun0`
- ICMP packet capture
- Packet reinjection
- Configured latency behavior
- Packet-loss behavior
- Runtime network metrics

---

## 15. Metrics and Monitoring

The emulator records network and system behavior during execution.

### Packet metrics

```text
packets_in
packets_out
bytes_in
bytes_out
```

### Loss metrics

```text
total_drops
model_drops
shaper_drops
```

### Latency metrics

```text
min
max
mean
standard deviation
p50
p95
p99
```

### System metrics

```text
CPU usage
Memory usage
Thread count
```

Linux `/proc` information is used for process and resource monitoring.

Welford's algorithm is used to calculate mean and variance incrementally.

---

## 16. Where the Main Concepts Are Used

| Concept | Project Component | Purpose |
|---|---|---|
| Linux TUN | `TunInterface` | Virtual network interface |
| `ioctl()` | `TunInterface.cpp` | TUN configuration |
| `poll()` | `TunInterface.cpp` | Packet-read waiting |
| IPv4 parsing | `PacketParser` | Packet/flow information |
| Endianness | `PacketParser` | Network/host byte conversion |
| Checksum | `PacketParser` | IPv4 header validation |
| Threads | `ChaosEngine` | Capture/scheduler separation |
| Mutex | `DelayQueue` | Shared queue protection |
| Condition variable | `DelayQueue` | Scheduler wake-up |
| Atomics | `MetricsCollector` | Thread-safe counters |
| Strategy pattern | Loss/Delay models | Extensible impairment models |
| Bernoulli model | `LossModel` | Independent packet loss |
| Markov chain | Gilbert-Elliott | Bursty packet loss |
| Gaussian distribution | Delay model | Random jitter |
| AR(1) model | Delay model | Correlated jitter |
| Min-heap | `DelayQueue` | Time-ordered release |
| Welford's algorithm | Metrics | Online mean/variance |
| `/proc` | `SystemMonitor` | CPU/memory/thread monitoring |
| RAII | TUN/file descriptors | Resource cleanup |
| Smart pointers | Engine/model ownership | Memory management |

---

## 17. Academic / Training Concept Coverage

### Linux

The project uses:

- TUN device management
- `ioctl`
- `poll`
- `/proc`
- Signals
- Process and thread monitoring
- Linux routing

### C++

The implementation uses:

- Classes and interfaces
- RAII
- Smart pointers
- Templates
- STL containers
- `std::thread`
- `std::mutex`
- `std::condition_variable`
- `std::atomic`
- `<chrono>`
- `<random>`

### Computer Networks

The project demonstrates:

- IPv4 packets
- IPv4 headers
- Protocol identification
- Ports
- Packet loss
- Latency
- Jitter
- Throughput
- Packet reordering
- Network byte order

### Computer Architecture

The project demonstrates:

- Memory representation of packet headers
- Endianness
- Software checksum processing
- Hardware checksum-offload concepts
- Hardware/software networking boundaries

### Probability and Statistics

The project uses:

- Bernoulli distributions
- Gaussian distributions
- Markov chains
- Correlated random processes
- Mean and variance
- Percentiles
- Statistical validation

---

## 18. Limitations

The current implementation has several limitations:

### TUN-only operation

The emulator currently requires traffic to pass through the configured TUN interface.

### No NFQUEUE mode

Transparent impairment of existing physical-interface traffic is not currently implemented.

### CLI-based configuration

`config/profile.json` documents the intended configuration structure, but runtime configuration is currently controlled through command-line parameters.

### Additional impairment models

Packet corruption and packet duplication are not currently part of the active processing pipeline.

### Metrics endpoint

The metrics collector supports Prometheus-style text generation, but an HTTP metrics endpoint has not yet been implemented.

### Network setup dependency

TUN configuration and routing require appropriate Linux networking privileges.

### WSL2 considerations

WSL2 networking can differ from native Linux, particularly around routing, interface visibility, and privileged networking operations.

---

## 19. Advantages

### Software-only

No dedicated network-emulation hardware is required.

### Repeatable

The same impairment configuration can be executed repeatedly.

### Configurable

Network conditions can be changed using command-line parameters.

### Lightweight

The implementation primarily uses Linux facilities and the C++ Standard Library rather than a large external networking framework.

### Educational

The project combines networking, Linux internals, C++, concurrency, probability, statistics, and systems programming.

### Extensible

The strategy-based model architecture allows additional loss and delay models to be introduced without redesigning the complete engine.

### Measurable

The system does not only introduce network impairments; it also records the resulting network and system behavior.

---

## 20. Safety and Testing Note

This project is intended for **controlled network testing and educational use**.

Changing system routes or attaching experimental network interfaces can affect connectivity. Testing should preferably be performed inside:

- WSL2
- Virtual machines
- Containers
- Isolated laboratory networks
- Dedicated test environments

Avoid applying experimental routing rules to production interfaces unless the configuration and consequences are fully understood.

---

## 21. Repository Usage

Clone:

```bash
git clone https://github.com/Aditya2108cse/network-chaos-emulator.git
cd network-chaos-emulator
```

Build:

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

Run model tests:

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

Run the emulator:

```bash
sudo ./chaos_emulator \
  --dev tun0 \
  --ip 10.8.0.1 \
  --loss-model gilbert-elliott \
  --delay-model correlated \
  --base-delay-ms 50 \
  --jitter-ms 10
```

---

## 22. Author

**Aditya Krishna**

B.Tech – Computer Science & Engineering

**Project Area:**  
Networking • Linux • C++ • Systems Programming • Network Resilience

**GitHub:**  
https://github.com/Aditya2108cse

**Repository:**  
https://github.com/Aditya2108cse/network-chaos-emulator

---

## 23. Conclusion

The **Network Latency & Packet-Loss Chaos Emulator** provides a practical software-based environment for reproducing degraded network conditions.

The project combines Linux networking, TUN interfaces, C++17 systems programming, multithreading, probabilistic impairment models, packet parsing, packet scheduling, bandwidth shaping, metrics collection, and statistical testing into a single application.

The main value of the project is not simply introducing packet loss or delay. It demonstrates how a network impairment engine can be **designed, implemented, measured, tested, and extended using low-level operating-system and networking facilities**.

The current implementation provides a foundation for future extensions such as transparent NFQUEUE processing, more advanced bandwidth shaping, packet corruption and duplication, configuration files, monitoring APIs, and a real-time visualization dashboard.
