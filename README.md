# libpkgreconcile-posix

`libpkgreconcile-posix` is the native POSIX persistence provider for
`libpkgreconcile` inventories.

It durably binds one store to one exact
`pkgreconcile::reconciliation_target_reference`, persists complete inventories
as immutable content-addressed generations, and atomically selects one current
generation.

The provider owns two state transitions:

- publish newly observed pending reconciliation tuples; and
- resolve one exact pending tuple.

Publishing cannot reactivate an exact tuple already recorded as `resolved`.
That is the anti-resurrection guarantee provided by this repository.

## Guarantees

A store is durably bound to one exact target reference. Reopening with another
reference is refused, including the initialization-recovery case where an empty
generation was selected before the binding file became durable.

Pending publication is atomic for the supplied batch. For every exact tuple:

- absent -> inserted as `pending`;
- already `pending` -> unchanged; and
- already `resolved` -> unchanged and reported as suppressed.

Resolution is exact and idempotent:

- `pending` -> durably `resolved`;
- `resolved` -> `already_resolved`; and
- absent -> `missing`.

Mutating methods return success only after the immutable generation, selector,
and store-directory durability boundary has been synchronized and the selected
inventory has been read back successfully. An exception after selector
replacement may leave either the old or requested generation selected; retrying
the same operation is safe because both public mutations are idempotent.

Store handles retain an opened directory authority. Renaming or replacing the
configured pathname after construction does not retarget an existing handle.
Authoritative file names are opened non-blocking and then required to be regular
immutable files, so replacing one with a FIFO is refused rather than allowed to
stall an indefinite read wait.

## Boundary

`libpkgreconcile-posix` does not:

- discover rejected application evidence;
- interpret rejected-object locator bytes;
- reopen retained objects;
- decide package ownership;
- read or publish installed package state;
- mutate target package paths;
- compare or merge retained contents;
- choose user-facing dispositions;
- garbage-collect resolved tombstones; or
- coordinate package transactions.

There is no compatibility rejected-file tree and no frontend in this
repository. The old toolchain remains a separate system.

`libpkgreconcile` owns the in-memory value model. `libcrypto` is used only to
content-address serialized generations.

## Requirements

Build-time requirements:

- Linux with `openat(2)`, `flock(2)`, `fsync(2)`, and
  `renameat2(RENAME_NOREPLACE)`;
- a C++17 compiler;
- Meson 1.6.0 or later;
- Ninja;
- pkg-config;
- `libpkgreconcile` 0.3.0 or later; and
- OpenSSL `libcrypto`.

Shared-library ABI tests require `nm`. Doxygen and `scdoc` are optional
documentation dependencies.

## Building

Shared:

```sh
meson setup build \
  -Ddefault_library=shared \
  -Dlink_mode=shared
meson compile -C build
meson test -C build --print-errorlogs
```

Static:

```sh
meson setup build-static \
  -Ddefault_library=static \
  -Dlink_mode=static
meson compile -C build-static
meson test -C build-static --print-errorlogs
```

The project rejects `default_library=both`; shared and static artifacts are
qualified independently.

## Using the provider

The target reference comes from the authority that owns target identity. This
repository does not define a neighboring provider identifier.

```cpp
#include <libpkgreconcile-posix/libpkgreconcile-posix.h>

pkgreconcile::posix::inventory_generation_store store(store_path, target);

auto receipt = store.publish_pending({pending});

if (receipt.changed()) {
  // A new durable generation was selected.
}

auto outcome = store.resolve(pending);
```

`publish_pending()` accepts a batch so evidence produced by one completed
operation can enter the pending inventory atomically. Duplicate exact tuples in
one request are rejected instead of being silently counted twice.

Compiler and linker flags are available through pkg-config:

```sh
pkg-config --cflags --libs libpkgreconcile-posix
pkg-config --static --libs libpkgreconcile-posix
```

## Storage

`STORAGE.md` is the authoritative repository document for format version 1,
publication ordering, initialization recovery, and corruption refusal.

The layout is a mechanism contract of this provider. Consumers must use the
public API rather than scan generation names or parse provider-private files.

## Tests

The suite is split by responsibility:

- `unit` — public error/value behavior local to this library;
- `mechanism` — real filesystem persistence, locking, corruption, recovery,
  authority anchoring, and anti-resurrection;
- `header` — independent installed-header consumers; and
- `contract` — ABI, dependencies, CI, documentation, release, repository,
  style, pkg-config, and test topology.

See `TESTING.md` for the qualification doctrine.

## License

`libpkgreconcile-posix` is licensed under GPL-3.0-or-later. See `COPYING` and
`COPYRIGHT`.
