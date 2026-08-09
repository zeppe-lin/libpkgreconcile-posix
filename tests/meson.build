# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later

sh = find_program('sh')
test_inc = include_directories('.')

unit_tests = [
  ['store-error', 'unit/store_error_test.cpp'],
]
foreach spec : unit_tests
  target = executable(
    spec[0] + '-test',
    spec[1],
    include_directories: test_inc,
    dependencies: libpkgreconcile_posix_dep,
    install: false,
  )
  test(spec[0], target, suite: 'unit')
endforeach

mechanism_tests = [
  ['initialization', 'mechanism/initialization_test.cpp'],
  ['publication', 'mechanism/publication_test.cpp'],
  ['resolution', 'mechanism/resolution_test.cpp'],
  ['anti-resurrection', 'mechanism/anti_resurrection_test.cpp'],
  ['locking', 'mechanism/locking_test.cpp'],
  ['root-authority', 'mechanism/root_authority_test.cpp'],
  ['corruption', 'mechanism/corruption_test.cpp'],
  ['recovery-refusal', 'mechanism/recovery_refusal_test.cpp'],
]
foreach spec : mechanism_tests
  target = executable(
    spec[0] + '-test',
    spec[1],
    include_directories: test_inc,
    dependencies: libpkgreconcile_posix_dep,
    install: false,
  )
  test(spec[0], target, suite: 'mechanism')
endforeach

public_headers = [
  ['export', 'libpkgreconcile-posix/export.h'],
  ['error', 'libpkgreconcile-posix/error.h'],
  ['inventory-store', 'libpkgreconcile-posix/inventory_store.h'],
  ['umbrella', 'libpkgreconcile-posix/libpkgreconcile-posix.h'],
]
foreach spec : public_headers
  target = executable(
    'header-' + spec[0] + '-test',
    'header/public_header_test.cpp',
    cpp_args: ['-DLIBPKGRECONCILE_POSIX_TEST_HEADER="' + spec[1] + '"'],
    dependencies: libpkgreconcile_posix_dep,
    install: false,
  )
  test('header-' + spec[0], target, suite: 'header')
endforeach

umbrella_header_test = executable(
  'umbrella-header-contract-test',
  'header/umbrella_header_test.cpp',
  dependencies: libpkgreconcile_posix_dep,
  install: false,
)
test('header-umbrella-contract', umbrella_header_test, suite: 'header')

if library_kind == 'shared'
  nm = find_program('nm')
  test(
    'abi-surface',
    sh,
    args: [
      meson.current_source_dir() / 'contracts/check_abi_surface.sh',
      nm.full_path(),
      libpkgreconcile_posix.full_path(),
      meson.project_source_root() / 'abi/libpkgreconcile-posix.exports',
    ],
    suite: 'contract',
  )
endif

foreach contract : [
  'architecture_contract',
  'ci_qualification_source',
  'documentation_source',
  'doxygen_contract',
  'release_metadata',
  'repository_contract',
  'style_contract',
  'storage_contract',
  'test_layout',
]
  test(
    contract.replace('_', '-'),
    sh,
    args: [
      meson.current_source_dir() / ('contracts/check_' + contract + '.sh'),
      meson.project_source_root(),
    ],
    suite: 'contract',
  )
endforeach

test(
  'pkgconfig-metadata',
  sh,
  args: [
    meson.current_source_dir() / 'contracts/check_pkgconfig_metadata.sh',
    meson.project_build_root(),
    meson.project_version(),
  ],
  suite: 'contract',
)
