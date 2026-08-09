# libpkgreconcile-posix storage format

## Scope

This document defines durable storage format version 1 used by
`inventory_generation_store`.

The format is owned by this provider. It exists so future releases can validate
and migrate their own durable state. It is not a discovery API for consumers.
Consumers must not derive evidence locators or reconciliation identity from
these pathnames.

## Layout

```text
STORE/
  binding
  current
  generations/
    v1-sha256-<64 lowercase hex>/
      inventory
```

`binding`, `current`, and `inventory` are regular immutable files. Generation
directories are immutable after installation. Temporary names contain `.tmp.`
and are never authoritative.

## Integer encoding

All integer fields are unsigned big-endian values.

Length-prefixed strings and byte arrays use a 32-bit length followed by exactly
that many bytes. Strings are byte strings; no locale or Unicode normalization is
performed by this provider.

## Binding

`binding` contains:

```text
8 bytes   magic "ZLRBND01"
u32       provider byte length
bytes     provider identifier
u32       opaque target-reference length
bytes     opaque target-reference bytes
```

The decoded value must be accepted by
`reconciliation_target_reference::make()` and must match the caller's exact
target reference.

## Inventory generation

`inventory` contains:

```text
8 bytes   magic "ZLRINV01"
          target reference encoded as above, without binding magic
u64       record count
repeat record count times:
  u8      reconciliation_record_status
  u8      rejected_object_side
  u32     path byte length
  bytes   std::filesystem::path native bytes
  u32     rejected-object provider length
  bytes   rejected-object provider
  u32     rejected-object locator length
  bytes   opaque rejected-object locator
```

The record numeric values are the corresponding public `libpkgreconcile` enum
values. Version 1 therefore accepts only statuses `pending=1`, `resolved=2` and
sides `incoming=1`, `prior_installed=2`.

Version 1 is defined by the current Linux provider. Target-relative path bytes are
the exact `std::filesystem::path::native()` byte sequence on that supported
host; no locale, Unicode normalization, or line-oriented encoding is applied.

After decoding, the provider reconstructs a
`reconciliation_inventory`. It re-encodes the normalized inventory and requires
byte-for-byte equality with the stored bytes. Noncanonical order, duplicate
exact tuples, malformed paths, cross-target records, and trailing bytes are
therefore rejected.

## Generation identity

The generation identity is:

```text
v1:sha256:<SHA-256 of exact inventory bytes>
```

The directory component replaces the first two separators:

```text
v1-sha256-<64 lowercase hex>
```

An existing directory at that identity must contain byte-identical inventory
data. A mismatch is treated as collision or corruption.

## Selector

`current` contains exactly one newline-terminated generation identity:

```text
v1:sha256:<64 lowercase hex>\n
```

No extra lines or trailing bytes are accepted.

## Initialization

For a new store, the provider creates and selects the empty target-bound
inventory, confirms selector durability, then publishes the target binding.

If a crash occurs after empty selection but before binding publication, a later
initializing open may complete the binding only when the selected inventory is
empty and bound to the exact caller-supplied target.

Once `binding` exists, missing or corrupt authoritative components are refused;
the constructor does not invent recovery state.

## Tombstones

Resolved records are serialized exactly like pending records with status value
`2`. Version 0.1.0 never removes them automatically.

This is intentional. The persistence provider can prove exact historical
resolution state but does not own a safe replay horizon for retirement.
