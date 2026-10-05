# hidppctl -- Configure HID++ compatible peripherals.
#
# Copyright (C) 2026 Предраг Јовановић
# SPDX-FileCopyrightText: 2026 Предраг Јовановић
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Look at the COPYING file for more information.

cli_src = files(
  'commands/cache.c',
  'commands/divert.c',
  'commands/keymap.c',
  'commands/list_events.c',
  'commands/list_features.c',
  'commands/list_keycodes.c',
  'commands/poll.c',
  'commands/remap.c',
  'commands/status.c',
  'args.c',
  'cache.c',
  'log.c',
  'main.c',
)

executable(
  'hidppctl',
  dependencies: [hidpp_dep, cpolyfill_dep],
  install: true,
  sources: cli_src,
)
