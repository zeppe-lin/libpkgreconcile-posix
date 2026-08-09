#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

root=$1
fail()
{
  echo "style-contract: $*" >&2
  exit 1
}

for required in .clang-format .editorconfig; do
  [ -s "$root/$required" ] || fail "missing $required"
done

if grep -RIn "$(printf '\t')" "$root/include" "$root/src" "$root/tests" \
    --include='*.h' --include='*.hpp' --include='*.cpp' --include='*.sh' >/dev/null; then
  fail 'source or test file contains tab characters'
fi

if grep -RIn '[[:blank:]]$' "$root/include" "$root/src" "$root/tests" \
    --include='*.h' --include='*.hpp' --include='*.cpp' --include='*.sh' >/dev/null; then
  fail 'source or test file contains trailing whitespace'
fi

printf '%s\n' 'style-contract: ok'
