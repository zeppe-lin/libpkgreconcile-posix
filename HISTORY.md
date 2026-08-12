# History

## 0.1.0 — unreleased

Initial native POSIX persistence boundary.

- durably bind one store to one exact reconciliation target reference;
- store complete inventories as immutable SHA-256-addressed generations;
- atomically select the current generation;
- publish pending evidence in atomic batches;
- retain resolved records as tombstones;
- suppress replay of exact resolved evidence;
- resolve exact pending tuples idempotently;
- refuse corrupt, mismatched, or ambiguous authoritative state;
- refuse FIFO substitutions at authoritative file names without blocking;
- anchor open handles to directory authority rather than a mutable pathname; and
- qualify shared/static, GCC/Clang, sanitizers, headers, ABI, and repository
  contracts.
