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

src = [
    'src/features/firmware_info.c',
    'src/features/keymap.c',
    'src/features/root.c',
    'src/device.c',
    'src/input.c',
    'src/names.c',
    'src/poll.c',
    'src/receiver.c',
    'src/util.c',
    generated_tables,
]

lib = library(
    'hidpp',
    dependencies: [hidapi_dep, allocator_dep],
    install: true,
    include_directories: inc,
    sources: src
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
    sources: ['src/hidppctl.c']
)

install_headers('include/hidpp.h')

install_man(
    'docs/libhidpp.3',
    'docs/hidppctl.1'
)
