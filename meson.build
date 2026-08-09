# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later

project(
  'libpkgreconcile-posix',
  'cpp',
  version: '0.1.0',
  license: 'GPL-3.0-or-later',
  meson_version: '>=1.6.0',
  default_options: [
    'cpp_std=c++17',
    'warning_level=3',
    'default_library=shared',
    'b_ndebug=if-release',
  ],
)

library_kind = get_option('default_library')
if library_kind == 'both'
  error('libpkgreconcile-posix requires separate shared and static builds')
endif
want_static = get_option('link_mode') == 'static'
if (library_kind == 'static') != want_static
  error('default_library and link_mode must both select shared or static')
endif

cpp = meson.get_compiler('cpp')
visibility_cpp_args = []
if cpp.has_argument('-fvisibility-inlines-hidden')
  visibility_cpp_args += ['-fvisibility-inlines-hidden']
endif

pkgconf = import('pkgconfig')
public_inc = include_directories('include')
libpkgreconcile_dep = dependency(
  'libpkgreconcile',
  version: '>=0.3.0',
  required: true,
  static: want_static,
)
libcrypto_dep = dependency('libcrypto', required: true, static: want_static)

elf_export_generator = find_program(
  meson.project_source_root() / 'scripts/generate-elf-export-script.sh',
)
elf_export_script_name = 'libpkgreconcile-posix.exports.map'
elf_export_script_path = meson.current_build_dir() / elf_export_script_name
elf_export_script = configure_file(
  input: 'abi/libpkgreconcile-posix.exports',
  output: elf_export_script_name,
  command: [elf_export_generator, '@INPUT@'],
  capture: true,
)

libpkgreconcile_posix = library(
  'pkgreconcile-posix',
  'src/error.cpp',
  'src/inventory_store.cpp',
  include_directories: public_inc,
  dependencies: [libpkgreconcile_dep, libcrypto_dep],
  install: true,
  version: meson.project_version(),
  soversion: '0',
  gnu_symbol_visibility: 'hidden',
  cpp_args: visibility_cpp_args,
  link_args: ['-Wl,--version-script=' + elf_export_script_path],
  link_depends: [elf_export_script],
)

libpkgreconcile_posix_dep = declare_dependency(
  link_with: libpkgreconcile_posix,
  include_directories: public_inc,
  dependencies: [libpkgreconcile_dep, libcrypto_dep],
)
meson.override_dependency('libpkgreconcile-posix', libpkgreconcile_posix_dep)

install_headers(
  'include/libpkgreconcile-posix/export.h',
  'include/libpkgreconcile-posix/error.h',
  'include/libpkgreconcile-posix/inventory_store.h',
  'include/libpkgreconcile-posix/libpkgreconcile-posix.h',
  subdir: 'libpkgreconcile-posix',
)

pkgconf.generate(
  libpkgreconcile_posix,
  name: 'libpkgreconcile-posix',
  filebase: 'libpkgreconcile-posix',
  description: 'Durable POSIX reconciliation inventory provider',
  subdirs: '.',
  requires: ['libpkgreconcile >= 0.3.0'],
  requires_private: ['libcrypto'],
)

subdir('man')
if get_option('tests').allowed()
  subdir('tests')
endif
