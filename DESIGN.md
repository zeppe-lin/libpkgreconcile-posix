# libpkgreconcile-posix design

## Purpose

`libpkgreconcile-posix` owns durable POSIX persistence for one target-bound
`libpkgreconcile` inventory.

It is a mechanism provider. It does not own evidence discovery, retained-object
access, package ownership, installed-state publication, target mutation, UI
policy, or transaction orchestration.

## Authority flow

```text
external evidence adapter
        |
        | pending_reconciliation values
        v
inventory_generation_store
        |
        | serialized complete inventory generations
        v
POSIX durable store
        |
        | reconciliation_inventory
        v
external resolution layer
```

The external arrows are boundaries, not hidden integrations in this repository.

## Host mechanism

The current provider is Linux-hosted POSIX mechanism code. Directory authority,
locking, and durability use `openat(2)`, `flock(2)`, and `fsync(2)`. Immutable
generation installation uses Linux `renameat2(RENAME_NOREPLACE)` so a
content-addressed generation can never be silently replaced. Unsupported host
mechanisms fail the build or the operation; the library does not emulate weaker
publication semantics.

## Why the store owns transitions

A full-inventory replacement API would make stale callers capable of losing a
concurrent resolution or resurrecting a tombstone. The public write surface is
therefore operation-shaped instead:

- `publish_pending(batch)` merges newly observed evidence into authoritative
  state while preserving all existing statuses; and
- `resolve(value)` changes only one exact existing tuple from `pending` to
  `resolved`.

Every mutation takes an exclusive store lock, rereads the authoritative current
inventory, computes the transition, and only then publishes a new generation.
There is no compare-and-swap token for callers to fake.

## Anti-resurrection

An exact `resolved` tuple is retained in every later generation unless a future
explicit retirement mechanism says otherwise. `publish_pending()` reports such
a tuple as suppressed and leaves it resolved.

The guarantee is exact-tuple based. A different retained side or different
rejected-object locator for the same target-relative path is new evidence and is
not suppressed.

There is deliberately no tombstone garbage collection in 0.1.0. Safe retirement
would require an evidence-lifetime and replay-horizon authority not owned here.

## Target binding

The store serializes the complete provider-qualified target reference supplied
by the caller. It never interprets those bytes.

Every selected generation contains the same target reference. Reads reject a
generation whose embedded target differs from the durable store binding or the
caller's expected target.

An empty inventory is still target-bound. Initialization cannot reuse an
unbound empty selection for another target.

## Root authority

After construction, operations are anchored through a retained directory file
descriptor and `openat()`-style relative access. The original pathname is only
diagnostic metadata.

This prevents pathname replacement from silently retargeting an existing store
handle.

## Concurrency

Reads acquire a non-blocking shared `flock()` on the store directory. Mutations,
initialization, and transition publication acquire a non-blocking exclusive
lock.

Lock contention is reported as `store_error`; the library never waits forever
for another process.

## Durability

A new inventory is encoded completely before publication. The provider:

1. writes and `fsync()`s an immutable inventory file;
2. `fsync()`s its generation directory;
3. installs the content-addressed immutable generation without replacement;
4. `fsync()`s the generations directory;
5. writes and `fsync()`s a temporary selector;
6. atomically renames it to `current`;
7. `fsync()`s the store directory; and
8. rereads the selected generation and verifies the exact encoded inventory.

Only then may a mutating call return success.

If an error occurs after step 6, the caller may not know whether the selector is
durable. Retrying is safe because publication and resolution are idempotent with
respect to authoritative state.

## Corruption policy

The provider refuses rather than reconstructs authoritative state when a bound
store loses or corrupts its selector, binding, or selected generation.

Temporary crash debris is ignored when it is not selected. No temporary name is
interpreted as authority.

Initialization has one narrow recoverable state: an empty generation for the
same target was durably selected but the binding file was not yet published.
That state may complete binding publication. A non-empty or foreign selected
inventory is refused.

## Non-authorities

This library does not:

- enumerate an application evidence store;
- know a `libpkgapply-posix` storage grammar;
- define rejected-object provider identifiers;
- reopen rejected objects;
- infer package ownership;
- synthesize a filesystem staging tree;
- mutate target package paths;
- publish canonical installed state;
- present diffs or merge editors;
- delete tombstones; or
- orchestrate lifecycle or transaction work.
