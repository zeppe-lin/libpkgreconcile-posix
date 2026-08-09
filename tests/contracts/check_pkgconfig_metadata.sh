#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

build=$1
expected_version=$2
pc=$build/meson-private/libpkgreconcile-posix.pc
fail()
{
  echo "pkgconfig-metadata-contract: $*" >&2
  [ ! -f "$pc" ] || cat "$pc" >&2
  exit 1
}

[ -s "$pc" ] || fail 'generated libpkgreconcile-posix.pc is missing'
version=$(sed -n 's/^Version:[[:space:]]*//p' "$pc")
[ "$version" = "$expected_version" ] ||
  fail "version is '$version', expected '$expected_version'"

grep -E '^Requires:[[:space:]].*libpkgreconcile[[:space:]]*>=[[:space:]]*0\.3\.0' "$pc" >/dev/null ||
  fail 'public libpkgreconcile dependency is missing or unversioned'
grep -E '^Requires\.private:[[:space:]].*libcrypto' "$pc" >/dev/null ||
  fail 'private libcrypto dependency is missing'

printf '%s\n' 'pkgconfig-metadata-contract: ok'
