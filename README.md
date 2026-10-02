# Network Latency & Packet-Loss Chaos Emulator — Milestone 1

A software-only network chaos engine: intercepts IP packets via a Linux
TUN virtual interface, applies configurable packet loss and delay/jitter
models, and reinjects the packets, while collecting latency/loss/throughput
metrics.

This milestone delivers a working, **compiled and unit-tested** core
pipeline: TUN interface, two loss models, two delay models, a min-heap
delay scheduler, and a metrics collector — wired together by `ChaosEngine`.

## Build

Requires g++ (C++17) and Linux TUN/TAP kernel headers (present on any
standard Linux install/dev box).

```bash
g++ -std=c++17 -Wall -Wextra -O2 -Isrc \
  src/main.cpp \
  src/net/TunInterface.cpp src/net/PacketParser.cpp \
  src/chaos/LossModel.cpp src/chaos/DelayJitterModel.cpp \
  src/scheduler/DelayQueue.cpp src/metrics/MetricsCollector.cpp \
  src/engine/ChaosEngine.cpp \
  -o chaos_emulator -lpthread
```

Or with CMake (`CMakeLists.txt` included):

```bash
mkdir build && cd build && cmake .. && make
```

## Run

Requires `CAP_NET_ADMIN` (root, or grant the capability directly to the
binary):

```bash
sudo ./chaos_emulator --dev tun0 --ip 10.8.0.1 \
  --loss-model gilbert-elliott --delay-model correlated \
  --base-delay-ms 50 --jitter-ms 10
```

Then route test traffic through `tun0` and observe with `ping`/`iperf3`:

```bash
sudo ip route add <test-target>/32 dev tun0
ping <test-target>          # observe injected delay/jitter/loss
iperf3 -c <test-target>     # observe throughput impact
```

Command-line flags (all optional, defaults shown):

| Flag | Default | Meaning |
|---|---|---|
| `--dev` | `tun0` | TUN device name |
| `--ip` | `10.8.0.1` | Local IP assigned to the interface |
| `--loss-model` | `gilbert-elliott` | `bernoulli` or `gilbert-elliott` |
| `--loss-p` | `0.05` | Drop probability (bernoulli only) |
| `--delay-model` | `correlated` | `gaussian` or `correlated` |
| `--base-delay-ms` | `50` | Mean added latency |
| `--jitter-ms` | `10` | Jitter standard deviation |
| `--alpha` | `0.85` | AR(1) correlation strength (correlated only) |
| `--report-interval` | `5` | Seconds between metrics printouts |

`config/profile.json` documents the target config-file schema for a
follow-up milestone (JSON parsing via `nlohmann/json` isn't wired in yet —
see `main.cpp` header comment).

## Testing without root/a real network stack

`src/tests/test_models.cpp` is a small, dependency-free test harness for
every piece of logic that does **not** require opening a TUN device or
shelling out to `ip` — i.e. everything except `TunInterface` itself. This
is what to run in a sandbox/CI container that lacks `CAP_NET_ADMIN`:

```bash
g++ -std=c++17 -Wall -Wextra -O2 -Isrc \
  src/tests/test_models.cpp \
  src/chaos/LossModel.cpp src/chaos/DelayJitterModel.cpp \
  src/net/PacketParser.cpp src/scheduler/DelayQueue.cpp \
  -o test_models -lpthread
./test_models
```

It statistically validates:
- **Bernoulli loss** converges to the configured drop probability
- **Gilbert-Elliott loss** is genuinely *bursty*: P(drop | previous packet
  dropped) comes out far above the unconditional drop rate, confirming the
  Markov-chain memory actually produces burst behavior rather than flat
  noise
- **Gaussian jitter** stays non-negative and centers on the configured base
  delay
- **Correlated (AR(1)) jitter** changes more slowly between consecutive
  packets than independent noise would, confirming the smoothing effect
- **IP checksum fix-up** self-verifies to zero per RFC 1071, and flow-key
  parsing correctly converts network-byte-order fields
- **DelayQueue** pops packets in release-time order rather than insertion
  order — the exact mechanism that produces reordering

All 12 assertions currently pass.

## Architecture

```
TUN read (capture thread) -> Loss model -> Delay model -> DelayQueue (min-heap)
                                                                |
TUN write (scheduler thread) <---------------------------------+
                    |
              MetricsCollector
```

Two threads by design: the capture thread must never block on anything but
the next incoming packet; the scheduler thread's whole job is to block
until a delayed packet's release time arrives. `DelayQueue` decouples them.

See `src/` for module layout; each header has design-rationale comments
explaining *why*, not just what.

## Concepts-to-code mapping (training topics)

| Topic | Where |
|---|---|
| Linux syscalls/ioctl, TUN device | `TunInterface` (`open`, `ioctl(TUNSETIFF)`) |
| Multithreading, mutex/condvar | `DelayQueue`, `ChaosEngine` (capture + scheduler threads) |
| Signal handling | `main.cpp` (`SIGINT`/`SIGTERM`) |
| RAII | `TunInterface` fd ownership |
| Templates/strategy pattern | `ILossModel` / `IDelayModel` interfaces |
| Atomics, lock-free counters | `MetricsCollector` |
| Endianness | `PacketParser::parseFlowKey` (`ntohl`/`ntohs`) |
| Checksums / hardware offload | `PacketParser::fixIpChecksum` |
| Markov chains (bursty loss) | `GilbertElliottLossModel` |
| Min-heap / priority queue | `DelayQueue` |

## Known limitations / next milestones

- `TunInterface::configure()` shells out to `ip`; not tested in
  network-restricted sandboxes (works on a real Linux host with root).
- No NFQUEUE (transparent real-traffic) mode yet — TUN-only for now.
- No token-bucket bandwidth shaper or corruption/duplication models yet.
- No JSON config-file loading yet (CLI flags only).
- No Prometheus HTTP endpoint yet (`MetricsCollector::toPrometheusText()`
  exists but isn't served over HTTP).
