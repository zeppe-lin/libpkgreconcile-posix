#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

root=$1
fail()
{
  echo "release-metadata-contract: $*" >&2
  exit 1
}

grep -F "version: '0.1.0'" "$root/meson.build" >/dev/null ||
  fail 'Meson project version is not 0.1.0'
grep -F "meson_version: '>=1.6.0'" "$root/meson.build" >/dev/null ||
  fail 'Meson compatibility floor is not 1.6.0'
grep -F "soversion: '0'" "$root/meson.build" >/dev/null ||
  fail 'shared-library ABI generation is not 0'
grep -F 'PROJECT_NUMBER         = 0.1.0' "$root/Doxyfile" >/dev/null ||
  fail 'Doxygen project version is stale'
grep -F '## 0.1.0' "$root/HISTORY.md" >/dev/null ||
  fail '0.1.0 history entry is missing'
[ -s "$root/abi/libpkgreconcile-posix.exports" ] ||
  fail 'reviewed shared ABI manifest is missing'

printf '%s\n' 'release-metadata-contract: ok'
