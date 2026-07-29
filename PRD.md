# PRD — MiniCache (a Redis-like distributed in-memory cache)

## 1. Summary
MiniCache is a single-binary, in-memory key-value store, network-accessible over a Redis-compatible wire protocol subset (RESP), supporting TTL-based expiry, pluggable eviction policies, crash-recoverable persistence, and optional leader-follower replication. Target: a project that is functionally demoable (connect with `redis-cli` or a raw `nc`/telnet session and run real commands), not just a library.

## 2. Goals
- G1: Support core Redis data types and commands well enough that `redis-cli` (or any RESP client) can talk to it unmodified.
- G2: Survive process restart without data loss beyond a small, bounded window (persistence + recovery).
- G3: Support configurable memory bounds with pluggable eviction (LRU, LFU) — never OOM-kill the process under load.
- G4: Handle concurrent clients correctly and with real throughput — not a single global lock around everything.
- G5: (Stretch) Support one leader + N followers with asynchronous replication.

## 3. Non-Goals (explicitly out of scope)
- Full Redis command surface (no pub/sub, no Lua scripting, no cluster mode, no ACLs).
- Multi-key transactions (MULTI/EXEC) — mention as future work only.
- Persistence guarantees beyond "AOF + periodic snapshot" (no synchronous replication, no Raft/Paxos consensus).
- A custom client library — `redis-cli` or a thin test client is sufficient.

## 4. Users / Use Cases
- **Primary use case (for this project's purpose):** a portfolio-quality systems project that can be demoed live in an interview: start the server, connect with `redis-cli -p 6380`, run `SET`, `GET`, `EXPIRE`, kill -9 the process, restart it, show data survived.
- **Secondary use case:** a benchmarking target — you can run standard Redis benchmarking tools (`redis-benchmark`) against it and report real throughput/latency numbers.

## 5. Functional Requirements

### 5.1 Data types & commands (must-have)
| Type | Commands |
|---|---|
| String | `SET`, `GET`, `DEL`, `EXISTS`, `INCR`, `DECR`, `APPEND`, `STRLEN` |
| Expiry | `EXPIRE key seconds`, `TTL key`, `PERSIST key` |
| Hash | `HSET`, `HGET`, `HDEL`, `HGETALL`, `HEXISTS` |
| List | `LPUSH`, `RPUSH`, `LPOP`, `RPOP`, `LRANGE`, `LLEN` |
| Set | `SADD`, `SREM`, `SISMEMBER`, `SMEMBERS` |
| Server | `PING`, `INFO`, `DBSIZE`, `FLUSHALL`, `SHUTDOWN` |

### 5.2 Expiry semantics
- Keys with a TTL are lazily checked on access (expired-on-read) **and** actively swept by a background thread (like Redis's active expiry cycle) — both must be implemented; this is a common interview probe ("what if nobody reads the key?").

### 5.3 Eviction (must-have)
- Configurable `maxmemory` (bytes). When exceeded, evict keys according to a configured policy:
  - `LRU` (approximate, via a doubly-linked list + hashmap, O(1) touch/evict)
  - `LFU` (frequency-based, using a bucketed/aging counter — not a naive full sort)
  - `noeviction` (reject writes with an error, like real Redis)
- Policy must be swappable at runtime-config level via a `Strategy` interface — no `if/else` chains in the eviction path.

### 5.4 Persistence (must-have)
- **AOF (Append-Only File)**: every write command is logged before being acknowledged to the client.
- **Snapshotting**: periodic full-state dump (like Redis RDB) to allow fast startup instead of replaying a potentially huge AOF from scratch.
- **Recovery**: on startup, load latest snapshot, then replay AOF entries written after that snapshot.

### 5.5 Networking
- TCP server implementing a **subset of RESP (REdis Serialization Protocol)** — this is what makes `redis-cli` able to talk to it with zero modification, which is the single most impressive demo moment of this project.
- Concurrent client handling (thread-per-connection is acceptable for v1; note event-loop/epoll as the scale-up path).

### 5.6 Sharding
- Keyspace partitioned across N shards via consistent hashing (or simple hash-mod-N for v1, consistent hashing as stretch).
- Each shard has its own lock/data structure — a request for key K only contends with other requests for keys in K's shard.

### 5.7 Replication (stretch, Day 3 if time allows)
- One leader accepts writes, streams AOF entries to connected followers.
- Followers are read-only, apply the stream in order.
- No failover/consensus — explicitly document this as the "what I'd add for production" gap.

## 6. Non-Functional Requirements
- **Throughput target:** report actual measured ops/sec via `redis-benchmark -p 6380` in the README — no throughput number should be claimed without a benchmark run backing it.
- **Correctness under concurrency:** a stress test with N client threads hammering shared keys (e.g. concurrent `INCR`) must produce the mathematically correct final value every run.
- **Startup recovery time:** document snapshot-load + AOF-replay time at some reference dataset size (e.g. 100k keys).

## 7. Success Metrics (how you'll know it's resume-ready, not just "working")
1. `redis-cli` connects and runs a real session unmodified.
2. Kill -9 mid-write, restart, data loss bounded to sub-second window — demonstrated live.
3. A concurrency stress test with a provable correct outcome (e.g. concurrent INCR to expected sum).
4. A `redis-benchmark` number in the README, not a guess.
5. README explicitly names every design pattern used and why, plus a "what I'd do at scale" section.

## 8. Milestones (maps to day-by-day build plan)
- **M1 (Day 1):** Single-threaded core — hashmap store, TTL, RESP parser, TCP server, `redis-cli` can `SET`/`GET`.
- **M2 (Day 2):** Sharding + concurrency, eviction policies (LRU/LFU), stress tests.
- **M3 (Day 3):** AOF + snapshot persistence, crash-recovery demo, benchmarking, (stretch) replication, README polish.

## 9. Open Questions / Decisions Deferred to Architecture Doc
- Thread-per-connection vs. event loop — see ARCHITECTURE.md §Concurrency Model.
- AOF fsync policy (always / every-second / never) — tradeoff documented in ARCHITECTURE.md.
