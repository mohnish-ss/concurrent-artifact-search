# Concurrent Artifact Search Simulator

A concurrent C simulation in which worker threads move through shared search sites, collect items, and publish them to a shared result set. The implementation demonstrates POSIX threads, synchronized shared state, and explicit invariants for concurrent systems.

## Technical Highlights

- Runs one POSIX thread per search worker.
- Protects site occupancy, per-site items, and the shared result set with synchronization primitives.
- Acquires pairs of site locks in ascending site-index order to prevent circular lock dependencies during movement.
- Transfers each item without holding a site lock and the result-set lock at the same time.
- Uses C11 atomics for termination and worker-failure signals.
- Verifies item counts, scores, worker locations, and remaining work after all threads join.

## Architecture

`SearchSimulation` owns a ring of `SearchSite` values, worker state, thread handles, and the shared `ResultSet`. Each worker repeatedly moves to an adjacent site, removes one item while holding that site's lock, then adds it to the result set under a separate lock. `SearchLock` uses an unnamed POSIX semaphore where the platform supports it and a POSIX mutex fallback on macOS, where unnamed semaphores are unavailable.

## Building

```bash
make
```

The build produces `artifact-search`.

## Running

Run the default simulation with eight workers, six sites, and sixty items:

```bash
./artifact-search
```

Or supply the worker, site, and item counts:

```bash
./artifact-search 12 7 250
```

The site count must be at least two.

## Testing

Run deterministic integration tests across several simulation sizes:

```bash
make test
```

Run AddressSanitizer and UndefinedBehaviorSanitizer:

```bash
make asan
```

Run ThreadSanitizer:

```bash
make tsan
```

Sanitizer availability depends on compiler and platform support.

## Technical Concepts

- C11
- POSIX Threads
- POSIX Semaphores
- Mutexes
- C11 Atomics
- Concurrency
- Synchronization
- Deterministic Lock Ordering
- Shared-State Invariants
