# ARCHITECTURE — MiniCache

## 1. High-Level Component Diagram

```
                        ┌───────────────────────┐
  redis-cli / client ──▶│   TCP Listener          │  accept() loop, one thread per connection (v1)
                        └───────────┬────────────┘
                                    │ raw bytes
                                    ▼
                        ┌───────────────────────┐
                        │   RESP Parser/Encoder   │  bytes <-> Command objects (Codec pattern)
                        └───────────┬────────────┘
                                    │ Command (Command pattern)
                                    ▼
                        ┌───────────────────────┐
                        │   Command Dispatcher    │  routes to handler by command name (Command + Registry)
                        └───────────┬────────────┘
                                    │
                                    ▼
                        ┌───────────────────────┐
                        │   Shard Router          │  hash(key) % N  → picks shard
                        └───────────┬────────────┘
             ┌──────────────────────┼──────────────────────┐
             ▼                      ▼                      ▼
      ┌─────────────┐       ┌─────────────┐        ┌─────────────┐
      │  Shard 0      │      │  Shard 1      │       │  Shard N      │
      │  - HashMap    │      │  - HashMap    │       │  - HashMap    │
      │  - own mutex  │      │  - own mutex  │       │  - own mutex  │
      │  - EvictionPolicy (Strategy: LRU/LFU) │
      │  - TTL index (min-heap by expiry)     │
      └──────┬────────┘      └──────┬────────┘       └──────┬────────┘
             ▼                      ▼                       ▼
      ┌─────────────┐       ┌─────────────┐        ┌─────────────┐
      │ AOF writer   │       │ AOF writer   │        │ AOF writer   │   (one log per shard, avoids contention)
      └─────────────┘       └─────────────┘        └─────────────┘

      Background threads (cross-cutting):
      - Active Expiry Sweeper  (periodically samples shards, purges expired keys)
      - Snapshotter            (periodic full-state dump per shard)
      - Replication Streamer   (leader only, tails AOF, pushes to followers)
```

## 2. Core Data Structures

### 2.1 Per-shard store
```cpp
struct Entry {
    Value value;              // std::variant<std::string, HashMap, std::deque<std::string>, std::unordered_set<std::string>>
    std::optional<uint64_t> expiresAtMs;
    uint64_t lastAccessMs;    // for LRU
    uint32_t accessFreq;      // for LFU (aged counter, not raw count — see §4)
};

class Shard {
    std::unordered_map<std::string, Entry> data_;
    std::mutex mutex_;
    std::unique_ptr<EvictionPolicy> evictionPolicy_;   // Strategy
    IntrusiveList lruList_;                            // O(1) touch/evict for LRU
    // TTL: a min-heap (or sorted structure) of (expiresAt, key) for the active sweeper
};
```

**Why per-shard hashmap + mutex, not one global lock:** a single global mutex around the whole keyspace serializes every operation regardless of which keys are touched — this is the #1 mistake in naive implementations and the #1 thing an interviewer will ask you to justify. Sharding bounds contention to keys that happen to hash to the same shard.

**Why `std::variant` for `Value`:** avoids a class hierarchy + virtual dispatch for something that's fundamentally a closed set of 4 types; pattern-matched with `std::visit`. Mention this as a deliberate "modern C++ over classic OOP" choice — a strong thing to say out loud.

### 2.2 TTL / expiry
- **Lazy expiry:** on every `GET`/access, check `expiresAtMs` first; if expired, delete-on-read before returning.
- **Active expiry:** a background thread periodically samples a random subset of keys-with-TTL per shard (mirrors real Redis's probabilistic active-expire cycle) and purges expired ones — bounds memory held by TTL'd keys nobody ever reads again.

### 2.3 Eviction — `EvictionPolicy` interface (Strategy pattern)
```cpp
class EvictionPolicy {
public:
    virtual void onAccess(const std::string& key) = 0;
    virtual void onInsert(const std::string& key) = 0;
    virtual std::string selectVictim() = 0;   // called when shard exceeds memory bound
    virtual ~EvictionPolicy() = default;
};
class LRUPolicy : public EvictionPolicy { /* intrusive doubly-linked list, O(1) */ };
class LFUPolicy : public EvictionPolicy { /* aged frequency counter buckets, O(1) amortized */ };
```
LFU note: don't implement it as "sort all keys by access count" — that's O(n log n) per eviction and will visibly fall over under load / in a benchmark. Use an aging counter (increment on access, periodically halve all counters) with a small number of frequency buckets — same trick real Redis uses (`LFU_INIT_VAL`, probabilistic increment, decay).

## 3. Concurrency Model
- **v1 (Day 2 deliverable):** thread-per-connection for networking; per-shard `std::mutex` for data access. Simple, correct, easy to reason about and to explain.
- **Documented scale-up path (not built, but written up in README):** replace thread-per-connection with an epoll/kqueue-based event loop (or `io_uring` on Linux) to avoid thread-per-connection overhead at high connection counts; replace per-shard `std::mutex` with a single-threaded event loop *per shard* (actor-style, like real Redis's single-threaded core) to remove locking entirely in the hot path.
- **Correctness proof point:** a stress test — spin up K threads, each issuing thousands of `INCR counterkey`, assert the final value equals `K * iterations`. This is the single test that proves your concurrency story isn't just "it compiled."

## 4. Persistence Design

### 4.1 AOF (Append-Only File)
- Every write command, once validated, is serialized (its RESP form is convenient — reuse the encoder) and appended to that shard's AOF file.
- fsync policy — pick one and justify it in the README:
  - `always`: fsync every write — safest, slowest.
  - `everysec`: fsync once/sec via a background flusher — Redis's real default, good latency/durability tradeoff. **Recommended default for this project.**
  - `never`: rely on OS page cache flush — fastest, weakest guarantee.

### 4.2 Snapshotting
- Periodically (config interval, or on `SHUTDOWN`), each shard serializes its full key space to a snapshot file.
- On startup: load the latest snapshot (fast), then replay only the AOF entries written *after* that snapshot's timestamp (bounded replay time) — this two-tier design is exactly why Redis itself uses RDB+AOF together rather than either alone, and is worth stating explicitly.

### 4.3 Recovery flow
```
startup → load snapshot (if exists) → replay AOF tail since snapshot timestamp → ready to accept connections
```

## 5. Networking / Wire Protocol
- Implement RESP2 (simpler than RESP3): Simple Strings (`+OK\r\n`), Errors (`-ERR ...\r\n`), Integers (`:123\r\n`), Bulk Strings (`$5\r\nhello\r\n`), Arrays (`*2\r\n...`).
- This is a small, well-specified parser (a few hundred lines) that unlocks compatibility with the real `redis-cli` — disproportionate demo value for the implementation effort.

## 6. Sharding & Replication

### 6.1 Sharding (single-process, must-have)
- `shardIndex = hash(key) % N`, N configurable at startup.
- Document consistent hashing as the stretch/scale-up alternative (avoids full re-sharding when N changes) — implement only if Day 3 has slack.

### 6.2 Replication (stretch)
- Leader accepts writes, appends to AOF as normal, and additionally pushes each committed write over a persistent TCP connection to any connected followers.
- Follower applies incoming stream to its own in-memory store; rejects direct client writes.
- Explicitly **not** implementing: automatic failover, quorum/consensus, conflict resolution. State this as a known gap, not an oversight — that framing matters in interviews.

## 7. Design Patterns Used (name these explicitly in your README)
| Pattern | Where | Why |
|---|---|---|
| Strategy | `EvictionPolicy` (LRU/LFU/noeviction) | swap policy without touching shard logic |
| Command | Parsed client request → `Command` object | decouples parsing from execution, enables AOF logging by re-serializing the same object |
| Factory | `CommandFactory` builds the right `Command` subclass from a RESP array | centralizes validation/parsing |
| Singleton (justified, narrow use) | `ShardRouter` | one routing table per process; explain why you didn't overuse this pattern elsewhere |
| Observer | Replication streamer subscribing to "write committed" events | decouples core write path from replication concerns |

## 8. Failure Modes Considered (worth a README section)
- Process killed mid-write → bounded by AOF fsync policy; document exact data-loss window.
- Disk full during AOF write → must fail the write to the client with an error, not silently drop it.
- Shard rebalancing when N changes → out of scope for hash-mod-N; noted as the reason consistent hashing exists.
- Follower falls behind leader → no backpressure/catch-up protocol implemented; documented gap.
