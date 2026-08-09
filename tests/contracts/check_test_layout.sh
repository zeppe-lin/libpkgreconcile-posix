#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

root=$1
meson=$root/tests/meson.build
fail()
{
  echo "test-layout-contract: $*" >&2
  exit 1
}

for directory in unit mechanism header support contracts; do
  [ -d "$root/tests/$directory" ] || fail "missing tests/$directory"
done
for suite in unit mechanism header contract; do
  grep -F "suite: '$suite'" "$meson" >/dev/null ||
    fail "Meson does not register the $suite suite"
done

if find "$root/tests" -maxdepth 1 -type f \
    \( -name '*.cpp' -o -name '*.hpp' -o -name '*.sh' \) | grep . >/dev/null; then
  fail 'uncategorized test source remains at tests/ root'
fi

for source in "$root"/tests/unit/*_test.cpp "$root"/tests/mechanism/*_test.cpp "$root"/tests/header/*_test.cpp; do
  relative=${source#"$root/tests/"}
  grep -F "$relative" "$meson" >/dev/null ||
    fail "test source is not registered: $relative"
done

for source in "$root"/tests/contracts/*.sh; do
  base=$(basename "$source")
  stem=${base#check_}
  stem=${stem%.sh}
  if ! grep -F "$base" "$meson" >/dev/null && ! grep -F "'$stem'" "$meson" >/dev/null; then
    fail "contract is not registered: $base"
  fi
done

if grep -E "test\('[^']*:" "$meson" >/dev/null; then
  fail 'Meson test names must not contain colon separators'
fi

printf '%s\n' 'test-layout-contract: ok'
