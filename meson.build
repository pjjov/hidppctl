# hidppctl -- Configure HID++ compatible peripherals.
#
# Copyright (C) 2026 Предраг Јовановић
# SPDX-FileCopyrightText: 2026 Предраг Јовановић
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Look at the COPYING file for more information.

project('hidppctl', 'c', version: '0.1.0')

allocator_dep = dependency('allocator_t', required: true)
cpolyfill_dep = dependency('cpolyfill', required: true)
hidapi_dep = dependency('hidapi', required: true)

inc = include_directories('include')

subdir('data')
subdir('lib')
subdir('cli')

install_headers('include/hidpp.h')

install_man(
    'docs/libhidpp.3',
    'docs/hidppctl.1'
)