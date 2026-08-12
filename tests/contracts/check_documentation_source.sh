#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

root=$1
fail()
{
  echo "documentation-source-contract: $*" >&2
  exit 1
}

for required in README.md DESIGN.md STORAGE.md HISTORY.md CONTRIBUTING.md MAINTAINING.md TESTING.md man/libpkgreconcile-posix.3.scdoc; do
  [ -s "$root/$required" ] || fail "missing or empty $required"
done

for document in README.md DESIGN.md STORAGE.md HISTORY.md CONTRIBUTING.md MAINTAINING.md TESTING.md; do
  first=$(sed -n '1p' "$root/$document")
  case $first in
    '# '*) ;;
    *) fail "$document does not start with an ATX level-one heading" ;;
  esac
done

if grep -RInE '^[-=]{3,}$' "$root"/*.md >/dev/null; then
  fail 'Setext Markdown heading remains in root documentation'
fi

# No old rejected-tree compatibility contract belongs in the native provider.
if grep -RInE '/var/lib/pkg/rejected|filesystem_reconciler|liblinediff|\brejmerge\b' \
    "$root/README.md" "$root/DESIGN.md" "$root/STORAGE.md" \
    "$root/CONTRIBUTING.md" "$root/MAINTAINING.md" "$root/TESTING.md" \
    "$root/man" "$root/include" >/dev/null; then
  fail 'legacy rejected-tree/frontend vocabulary leaked into current documentation'
fi

# This provider persists opaque locators but does not mint neighboring provider
# protocol identifiers.
if grep -RInE 'libpkgapply-posix/rejected|pkgctl/native-target|reconcile-posix/object' \
    "$root/README.md" "$root/DESIGN.md" "$root/STORAGE.md" "$root/man" "$root/include" >/dev/null; then
  fail 'documentation invents a provider identifier owned by another boundary'
fi

grep -Fi 'renameat2(RENAME_NOREPLACE)' "$root/README.md" >/dev/null ||
  fail 'README omits the Linux no-replace host requirement'
grep -Fi 'Linux-hosted POSIX mechanism code' "$root/DESIGN.md" >/dev/null ||
  fail 'DESIGN omits the actual host mechanism boundary'
grep -Fi 'std::filesystem::path::native()' "$root/STORAGE.md" >/dev/null ||
  fail 'STORAGE omits the persisted native path-byte contract'
grep -Fi 'file opens are non-blocking' "$root/STORAGE.md" >/dev/null ||
  fail 'STORAGE omits non-blocking authoritative-file refusal'

for document in README.md DESIGN.md STORAGE.md; do
  grep -Fi 'target' "$root/$document" >/dev/null || fail "$document omits target binding"
  grep -Fi 'resolved' "$root/$document" >/dev/null || fail "$document omits resolved tombstones"
done

grep -F 'does not' "$root/README.md" >/dev/null ||
  fail 'README does not state non-authorities'

printf '%s\n' 'documentation-source-contract: ok'
