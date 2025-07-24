# Extended-Raft-Consensus-Protocol - implementation with gRPC

> **Goal**: Learn and demonstrate the core pieces of the Raft consensus algorithm — leader election, heartbeats, log replication plumbing, controlled failures/partitions, and basic latency/throughput benchmarking — in a small, testable C++20 codebase.

---

## Table of contents

1. [Features](#features)
2. [Repo layout](#repo-layout)
3. [Prerequisites](#prerequisites)
4. [Quick start (build & run)](#quick-start-build--run)
5. [Running a manual cluster](#running-a-manual-cluster)
6. [Latency / throughput benchmarks](#latency--throughput-benchmarks)
7. [Testing](#testing)
8. [Important flags](#important-flags)
9. [Logging](#logging)
10. [Troubleshooting](#troubleshooting)
11. [License](#license)

---

## Features

* **Core Raft roles**: *Leader*, *Follower*, *Candidate*
* **RPCs over gRPC**: `RequestVote`, `AppendEntries` (heartbeats included)
* **Volatile & persistent state**: terms, votes, minimal log entries
* **Failure simulation**: node **disconnect** or **network partition** via a controllable test harness
* **Command‑line node** with an interactive REPL to (re)connect/kill nodes
* **Latency & throughput drivers** (`latency`, `tput`) + Python plotting scripts
* **Unit & integration tests** (GoogleTest + gRPC test controller)
* **Modern C++ tooling**: CMake, Abseil flags, spdlog

---

## Repo layout

```
raft-lab-voteforme-lab-3-solution/
├── app/                    # Executables (node, multinode, latency, tput)
├── bench/                  # Plotting scripts & result charts
├── inc/                    # Public headers
│   ├── common/             # Config, logging, utils
│   ├── rafty/              # Raft core implementation
│   └── toolings/           # Test controller, config generator, msg_queue, etc.
├── integration_tests/      # End-to-end tests using gRPC test controller
├── proto/                  # .proto files + CMake to generate code
├── src/                    # Library sources
├── unittests/              # Unit tests (e.g., msg_queue)
├── cmake/                  # gRPC / protobuf cmake helpers
├── install_grpc.sh         # Convenience script to install gRPC locally
├── install_gcc-13.sh       # (Optional) Script to install a newer GCC
└── CMakeLists.txt
```

---

## Prerequisites

* **C++20** compiler (GCC 11+/Clang 14+; script provided for GCC 13 on Ubuntu)
* **CMake ≥ 3.10**
* **gRPC + Protobuf** (use `install_grpc.sh`, installs to `$HOME/.local`)
* **Abseil**, **spdlog**, **GoogleTest** (fetched/linked via CMake)
* **Python 3** (optional, for plotting in `bench/`)

On Ubuntu you can run:

```bash
./install_grpc.sh         # installs gRPC/protobuf into $HOME/.local
# (optional) ./install_gcc-13.sh
```

---

## Quick start (build & run)

```bash
git submodule update --init --recursive   # if applicable

# Build
mkdir -p build && cd build
cmake -DCMAKE_PREFIX_PATH=$HOME/.local .. # point to where gRPC/protobuf were installed
make -j

# Run unit & integration tests
ctest --output-on-failure
```

Binaries will be placed under `build/app/`, `build/unittests/`, and `build/integration_tests/`.

---

## Running a manual cluster

Start 3 nodes on localhost, each in its own terminal:

**Terminal 1**

```bash
./app/node --id=1 --port=50051 \
  --peers=2@127.0.0.1:50052,3@127.0.0.1:50053 \
  --verbosity=1
```

**Terminal 2**

```bash
./app/node --id=2 --port=50052 \
  --peers=1@127.0.0.1:50051,3@127.0.0.1:50053 \
  --verbosity=1
```

**Terminal 3**

```bash
./app/node --id=3 --port=50053 \
  --peers=1@127.0.0.1:50051,2@127.0.0.1:50052 \
  --verbosity=1
```

Interactive prompt:

```
Enter commands:
    r                connect peers + run the raft instance
    dis <id{,id}>    disconnect node(s) from the cluster
    conn <id{,id}>   reconnect node(s) to the cluster
    k                kill this raft instance
> 
```

Use it to simulate failures/partitions and watch re‑elections.

---

## Latency / throughput benchmarks

`latency` and `tput` spawn a cluster, drive load, and write results you can plot.

### Run

```bash
# In build/
./app/tput \
  --num=3 \                 # nodes (>= 3)
  --bin=./app/node \        # path to node binary
  --verbosity=0 \
  --fail_type=0             # 0=disconnect, 1=partition

# Results: build/app/result.txt
```

### Plot

```bash
cd ../bench
python3 lat-tput.py         # reads ../build/app/result.txt
# Produces LatencyThroughputLineChart*.png etc.
```

Pre-generated charts live in `bench/`:

* `latAvg_vs_Throughput_Task4.png`
* `latP50_vs_Throughput_Task4.png`
* `latP90_vs_Throughput_Task4.png`
* `latP99_vs_Throughput_Task4.png`

---

## Testing

* **Unit tests** in `unittests/`
* **Integration tests** in `integration_tests/` (spin up nodes + gRPC test controller)

Run everything:

```bash
cd build
ctest --output-on-failure
```

Or a single test:

```bash
./integration_tests/raft_test --verbosity=2
```

---

## Important flags

| Flag                 | Type     | Default           | Meaning                          |
| -------------------- | -------- | ----------------- | -------------------------------- |
| `--id`               | `uint16` | `0`               | Unique node id                   |
| `--port`             | `uint16` | `50051`           | Node’s listen port               |
| `--peers`            | string   | empty             | `2@127.0.0.1:50052,3@...`        |
| `--verbosity`        | `int`    | `1`               | 0=silent, 1=file, 2=file+console |
| `--fail_type`        | `int`    | `0`               | 0=disconnect, 1=partition        |
| `--enable_ctrl`      | `bool`   | `false`           | Enable gRPC test controller      |
| `--ctrl_addr`        | `string` | `localhost:55000` | Controller address               |
| `--node_tester_port` | `uint64` | `55001`           | Node tester port                 |
| `--num` (lat/tput)   | `uint64` | `3`               | Nodes to spawn                   |
| `--bin` (lat/tput)   | `string` | `./node`          | Path to node binary              |

---

## Logging

* Uses **spdlog** with console + file sinks
* Controlled via `--verbosity`
* Per-node log files are written when file sinks are enabled

---

## Troubleshooting

**`fatal error: grpcpp/...`**
Ensure gRPC/protobuf are installed to `$HOME/.local` and pass it to CMake:

```bash
cmake -DCMAKE_PREFIX_PATH=$HOME/.local ..
```

**Old compiler errors**
Use `install_gcc-13.sh` or upgrade your toolchain.

**Port already in use**
Change `--port` and the peer list.

**Benchmark scripts can’t find `result.txt`**
Run `tput` from the **build** directory or update the path in `bench/lat-tput.py`.

---

## License

This code is provided for educational purposes (Raft lab). If you intend to reuse it, please add an explicit LICENSE file appropriate for your needs.

---

**Questions / improvements?**
Open an issue or propose a PR — especially if you want to extend this with persistence, snapshotting, membership changes, or more adversarial tests.
