# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later

scdoc = find_program('scdoc', required: get_option('man_pages'))
if scdoc.found()
  manpage = custom_target(
    'libpkgreconcile-posix.3',
    input: 'libpkgreconcile-posix.3.scdoc',
    output: 'libpkgreconcile-posix.3',
    command: [scdoc],
    feed: true,
    capture: true,
    install: true,
    install_dir: get_option('mandir') / 'man3',
  )
endif
