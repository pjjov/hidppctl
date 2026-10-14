# hidppctl -- Configure HID++ compatible peripherals.
#
# Copyright (C) 2026 Предраг Јовановић
# SPDX-FileCopyrightText: 2026 Предраг Јовановић
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Look at the COPYING file for more information.

test_src = files(
  'fake.c',
  'main.c',
  'test_battery.c',
  'test_cache.c',
  'test_device.c',
  'test_action.c',
  'test_backlight.c',
  'test_device_name.c',
  'test_firmware.c',
  'test_hook.c',
  'test_hosts.c',
  'test_illumination.c',
  'test_keymap.c',
  'test_names.c',
  'test_nickname.c',
  'test_packet.c',
  'test_poll.c',
  'test_receiver.c',
  'test_robustness.c',
  'test_retry.c',
  'test_root.c',
)

# Assertions must stay active in every build type.
test_exe = executable(
  'hidpp-tests',
  sources: test_src,
  dependencies: [hidpp_dep, cpolyfill_dep],
  c_args: ['-UNDEBUG', '-D_POSIX_C_SOURCE=200809L'],
  install: false,
)

# One meson test per suite, so failures are attributed to a feature.
test_suites = [
  'packet',
  'names',
  'receiver',
  'hook',
  'device',
  'root',
  'firmware',
  'device_name',
  'nickname',
  'battery',
  'keymap',
  'poll',
  'cache',
  'robustness',
  'hosts',
  'backlight',
  'illumination',
  'action',
  'retry',
]

foreach suite : test_suites
  test(suite, test_exe, args: ['--no-color', suite], suite: 'hidpp')
endforeach
