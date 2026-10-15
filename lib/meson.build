# hidppctl -- Configure HID++ compatible peripherals.
#
# Copyright (C) 2026 Предраг Јовановић
# SPDX-FileCopyrightText: 2026 Предраг Јовановић
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Look at the COPYING file for more information.

lib_src = files(
  'features/action.c',
  'features/backlight.c',
  'features/battery.c',
  'features/brightness.c',
  'features/crown.c',
  'features/device_name.c',
  'features/disable_keys.c',
  'features/dpi.c',
  'features/equalizer.c',
  'features/firmware_info.c',
  'features/fn_inversion.c',
  'features/hires_wheel.c',
  'features/hosts.c',
  'features/illumination.c',
  'features/keymap.c',
  'features/led.c',
  'features/mode_status.c',
  'features/nickname.c',
  'features/perkey.c',
  'features/platform.c',
  'features/pointer.c',
  'features/report_rate.c',
  'features/root.c',
  'features/scrolling.c',
  'features/sidetone.c',
  'features/smartshift.c',
  'features/solar.c',
  'features/thumbwheel.c',
  'features/touch_mouse.c',
  'features/touchpad.c',
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
  dependencies: [hidapi_dep, allocator_dep, cpolyfill_dep],
  install: true,
  include_directories: inc,
  sources: lib_src,
)

hidpp_dep = declare_dependency(
  link_with: lib,
  include_directories: inc,
)

meson.override_dependency('hidpp', hidpp_dep)
