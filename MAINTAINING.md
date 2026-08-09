# Maintaining libpkgreconcile-posix

## Release boundary

Version 0.1.0 owns one durable mechanism: target-bound reconciliation inventory
persistence with anti-resurrection.

Keep project version, `HISTORY.md`, Doxyfile, pkg-config metadata, shared ABI
manifest, and SONAME policy aligned for releases.

## Storage compatibility

`STORAGE.md` is authoritative for durable format version 1. Never change the
meaning of an existing format byte silently.

If an incompatible format becomes necessary, add explicit version handling and
qualification. Do not reinterpret old bytes under a new meaning.

## Tombstones

Do not add tombstone deletion merely to control disk usage. Retirement requires
a separately justified replay-horizon authority. Until that exists, resolved
records remain part of authoritative history.

## Failure policy

Corrupt or incomplete bound stores are refused. Recovery code must not infer a
new current generation from directory contents.

The only initialization completion currently allowed is the documented
empty-selection-before-binding case for the exact same target.

## ABI

The shared ABI is an exact mangled-symbol manifest. Public additions require
review and manifest update. Private implementation symbols must remain hidden.
