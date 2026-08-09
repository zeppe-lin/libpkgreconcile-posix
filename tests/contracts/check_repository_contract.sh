#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

root=$1
fail()
{
  echo "repository-contract: $*" >&2
  exit 1
}

for required in \
  include/libpkgreconcile-posix/export.h \
  include/libpkgreconcile-posix/error.h \
  include/libpkgreconcile-posix/inventory_store.h \
  include/libpkgreconcile-posix/libpkgreconcile-posix.h \
  src/error.cpp src/inventory_store.cpp \
  abi/libpkgreconcile-posix.exports \
  scripts/generate-elf-export-script.sh \
  STORAGE.md tests/meson.build \
  ci/configure-and-test.sh ci/qualify-installed.sh ci/installed-consumer.cpp \
  .github/workflows/ci.yml; do
  [ -s "$root/$required" ] || fail "missing or empty $required"
done

for directory in tests/unit tests/mechanism tests/header tests/support tests/contracts; do
  [ -d "$root/$directory" ] || fail "missing $directory"
done

[ ! -d "$root/tools" ] || fail 'frontend tool directory is forbidden'

printf '%s\n' 'repository-contract: ok'
