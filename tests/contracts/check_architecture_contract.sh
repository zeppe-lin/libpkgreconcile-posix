#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

root=$1
fail()
{
  echo "architecture-contract: $*" >&2
  exit 1
}

# This repository may depend only on the reconciliation semantic core and the
# cryptographic digest mechanism used for content addressing.
if grep -RInE '#[[:space:]]*include[[:space:]]*<libpkg(apply|state|transaction|exec|resolve|plan|catalog|source|build)' \
    "$root/include" "$root/src" --include='*.h' --include='*.cpp' >/dev/null; then
  fail 'neighboring package-management authority leaked into POSIX persistence'
fi

for dependency in libpkgreconcile libcrypto; do
  grep -F "'$dependency'" "$root/meson.build" >/dev/null ||
    fail "required dependency is missing from Meson: $dependency"
done

count=$(grep -cE '^[[:space:]]*[A-Za-z0-9_]+_dep[[:space:]]*=[[:space:]]*dependency\(' "$root/meson.build")
[ "$count" -eq 2 ] || fail "expected exactly two external dependencies, found $count"
grep -E '^libpkgreconcile_dep[[:space:]]*=[[:space:]]*dependency\(' "$root/meson.build" >/dev/null ||
  fail 'libpkgreconcile dependency declaration shape drifted'
grep -E '^libcrypto_dep[[:space:]]*=[[:space:]]*dependency\(' "$root/meson.build" >/dev/null ||
  fail 'libcrypto dependency declaration shape drifted'


for forbidden in tools rejmerge MIGRATION.md; do
  [ ! -e "$root/$forbidden" ] || fail "forbidden frontend/compatibility surface remains: $forbidden"
done

if grep -RInE '/var/lib/pkg/rejected|filesystem_reconciler|liblinediff' \
    "$root/include" "$root/src" "$root/tests" --exclude='check_*' >/dev/null; then
  fail 'legacy rejected-tree or staging authority leaked into implementation/tests'
fi

printf '%s\n' 'architecture-contract: ok'
