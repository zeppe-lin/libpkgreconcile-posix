#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

root=$1
config=$root/Doxyfile
fail()
{
  echo "doxygen-contract: $*" >&2
  exit 1
}

[ -s "$config" ] || fail 'Doxyfile is missing'
for setting in \
  'EXTRACT_ALL            = NO' \
  'WARN_IF_UNDOCUMENTED   = YES' \
  'WARN_IF_DOC_ERROR      = YES' \
  'WARN_NO_PARAMDOC       = YES' \
  'WARN_AS_ERROR          = FAIL_ON_WARNINGS' \
  'GENERATE_HTML           = YES' \
  'GENERATE_LATEX          = NO'; do
  grep -F "$setting" "$config" >/dev/null ||
    fail "missing Doxygen contract setting: $setting"
done

for header in "$root"/include/libpkgreconcile-posix/*.h; do
  grep -F '@file' "$header" >/dev/null ||
    fail "public header lacks a file contract: ${header#$root/}"
done

printf '%s\n' 'doxygen-contract: ok'
