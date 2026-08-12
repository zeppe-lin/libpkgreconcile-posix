#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

root=$1
source=$root/src/inventory_store.cpp
storage=$root/STORAGE.md
fail()
{
  echo "storage-contract: $*" >&2
  exit 1
}

for magic in ZLRBND01 ZLRINV01; do
  grep -F "$magic" "$storage" >/dev/null || fail "STORAGE.md omits $magic"
done
# Source spells the magic byte-wise; require the exact byte sequence there.
grep -F "'Z', 'L', 'R', 'B', 'N', 'D', '0', '1'" "$source" >/dev/null ||
  fail 'binding format magic drifted from version 1'
grep -F "'Z', 'L', 'R', 'I', 'N', 'V', '0', '1'" "$source" >/dev/null ||
  fail 'inventory format magic drifted from version 1'
grep -F 'v1:sha256:' "$source" >/dev/null || fail 'generation identity version drifted'
grep -F 'v1:sha256:' "$storage" >/dev/null || fail 'storage documentation omits identity version'
grep -F 'O_NONBLOCK' "$source" >/dev/null || fail 'authoritative file reads may block on special files'
grep -F 'Version 0.1.0 never removes them automatically' "$storage" >/dev/null ||
  fail 'storage document does not pin tombstone-retention policy'

printf '%s\n' 'storage-contract: ok'
