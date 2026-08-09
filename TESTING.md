# libpkgreconcile-posix testing

## Principle

Tests qualify authority and failure behavior, not only successful serialization.
A durable store is useful only if it refuses to guess when its authority is
ambiguous.

## Suites

### unit

Pure public values owned by this repository. The initial suite verifies the
public `store_error` contract.

### mechanism

Real filesystem-backed store behavior:

- initialization and exact target binding;
- rejection of NUL-bearing store pathnames before POSIX path use;
- atomic batch publication;
- byte-exact round trips for opaque references and non-line-oriented path bytes;
- idempotent publication;
- exact resolution;
- tombstone anti-resurrection across reopen;
- same-path distinct evidence;
- non-blocking lock contention;
- root-directory authority after rename/replacement;
- symlink refusal;
- selector, binding, mode, and generation corruption;
- narrow interrupted-initialization completion;
- refusal of missing authoritative components; and
- ignorance of unselected temporary crash debris.

These tests use fictional `test.*` provider identifiers only.

### header

Every installed public header compiles independently. The umbrella consumer also
checks non-copyable store-handle and exception-shape contracts.

### contract

Source-level contracts enforce:

- exact ELF ABI;
- dependency/authority boundaries;
- explicit GCC/Clang and shared/static CI qualification;
- documentation truth;
- Doxygen coverage;
- release metadata;
- repository shape;
- style/whitespace;
- pkg-config dependency closure; and
- test topology.

## Required matrices

Shared and static builds must both pass. GCC and Clang must both qualify the
normal suites. ASan+UBSan must run the complete test set.

The shared ABI contract compares the entire dynamic symbol set against the
reviewed manifest in `abi/libpkgreconcile-posix.exports`.

## Regression obligations

Any change to storage encoding, generation selection, locking, initialization,
anti-resurrection, or target binding requires a mechanism regression.

A new neighboring package-management dependency is an architecture change and
must update both design documentation and the architecture contract.

Tests must not inspect or depend on an external rejected-object provider layout.
Only this repository's own private persistence layout may be inspected by
mechanism fixtures.
