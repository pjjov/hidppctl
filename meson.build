# hidppctl -- Configure HID++ compatible peripherals.
#
# Copyright (C) 2025 Предраг Јовановић
# SPDX-FileCopyrightText: 2025 Предраг Јовановић
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Look at the COPYING file for more information.

project('hidppctl', 'c')

hidapi_dep = dependency('hidapi', required: true)
xdo_dep = declare_dependency(link_args : ['-lxdo'])

executable(
    'hidppctl',
    dependencies: [hidapi_dep, xdo_dep],
    sources: [
        'src/daemon.c',
        'src/hidppctl.c',
        'src/hidpp.c',
        'src/names.c',
    ]
)
