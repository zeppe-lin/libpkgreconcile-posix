#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

[ "$#" -eq 2 ] || {
  echo "usage: $0 BUILD-DIR {shared|static}" >&2
  exit 2
}

build=$1
mode=$2
case $mode in
  shared|static) ;;
  *) exit 2 ;;
esac

root=$(CDPATH= cd "$(dirname "$0")/.." && pwd)
dependency_prefix=$(cat "$build/ci-dependency-prefix")
install_prefix=$(cat "$build/ci-install-prefix")
export PKG_CONFIG_PATH="$install_prefix/lib/pkgconfig:$dependency_prefix/lib/pkgconfig"
export LD_LIBRARY_PATH="$install_prefix/lib:$dependency_prefix/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

for header in export.h error.h inventory_store.h libpkgreconcile-posix.h; do
  [ -s "$install_prefix/include/libpkgreconcile-posix/$header" ] || {
    echo "installed-qualification: missing header $header" >&2
    exit 1
  }
done
[ -s "$install_prefix/lib/pkgconfig/libpkgreconcile-posix.pc" ] || {
  echo 'installed-qualification: pkg-config metadata missing' >&2
  exit 1
}

case $mode in
  shared)
    flags=$(pkg-config --cflags --libs libpkgreconcile-posix)
    ;;
  static)
    flags=$(pkg-config --static --cflags --libs libpkgreconcile-posix)
    ;;
esac

# shellcheck disable=SC2086
${CXX:-c++} -std=c++17 -Wall -Wextra -Wpedantic -Werror \
  "$root/ci/installed-consumer.cpp" $flags -o "$build/installed-consumer"
"$build/installed-consumer"

printf '%s\n' 'installed-qualification: ok'
