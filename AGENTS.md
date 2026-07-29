# AGENTS.md — Instructions for Claude Code / coding agents working on MiniCache

This file is the entry point any coding agent (Claude Code, etc.) should read before touching this repo. Read `PRD.md` and `ARCHITECTURE.md` first — this file governs *how* to work, those govern *what* to build.

## 1. Project Snapshot
MiniCache is a Redis-like in-memory KV store in C++17: RESP protocol, sharded storage, pluggable eviction (LRU/LFU), AOF + snapshot persistence, thread-per-connection networking. Full spec in `PRD.md`; full design in `ARCHITECTURE.md`.

## 2. Ground Rules for Agents
- **Do not simplify scope silently.** If a requirement in PRD.md/ARCHITECTURE.md seems like too much for the current session, say so explicitly and propose a cut — don't quietly implement a weaker version (e.g. don't swap LFU for "just use LRU twice," don't swap the AOF for "just skip persistence for now") without flagging it.
- **Every design pattern named in ARCHITECTURE.md §7 must actually appear in the code as a named, identifiable class/interface** — not just conceptually present. A resume claim of "used Strategy pattern for eviction" must map to an actual `EvictionPolicy` abstract class with swappable implementations.
- **Prefer correctness and clarity over premature optimization.** Get the single-threaded core right (Day 1 scope) before touching concurrency.
- **Every module needs a unit test before being marked done.** No module is "complete" without a corresponding test file.
- **Commit after each working milestone**, not after the whole feature — small, buildable commits with clear messages (`feat(storage): add per-shard hashmap with TTL`, not `wip`).

## 3. Build & Test Commands
```bash
# Configure + build
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)

# Run unit tests
ctest --output-on-failure

# Run the server
./minicache_server --port 6380 --shards 4 --maxmemory 256mb --eviction lru

# Smoke test with redis-cli (if installed) or netcat
redis-cli -p 6380 SET foo bar
redis-cli -p 6380 GET foo

# Benchmark (if redis-benchmark is available)
redis-benchmark -p 6380 -t set,get -n 100000 -c 50
```
Agents should run the build + test suite after every non-trivial change and report failures immediately rather than continuing to layer new code on a broken build.

## 4. Directory Layout (target)
```
minicache/
├── PRD.md
├── ARCHITECTURE.md
├── AGENTS.md
├── TECH_STACK.md
├── CMakeLists.txt
├── src/
│   ├── net/           # TCP listener, connection handling
│   ├── protocol/       # RESP parser/encoder
│   ├── command/        # Command objects, CommandFactory, Dispatcher
│   ├── store/           # Shard, Entry, ShardRouter
│   ├── eviction/        # EvictionPolicy, LRUPolicy, LFUPolicy
│   ├── persistence/     # AOF writer/reader, Snapshotter
│   ├── replication/     # Leader/follower streaming (stretch)
│   └── main.cpp
├── tests/
│   ├── test_resp_protocol.cpp
│   ├── test_shard.cpp
│   ├── test_eviction.cpp
│   ├── test_persistence.cpp
│   └── test_concurrency_stress.cpp
└── README.md            # final polished writeup, benchmark numbers, patterns table
```

## 5. Suggested Agent/Task Split (for parallelizing across sessions or subagents)
If running multiple agent sessions in parallel, split along these seams — they have minimal interface overlap:

1. **Storage Agent** — `store/`, `eviction/`. Owns `Shard`, `Entry`, `EvictionPolicy` + implementations, TTL handling. Depends on nothing else.
2. **Protocol Agent** — `protocol/`, `command/`. Owns RESP parsing/encoding and `Command`/`CommandFactory`/`Dispatcher`. Depends on Storage Agent's public interface (can mock it with a stub `Shard` while developing in parallel).
3. **Networking Agent** — `net/`. Owns the TCP listener and connection loop. Depends on Protocol Agent's `Dispatcher` interface.
4. **Persistence Agent** — `persistence/`. Owns AOF + snapshot read/write and startup recovery flow. Depends on Storage Agent (needs to serialize `Entry`/`Shard` state) and Command Agent (needs to log `Command` objects).
5. **Replication Agent** (stretch, do last) — `replication/`. Depends on Persistence Agent's AOF stream.

Each agent should define its module's public header first (`.hpp` with the class interfaces from ARCHITECTURE.md) so other agents can build against it without waiting.

## 6. Definition of Done (per milestone, mirrors PRD.md §8)
- **M1 done** when: `redis-cli SET`/`GET`/`DEL`/`EXPIRE`/`TTL` work end-to-end against a running single-shard server, with passing unit tests for the RESP parser and the store.
- **M2 done** when: sharding is live, `INCR` concurrency stress test passes deterministically 10/10 runs, LRU and LFU are both selectable via config and have dedicated tests proving eviction order.
- **M3 done** when: killing the process mid-write and restarting recovers all acknowledged writes (demonstrated by a scripted test, not just manually), and `redis-benchmark` output is captured in `README.md`.

## 7. Code Style
- C++17, no raw `new`/`delete` — use smart pointers / RAII throughout.
- Header guards via `#pragma once`.
- No global mutable state outside `main.cpp` wiring — every component should be constructible/testable in isolation.
- Prefer `std::variant` + `std::visit` over inheritance for the closed `Value` type set (per ARCHITECTURE.md §2.1).

## 8. What NOT to do
- Don't add Lua scripting, pub/sub, or cluster mode — explicitly out of scope per PRD.md §3.
- Don't implement your own hashmap/data structures unless there's a specific documented reason (e.g. the intrusive LRU list) — use STL where STL is fine, and justify any custom structure in a code comment.
- Don't skip the concurrency stress test — it's the single piece of evidence that the "thread-safe" claim in the README is true.
