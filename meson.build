# hidppctl -- Configure HID++ compatible peripherals.
#
# Copyright (C) 2026 Предраг Јовановић
# SPDX-FileCopyrightText: 2026 Предраг Јовановић
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Look at the COPYING file for more information.

project('hidppctl', 'c')

allocator_dep = dependency('allocator_t', required: true)
cpolyfill_dep = dependency('cpolyfill', required: true)
hidapi_dep = dependency('hidapi', required: true)

python = find_program('python3')

table_files = files(
  'data/controls.txt',
  'data/features.txt',
)

generated_tables = custom_target(
  'constant_tables',
  input: table_files,
  output: 'constant_tables_generated.c',
  command: [
    python,
    files('data/embed.py'),
    '--output', '@OUTPUT@',
    '--input', '@INPUT@',
  ],
  build_by_default: true,
)

inc = include_directories('include')

cli_src = [
    'cli/args.c',
    'cli/main.c',
]

lib_src = [
    'lib/features/firmware_info.c',
    'lib/features/keymap.c',
    'lib/features/root.c',
    'lib/device.c',
    'lib/input.c',
    'lib/names.c',
    'lib/poll.c',
    'lib/receiver.c',
    'lib/util.c',
    generated_tables,
]

lib = library(
    'hidpp',
    dependencies: [hidapi_dep, allocator_dep],
    install: true,
    include_directories: inc,
    sources: lib_src
)

hidpp_dep = declare_dependency(
    link_with: [lib],
    include_directories: inc,
)

meson.override_dependency('hidpp', hidpp_dep)

executable(
    'hidppctl',
    dependencies: [hidpp_dep, cpolyfill_dep],
    install: true,
    sources: cli_src
)

install_headers('include/hidpp.h')

install_man(
    'docs/libhidpp.3',
    'docs/hidppctl.1'
)
