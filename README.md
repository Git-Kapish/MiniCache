# MiniCache

![Build & Test CI](https://github.com/Git-Kapish/MiniCache/actions/workflows/ci.yml/badge.svg)

**A Low-Level Design (LLD) project** — a Redis-like, distributed in-memory key-value cache built from scratch in C++17. Implements a real wire protocol (RESP2, compatible with `redis-cli`), sharded concurrent storage, pluggable eviction policies, crash-recoverable persistence, consistent hashing, and leader-follower replication.

This project demonstrates core low-level system design principles: data structure selection under concurrency and memory constraints, design pattern usage (Strategy, Command, Factory, Observer), and the durability/performance tradeoffs behind a production-grade in-memory datastore.

Full technical documentations:
- [`PRD.md`](./PRD.md) — Product requirements, scope, and non-goals
- [`ARCHITECTURE.md`](./ARCHITECTURE.md) — Component layout, memory model, concurrency, and design patterns
- [`AGENTS.md`](./AGENTS.md) — Module boundaries, build commands, and coding conventions
- [`TECH_STACK.md`](./TECH_STACK.md) — Language and tooling choices with technical rationale

---

## Project Architecture & Directory Layout

```
minicache/
├── PRD.md
├── ARCHITECTURE.md
├── AGENTS.md
├── TECH_STACK.md
├── README.md
├── CMakeLists.txt
├── .gitignore
├── src/
│   ├── net/              # TCP listener & socket abstraction (thread-per-connection)
│   ├── protocol/          # RESP2 parser and encoder (bytes <-> RespArray/RespValue)
│   ├── command/            # Command objects, CommandFactory, Dispatcher
│   ├── store/               # Shard, Entry, ShardRouter, ConsistentHashRing, TTL sweeper
│   ├── eviction/             # Strategy pattern: LRUPolicy, LFUPolicy, NoEvictionPolicy
│   ├── persistence/           # AOF Writer, Snapshotter, Startup Recovery Engine
│   ├── replication/            # Leader streamer & Follower replication client
│   └── main.cpp                 # Main entry point & CLI configuration parsing
└── tests/
    ├── test_resp_protocol.cpp
    ├── test_shard.cpp
    ├── test_eviction.cpp
    ├── test_concurrency_stress.cpp
    ├── test_persistence.cpp
    ├── test_consistent_hash.cpp
    └── test_replication.cpp
```

---

## Low-Level Design (LLD) Patterns

| Design Pattern | Implementation | Purpose & Tradeoff |
|---|---|---|
| **Strategy Pattern** | [`EvictionPolicy`](file:///d:/Projects/MiniCache/src/eviction/eviction_policy.hpp) (`LRUPolicy`, `LFUPolicy`, `NoEvictionPolicy`) | Swaps memory eviction algorithms dynamically without modifying shard storage logic. |
| **Command Pattern** | [`Command`](file:///d:/Projects/MiniCache/src/command/command.hpp) hierarchy (`SetCommand`, `GetCommand`, `HSetCommand`, etc.) | Encapsulates parsed request execution; identical objects are reused for AOF logging and replication broadcasting. |
| **Factory Pattern** | [`CommandFactory`](file:///d:/Projects/MiniCache/src/command/command_factory.hpp) | Decouples RESP array parsing from command construction and validates arguments centrally. |
| **Observer Pattern** | [`ReplicationStreamer`](file:///d:/Projects/MiniCache/src/replication/replication_streamer.hpp) | Leader streams committed RESP write events to followers asynchronously without coupling the hot write execution path. |
| **Consistent Hashing** | [`ConsistentHashRing`](file:///d:/Projects/MiniCache/src/store/consistent_hash_ring.hpp) | Ring placement using FNV-1a hashing and 100 virtual nodes/shard to minimize key relocation during ring scaling. |

---

## Server Configuration Options

| Option | Flag | Default | Description |
|---|---|---|---|
| Port | `--port`, `-p` | `6380` | TCP port to listen on. |
| Shards | `--shards` | `4` | Number of independent internal storage shards. |
| Memory Limit | `--maxmemory` | `256mb` | Total memory limit across all shards (e.g. `256mb`, `1gb`). |
| Eviction Policy | `--eviction` | `lru` | Eviction strategy: `lru`, `lfu`, or `noeviction`. |
| Sharding Mode | `--sharding-mode` | `modulo` | Key partitioning strategy: `modulo` or `consistent`. |
| AOF Persistence | `--aof` | `true` | Enable append-only write logging: `true` or `false`. |
| AOF File Path | `--aof-file` | `appendonly.aof` | Path to the AOF log file. |
| Fsync Policy | `--fsync` | `everysec` | Disk sync policy: `everysec`, `always`, or `never`. |
| Replication Mode | `--replicaof` | *(none)* | Run as Follower replica streaming from `<host> <port>` (Read-Only mode). |

---

## Benchmark Results

*(Measured using `redis-benchmark -p 6380 -t set,get -n 100000 -c 50` against live MiniCache server running 4 shards with AOF persistence enabled)*

```text
SET: 9,711.57 requests per second (p50: 6.0 ms, p99: 18.0 ms)
GET: 11,516.76 requests per second (p50: 3.0 ms, p99: 7.0 ms)
```

---

## Getting Started & Building

### 1. Build and Run Tests

#### Windows (MSVC):
```powershell
mkdir -p build && cd build
& "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" .. -DCMAKE_BUILD_TYPE=Debug
& "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build . --config Debug

# Run full CTest suite (7/7 test targets)
& "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --output-on-failure -C Debug
```

#### Linux / macOS:
```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Run full CTest suite
ctest --output-on-failure
```

---

### 2. Running MiniCache

#### Single Server Mode:
```powershell
./Debug/minicache_server.exe --port 6380 --shards 4 --maxmemory 256mb --eviction lru --aof true
```

#### Leader-Follower Replication Mode:
```powershell
# Terminal 1 (Leader):
./Debug/minicache_server.exe --port 6380

# Terminal 2 (Follower - Read Only):
./Debug/minicache_server.exe --port 6381 --replicaof 127.0.0.1 6380
```

#### Client Interaction (`redis-cli`):
```bash
redis-cli -p 6380 SET foo "bar"
redis-cli -p 6380 GET foo
redis-cli -p 6380 HSET user:1 name "Alice" email "alice@test.com"
redis-cli -p 6380 HGETALL user:1
```
