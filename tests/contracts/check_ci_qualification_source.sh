#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

root=$1
workflow=$root/.github/workflows/ci.yml
runner=$root/ci/configure-and-test.sh
fail()
{
  echo "ci-qualification-source-contract: $*" >&2
  exit 1
}

[ -s "$workflow" ] || fail 'CI workflow is missing'
[ -s "$runner" ] || fail 'CI configure/test runner is missing'
for suite in unit mechanism header contract; do
  grep -F 'for suite in unit mechanism header contract' "$runner" >/dev/null ||
    fail "CI runner does not enumerate the $suite suite explicitly"
done
for mode in shared static; do
  grep -F "default_library: $mode" "$workflow" >/dev/null ||
    fail "workflow does not qualify $mode linkage"
done
for compiler in 'compiler: g++' 'compiler: clang++'; do
  grep -F "$compiler" "$workflow" >/dev/null ||
    fail "workflow does not qualify $compiler"
done
grep -F 'b_sanitize=address,undefined' "$workflow" >/dev/null ||
  fail 'workflow does not qualify ASan+UBSan'
grep -F 'doxygen Doxyfile' "$workflow" >/dev/null ||
  fail 'workflow does not validate Doxygen'
grep -F 'ref: v0.3.0' "$workflow" >/dev/null ||
  fail 'workflow does not pin libpkgreconcile 0.3.0'

printf '%s\n' 'ci-qualification-source-contract: ok'
