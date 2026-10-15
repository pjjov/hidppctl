# hidppctl -- Configure HID++ compatible peripherals.
#
# Copyright (C) 2026 Предраг Јовановић
# SPDX-FileCopyrightText: 2026 Предраг Јовановић
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Look at the COPYING file for more information.

cli_src = files(
  'commands/action.c',
  'commands/backlight.c',
  'commands/battery.c',
  'commands/brightness.c',
  'commands/cache.c',
  'commands/crown.c',
  'commands/disable_keys.c',
  'commands/divert.c',
  'commands/dpi.c',
  'commands/equalizer.c',
  'commands/fn_inversion.c',
  'commands/host.c',
  'commands/illumination.c',
  'commands/keymap.c',
  'commands/led.c',
  'commands/list_events.c',
  'commands/list_features.c',
  'commands/list_keycodes.c',
  'commands/mode.c',
  'commands/nickname.c',
  'commands/perkey.c',
  'commands/platform.c',
  'commands/pointer.c',
  'commands/poll.c',
  'commands/remap.c',
  'commands/report_rate.c',
  'commands/scroll.c',
  'commands/sidetone.c',
  'commands/smartshift.c',
  'commands/solar.c',
  'commands/status.c',
  'commands/thumbwheel.c',
  'commands/touch.c',
  'commands/wheel.c',
  'args.c',
  'cache.c',
  'commands.c',
  'log.c',
  'main.c',
  'util.c',
)

executable(
  'hidppctl',
  dependencies: [hidpp_dep, cpolyfill_dep],
  install: true,
  sources: cli_src,
)
