# Network Latency & Packet-Loss Chaos Emulator

> **A software-only network chaos testing engine built in C++17 for
> Linux that reproduces latency, jitter, packet loss, bandwidth
> constraints, and packet reordering through a virtual TUN network
> interface.**

![Language](https://img.shields.io/badge/C%2B%2B-17-blue)
![Platform](https://img.shields.io/badge/Platform-Linux-orange)
![Network](https://img.shields.io/badge/Network-TUN%2FIP-green)
![Build](https://img.shields.io/badge/Build-CMake%20%2F%20g%2B%2B-lightgrey)
![Status](https://img.shields.io/badge/Status-Working-success)

## 1. Project Overview

The **Network Latency & Packet-Loss Chaos Emulator** is a systems and
networking project designed to simulate unreliable network conditions in
a controlled and repeatable software environment.

The emulator captures IP packets through a Linux **TUN virtual network
interface**, applies configurable network impairments, and reinjects the
packets back into the Linux networking stack.

Instead of requiring special network hardware or physically creating a
poor network connection, the project allows developers and students to
reproduce conditions such as:

-   High network latency
-   Random packet loss
-   Bursty packet loss
-   Network jitter
-   Packet reordering
-   Bandwidth constraints
-   Delayed packet delivery

The project is implemented using **C++17**, Linux system interfaces,
multithreading, synchronization primitives, probabilistic models, and
packet-processing techniques.

------------------------------------------------------------------------

## 2. Aim of the Project

The main aim of this project is to develop a **software-based network
chaos testing system** that can intentionally introduce controlled
network impairments and measure their effects on network traffic.

The project demonstrates how operating-system facilities, networking
concepts, C++ systems programming, concurrency, and statistical models
can be combined to build a practical network-testing tool.

### Main objectives

1.  Create a virtual network interface using Linux TUN.
2.  Capture IP packets from the virtual interface.
3.  Identify packet protocol and flow information.
4.  Apply configurable packet-loss models.
5.  Apply configurable latency and jitter models.
6.  Schedule delayed packets using a thread-safe priority queue.
7.  Reinject processed packets into the network stack.
8.  Collect packet, byte, latency, loss, and throughput statistics.
9.  Validate probabilistic models using automated tests.
10. Provide a reproducible environment for network resilience testing.

------------------------------------------------------------------------

## 3. Problem Statement

Modern applications are expected to remain reliable even when network
conditions are unstable.

Real-world networks can experience:

-   Congestion
-   Weak wireless signals
-   Mobile-network fluctuations
-   Long-distance communication delays
-   Router queue overflow
-   Temporary packet loss
-   Variable latency
-   Bandwidth limitations

Testing these conditions on a physical network can be difficult because
the behavior is often unpredictable and difficult to reproduce.

The proposed emulator addresses this problem by providing a controlled
software environment in which network impairments can be configured and
repeated without requiring dedicated network-emulation hardware.

------------------------------------------------------------------------

## 4. Why This Project Is Useful

The emulator can be used to test how applications behave under degraded
network conditions.

Potential use cases include:

### Web and API applications

Test request timeouts, retries, connection handling, and service
behavior when latency or packet loss increases.

### Video conferencing

Study how applications react to jitter, packet loss, and unstable
latency.

### Online games

Evaluate the effects of latency, jitter, packet loss, and packet
reordering on real-time communication.

### Distributed systems

Test service-to-service communication under unreliable network
conditions.

### IoT and embedded systems

Simulate unstable wireless or low-bandwidth communication.

### Network software development

Validate timeout, retry, buffering, and fault-tolerance mechanisms.

### Academic and laboratory work

Demonstrate Linux networking, packet processing, concurrency,
probability models, and systems programming.

------------------------------------------------------------------------

## 5. Key Features

### Virtual network interface

Uses Linux **TUN/TAP infrastructure** to create a software-defined
network interface.

The emulator currently operates at the IP layer using a TUN interface.

### Configurable packet loss

Two loss models are implemented:

-   **Bernoulli loss**
-   **Gilbert-Elliott burst loss**

Bernoulli loss treats packet loss approximately as independent random
events.

Gilbert-Elliott loss uses a two-state Markov model to represent more
realistic **bursty packet loss**.

### Configurable latency and jitter

Two delay models are implemented:

-   **Gaussian delay/jitter**
-   **Correlated AR(1) delay/jitter**

The correlated model maintains statistical memory between consecutive
packets, allowing latency to vary more smoothly.

### Delay scheduling

Delayed packets are stored in a thread-safe **min-heap priority queue**.

Packets are released according to their calculated release time rather
than their insertion order.

This also allows the emulator to naturally demonstrate **packet
reordering**.

### Multithreaded architecture

Two main execution threads are used:

-   Capture/processing thread
-   Packet scheduler/reinjection thread

This prevents delayed packets from blocking packet capture.

### Packet parsing

The project parses IPv4 packet headers and extracts flow information
such as:

-   Source IP
-   Destination IP
-   Protocol
-   Source port
-   Destination port

Network-byte-order conversion is handled using standard networking
functions.

### IP checksum handling

The project can recompute the IPv4 header checksum after packet
processing.

This demonstrates work that may normally be handled by network hardware
checksum offloading.

### Metrics collection

The metrics subsystem tracks information such as:

-   Packets received
-   Packets transmitted
-   Bytes processed
-   Packets dropped
-   Loss-related drops
-   Bandwidth-related drops
-   Latency samples
-   Minimum latency
-   Maximum latency
-   Mean latency
-   Standard deviation
-   Percentiles

### System monitoring

Linux `/proc` information is used to monitor the emulator's:

-   CPU usage
-   Memory usage
-   Thread count

### Experiment reporting

The project can produce a report containing network-performance
measurements and experiment statistics.

------------------------------------------------------------------------

## 6. System Architecture

``` text
                  Linux Kernel
                       |
                       v
                +--------------+
                |   TUN tun0   |
                +--------------+
                       |
                       v
              +-------------------+
              | Capture Thread    |
              | poll() + read()   |
              +-------------------+
                       |
                       v
              +-------------------+
              | Packet Parser     |
              | IPv4 / Flow Info  |
              +-------------------+
                       |
                       v
              +-------------------+
              | Loss Model        |
              | Bernoulli / GE    |
              +-------------------+
                       |
                       v
              +-------------------+
              | Delay/Jitter      |
              | Gaussian / AR(1)  |
              +-------------------+
                       |
                       v
              +-------------------+
              | DelayQueue        |
              | Thread-safe Heap  |
              +-------------------+
                       |
                       v
              +-------------------+
              | Scheduler Thread  |
              | Timed Release     |
              +-------------------+
                       |
                       v
              +-------------------+
              | Checksum + Write  |
              | back to TUN        |
              +-------------------+
                       |
                       v
                  Network Stack
```

------------------------------------------------------------------------

## 7. Processing Pipeline

For each packet, the emulator follows this general pipeline:

``` text
Packet arrives
      |
      v
Parse IPv4 packet
      |
      v
Check protocol/flow
      |
      v
Apply loss model
      |
      +---- Drop ----> Record metric
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
Recalculate/check IP checksum
      |
      v
Write packet to TUN
      |
      v
Record metrics
```

------------------------------------------------------------------------

## 8. Technology Stack

  Technology                  Purpose
  --------------------------- ---------------------------------------------
  C++17                       Core emulator implementation
  Linux                       Operating-system and networking environment
  TUN/TAP                     Virtual network interface
  POSIX/Linux system calls    Device and packet I/O
  `ioctl()`                   TUN interface configuration
  `poll()`                    Efficient packet-read waiting
  `read()` / `write()`        Packet input/output
  `std::thread`               Concurrent capture and scheduling
  `std::mutex`                Shared-data synchronization
  `std::condition_variable`   Scheduler notification/wakeup
  `std::atomic`               Thread-safe metrics/counters
  `<random>`                  Statistical loss and jitter generation
  CMake                       Optional build system
  g++                         C++ compiler
  `/proc`                     Linux process/resource monitoring
  Git/GitHub                  Version control and project hosting

------------------------------------------------------------------------

## 9. Where Each Concept Is Used

  -----------------------------------------------------------------------
  Concept                 Implementation          Purpose
  ----------------------- ----------------------- -----------------------
  Linux TUN               `TunInterface`          Creates virtual network
                                                  interface

  `ioctl()`               `TunInterface.cpp`      Configures TUN device

  `poll()`                `TunInterface.cpp`      Waits efficiently for
                                                  packets

  Raw packet processing   `PacketParser`          Reads IP header
                                                  information

  Endianness              `PacketParser`          Converts network/host
                                                  byte order

  Checksum                `PacketParser`          Maintains valid IPv4
                                                  headers

  Threads                 `ChaosEngine`           Separates capture and
                                                  scheduling

  Mutex                   `DelayQueue`            Protects shared queue

  Condition variable      `DelayQueue`            Wakes scheduler when
                                                  work arrives

  Atomics                 `MetricsCollector`      Thread-safe counters

  Strategy pattern        Loss/Delay interfaces   Allows different models

  Bernoulli model         `LossModel`             Independent packet loss

  Markov chain            Gilbert-Elliott model   Bursty packet loss

  Gaussian distribution   Delay model             Independent random
                                                  jitter

  AR(1) model             Delay model             Correlated jitter

  Min-heap                `DelayQueue`            Time-ordered packet
                                                  release

  Welford's algorithm     Metrics                 Online mean/variance

  `/proc`                 `SystemMonitor`         CPU/memory/thread
                                                  monitoring

  RAII                    TUN/file-descriptor     Safe resource cleanup
                          management              

  Smart pointers          Engine/model ownership  Automatic memory
                                                  management
  -----------------------------------------------------------------------

------------------------------------------------------------------------

## 10. Project Structure

``` text
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

------------------------------------------------------------------------

## 11. Build Requirements

### Minimum requirements

-   Linux or WSL2 with a working Linux environment
-   g++ with C++17 support
-   Linux TUN/TAP support
-   CMake (optional)
-   pthread support
-   Root privileges or appropriate `CAP_NET_ADMIN` capability for TUN
    configuration

### Recommended environment

The project was developed and tested in a Linux environment using
**WSL2**.

------------------------------------------------------------------------

## 12. Build From Source

### Direct g++ build

``` bash
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

### CMake build

``` bash
mkdir -p build
cd build
cmake ..
make
```

------------------------------------------------------------------------

## 13. Running the Emulator

A typical configuration is:

``` bash
sudo ./chaos_emulator \
  --dev tun0 \
  --ip 10.8.0.1 \
  --loss-model gilbert-elliott \
  --delay-model correlated \
  --base-delay-ms 50 \
  --jitter-ms 10
```

Traffic must be routed through the test interface/network path before
the emulator can process it.

Example:

``` bash
sudo ip route add <test-target>/32 dev tun0
```

Traffic can then be generated using tools such as:

``` bash
ping <test-target>
```

For throughput experiments:

``` bash
iperf3 -c <test-target>
```

> **Important:** The emulator is intended for controlled testing
> environments. Do not redirect production traffic through an
> experimental TUN configuration without understanding the routing and
> network impact.

------------------------------------------------------------------------

## 14. Command-Line Parameters

  Parameter                         Default Description
  --------------------- ------------------- -------------------------------
  `--dev`                            `tun0` TUN device name
  `--ip`                         `10.8.0.1` IP assigned to TUN
  `--loss-model`          `gilbert-elliott` Loss model
  `--loss-p`                         `0.05` Bernoulli loss probability
  `--delay-model`              `correlated` Delay/jitter model
  `--base-delay-ms`                    `50` Base delay
  `--jitter-ms`                        `10` Jitter magnitude
  `--alpha`                          `0.85` AR(1) correlation coefficient
  `--report-interval`                   `5` Metrics reporting interval

------------------------------------------------------------------------

## 15. Loss Models

### Bernoulli Loss

The Bernoulli model applies an approximately independent probability to
every packet.

If:

``` text
loss-p = 0.05
```

then each packet has approximately a 5% probability of being dropped.

This model is useful for:

-   Simple experiments
-   Baseline statistical testing
-   Controlled random loss

### Gilbert-Elliott Loss

The Gilbert-Elliott model uses two states:

``` text
        +---------+
        |  GOOD   |
        +---------+
          |     ^
       bad |     | recovery
          v     |
        +---------+
        |  BAD    |
        +---------+
```

The model produces correlated packet loss and can therefore generate
**loss bursts** rather than independent isolated drops.

This is useful for approximating unstable network behavior.

------------------------------------------------------------------------

## 16. Delay and Jitter Models

### Gaussian Model

Generates delay variations using a Gaussian/normal distribution around a
configured base delay.

This represents independently varying latency.

### Correlated AR(1) Model

The correlated model uses the previous jitter value when generating the
next value.

Conceptually:

``` text
J(t) = alpha × J(t-1) + random_component
```

A larger correlation factor produces smoother changes between
consecutive packets.

This helps model networks where latency changes gradually rather than
independently for every packet.

------------------------------------------------------------------------

## 17. DelayQueue and Packet Reordering

The `DelayQueue` stores packets according to their calculated release
times.

For example:

``` text
Packet A -> release at 150 ms
Packet B -> release at 120 ms
Packet C -> release at 180 ms
```

Although A entered the queue before B, the scheduler releases:

``` text
B -> A -> C
```

Therefore, variable delay can naturally produce **packet reordering**.

This is an important network behavior because real applications may need
to handle packets arriving out of order.

------------------------------------------------------------------------

## 18. Metrics and Monitoring

The metrics system collects network-level measurements including:

### Traffic metrics

-   Packets in
-   Packets out
-   Bytes in
-   Bytes out

### Loss metrics

-   Total drops
-   Model-based drops
-   Queue/bandwidth-related drops

### Latency metrics

-   Minimum
-   Maximum
-   Mean
-   Standard deviation
-   p50
-   p95
-   p99

### System metrics

-   CPU usage
-   Memory usage
-   Thread count

The implementation uses **Welford's online algorithm** for stable
incremental calculation of mean and variance without requiring the
entire data set to remain in memory.

------------------------------------------------------------------------

## 19. Testing

The project contains a dependency-free test program:

``` bash
./test_models
```

The test suite validates the components that do not require a real TUN
device.

### Current validation includes

-   Bernoulli loss probability
-   Gilbert-Elliott burst behavior
-   Gaussian delay behavior
-   Correlated jitter behavior
-   IPv4 checksum calculation
-   Network byte-order parsing
-   DelayQueue release ordering
-   Packet reordering behavior

The current test suite contains **12 assertions**, all of which pass in
the validated build.

This separation between model tests and live network tests makes the
project easier to validate in CI or restricted environments.

------------------------------------------------------------------------

## 20. Experiment Example

A representative experiment can be configured with:

``` text
Duration:              60 seconds
Traffic:               ICMP
Configured latency:    100 ms
Configured jitter:      20 ms
Configured loss:         5%
Bandwidth cap:          10 Mbps
```

Example observed values from the project validation:

  Metric            Configured   Observed
  --------------- ------------ ----------
  Latency / RTT         100 ms   104.1 ms
  Jitter                 20 ms    16.4 ms
  Packet loss             5.0%       6.7%

Observed values can differ from configured impairment values because the
measurement includes the behavior of the test environment, packet
timing, routing, and measurement method.

------------------------------------------------------------------------

## 21. Advantages

### Software-only

No dedicated network-emulation hardware is required.

### Repeatable

The same impairment configuration can be executed repeatedly.

### Configurable

Network conditions can be changed through command-line parameters.

### Lightweight

The implementation uses standard Linux facilities and C++17 rather than
a large external framework.

### Educational

The project demonstrates concepts from:

-   Computer networks
-   Operating systems
-   Linux internals
-   C++
-   Computer architecture
-   Probability and statistics
-   Concurrent programming

### Extensible

The strategy-based model architecture allows additional loss and delay
models to be introduced without redesigning the entire engine.

### Measurable

The project does not only introduce impairments; it also records the
resulting network behavior.

------------------------------------------------------------------------

## 22. Limitations

The current implementation has several limitations.

### TUN-only operation

The current implementation requires traffic to pass through the
configured TUN interface.

### No NFQUEUE mode

Transparent impairment of existing physical-interface traffic is not
currently implemented.

### Configuration file not active

`config/profile.json` documents the intended configuration structure,
but runtime configuration is currently controlled through CLI
parameters.

### Additional impairment models pending

Packet corruption and duplication are not currently part of the active
pipeline.

### Prometheus endpoint not available

The metrics collector contains Prometheus-style text generation, but an
HTTP metrics endpoint has not yet been implemented.

### Network setup dependency

TUN configuration and routing require appropriate Linux networking
privileges.

### WSL2 considerations

WSL2 networking behavior can differ from a native Linux installation,
particularly around routing, interface visibility, and privileged
networking operations.

------------------------------------------------------------------------

## 23. Future Enhancements

Planned or possible improvements include:

1.  **NFQUEUE support** for transparent traffic impairment.
2.  **JSON configuration loading**.
3.  **Packet duplication model**.
4.  **Packet corruption model**.
5.  **Advanced bandwidth shaping using token buckets**.
6.  **Prometheus HTTP metrics endpoint**.
7.  **Real-time web dashboard**.
8.  **Experiment configuration profiles**.
9.  **CSV/JSON experiment export**.
10. **Automated experiment comparison**.
11. **Docker-based test environment**.
12. **Continuous integration testing**.
13. **Additional TCP/UDP traffic generators**.
14. **More advanced network models based on measured traces**.

------------------------------------------------------------------------

## 24. Project Status

### Implemented

-   [x] C++17 core implementation
-   [x] Linux TUN interface
-   [x] Packet capture
-   [x] IPv4 parsing
-   [x] IP checksum handling
-   [x] Bernoulli loss model
-   [x] Gilbert-Elliott loss model
-   [x] Gaussian delay/jitter model
-   [x] Correlated AR(1) jitter model
-   [x] Thread-safe DelayQueue
-   [x] Multithreaded processing
-   [x] Metrics collection
-   [x] Linux resource monitoring
-   [x] Experiment reporting
-   [x] Automated model tests
-   [x] WSL2 live validation

### Planned

-   [ ] NFQUEUE support
-   [ ] JSON runtime configuration
-   [ ] Packet corruption
-   [ ] Packet duplication
-   [ ] Full bandwidth shaping
-   [ ] Prometheus HTTP endpoint
-   [ ] Web dashboard

------------------------------------------------------------------------

## 25. Training/Academic Concept Coverage

This project was designed to combine multiple technical concepts into
one practical system.

### Linux

Used for:

-   TUN device management
-   `ioctl`
-   `poll`
-   `/proc`
-   signals
-   process and thread monitoring

### C++

Used for:

-   Classes and interfaces
-   RAII
-   Smart pointers
-   Templates
-   STL containers
-   `std::thread`
-   `std::mutex`
-   `std::condition_variable`
-   `std::atomic`
-   `<chrono>`
-   `<random>`

### Computer Networks

Used for:

-   IP packets
-   IPv4 headers
-   Protocol identification
-   Ports
-   Packet loss
-   Latency
-   Jitter
-   Throughput
-   Packet reordering
-   Network byte order

### Computer Architecture

Demonstrated through:

-   Memory representation of packet headers
-   Endianness
-   Software checksum processing
-   Hardware checksum-offload concepts
-   Hardware/software networking boundaries

### Probability and Statistics

Used for:

-   Bernoulli distributions
-   Gaussian distributions
-   Markov chains
-   Correlated random processes
-   Mean/variance
-   Percentiles
-   Statistical validation

------------------------------------------------------------------------

## 26. Repository Usage

Clone the project:

``` bash
git clone git@github.com:Aditya2108cse/network-chaos-emulator.git
cd network-chaos-emulator
```

Build:

``` bash
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

Run the model tests:

``` bash
g++ -std=c++17 -Wall -Wextra -O2 -Isrc \
  src/tests/test_models.cpp \
  src/chaos/LossModel.cpp \
  src/chaos/DelayJitterModel.cpp \
  src/net/PacketParser.cpp \
  src/scheduler/DelayQueue.cpp \
  -o test_models -lpthread

./test_models
```

------------------------------------------------------------------------

## 27. Safety and Testing Note

This project is intended for **controlled network testing and
educational use**.

Changing system routes or attaching experimental network interfaces can
affect connectivity. Testing should preferably be performed inside:

-   WSL2
-   Virtual machines
-   Containers
-   Isolated laboratory networks
-   Dedicated test environments

Avoid applying experimental routing rules to production interfaces
unless the configuration and consequences are fully understood.

------------------------------------------------------------------------

## 28. Author

**Aditya Krishna**

B.Tech --- Computer Science & Engineering

Project area:

**Networking • Linux • C++ • Systems Programming • Network Resilience**

GitHub:

https://github.com/Aditya2108cse

Repository:

https://github.com/Aditya2108cse/network-chaos-emulator

------------------------------------------------------------------------

## 29. Conclusion

The Network Latency & Packet-Loss Chaos Emulator provides a practical
software-based environment for reproducing degraded network conditions.

The project combines Linux networking, TUN interfaces, C++17 systems
programming, multithreading, probabilistic impairment models, packet
parsing, scheduling, metrics collection, and statistical testing into a
single application.

Its main value is not simply introducing packet loss or delay, but
demonstrating **how a network impairment engine can be designed,
implemented, measured, tested, and extended using low-level
operating-system and networking facilities**.

The current implementation establishes a functional foundation for
future features such as transparent NFQUEUE processing, advanced
bandwidth shaping, packet corruption/duplication, configuration files,
monitoring APIs, and a real-time visualization dashboard.
