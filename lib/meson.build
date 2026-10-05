# hidppctl -- Configure HID++ compatible peripherals.
#
# Copyright (C) 2026 Предраг Јовановић
# SPDX-FileCopyrightText: 2026 Предраг Јовановић
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Look at the COPYING file for more information.

lib_src = files(
  'features/battery.c',
  'features/device_name.c',
  'features/firmware_info.c',
  'features/keymap.c',
  'features/root.c',
  'cache.c',
  'device.c',
  'input.c',
  'names.c',
  'poll.c',
  'receiver.c',
  'util.c',
)

lib_src += tables_src

lib = library(
  'hidpp',
  dependencies: [hidapi_dep, allocator_dep],
  install: true,
  include_directories: inc,
  sources: lib_src,
)

hidpp_dep = declare_dependency(
  link_with: lib,
  include_directories: inc,
)

meson.override_dependency('hidpp', hidpp_dep)
