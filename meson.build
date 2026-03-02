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

inc = include_directories('include')

lib = library(
    'hidpp',
    dependencies: [hidapi_dep, allocator_dep],
    install: true,
    include_directories: inc,
    sources: [
        'src/hidpp.c',
        'src/names.c',
        'src/input.c',
    ]
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
