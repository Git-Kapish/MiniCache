# TECH_STACK — MiniCache

## Language & Standard
- **C++17.** Chosen over C++20 for broader toolchain compatibility (no need for coroutines/modules here) while still getting `std::variant`, `std::optional`, structured bindings, and `if constexpr` — all used deliberately in this design (see ARCHITECTURE.md §2.1).

## Build System
- **CMake (≥3.16)**, out-of-source builds. Chosen over Makefiles for portability and because it's the de facto standard expected by any reviewer/interviewer skimming the repo.

## Testing
- **GoogleTest** (or Catch2 — pick one, don't mix). GoogleTest recommended if you also want to demonstrate mocking (`gmock`) for the agent-parallelized module boundaries in AGENTS.md §5.
- **CTest** as the runner, wired through CMake, so `ctest` alone runs the full suite — matters for CI and for Definition-of-Done checks.

## Concurrency Primitives
- `std::thread`, `std::mutex`, `std::condition_variable` for v1 (thread-per-connection + per-shard locking).
- No external threading library needed at this scope — pulling in something like Boost.Asio for the event-loop scale-up path is a documented future step (ARCHITECTURE.md §3), not a Day 1-3 requirement.

## Networking
- Raw POSIX sockets (`<sys/socket.h>`) for the TCP listener — no framework. This is deliberate: implementing the RESP parser and connection loop by hand is the actual point of the exercise; a framework would hide the part that's meant to demonstrate skill.
- If targeting Windows as well as Linux, isolate the socket calls behind a small platform shim — but default to Linux-only to keep scope bounded.

## Persistence
- Plain flat files for AOF (append-only, line/record-delimited RESP-encoded commands) and snapshots (custom binary format, or even a simple length-prefixed record format). No embedded DB (SQLite etc.) — the point is to build the durability logic yourself, not delegate it.

## Benchmarking / Validation
- **redis-benchmark** (ships with the standard Redis distribution, or installable via `apt install redis-tools` without needing the full Redis server) — used purely as a load-generating client against your RESP-compatible server. This is what produces the throughput number for the README/resume claim.
- **redis-cli** — used as a manual/demo client; also installable via `redis-tools` without running actual Redis.

## Optional / Stretch Tech
- **Consistent hashing** implementation (custom, no library) if Day 3 has slack — see ARCHITECTURE.md §6.1.
- **Replication transport**: plain TCP + your own AOF-record framing — no need for gRPC/Protobuf here; introducing them would add ceremony without adding to the resume story (this project's story is "I built the systems primitives myself").

## Explicitly Avoided
- **Boost** (beyond maybe `Boost.Asio` as a documented future step) — keeping the dependency footprint minimal makes the project easier for anyone (including an interviewer) to clone and build with just a standard C++ toolchain + CMake.
- **Any actual Redis source/library code** — the entire value of this project is that the RESP protocol and storage engine are your own implementation, not a wrapper around `hiredis` or similar.

## Dev Environment Notes for Agents
- Target platform: Linux (matches the POSIX socket choice above). If building/testing on macOS, POSIX sockets still work; if on Windows, note the platform shim requirement from the Networking section above before starting network code.
- No API keys, external services, or network access required to build or run — the entire project is self-contained, which matters if a coding agent is running in a sandboxed environment without egress.
