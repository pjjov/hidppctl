<!--
    hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
-->

# hidppctl - Configure HID++ compatible peripherals.

**hidppctl** is a command-line utility that can be used to query
and configure devices that use the proprietary HID++ protocol.

Alongside the program, a complementary C library with same
functionality is provided.

## Usage

The command-line program supports following options:

```
usage: hidppctl [OPTIONS]... <command>

Configure HID++ compatible devices.

Options:
  -?, --help       Shows this information.
  -V, --version    Shows version information.
  -r, --receiver   Specifies which HID++ receiver to use.
  -d, --device     Specifies which HID++ device index to use.
  --host           Selects a host for host-specific commands.
  --sensor         Selects a motion sensor for the 'dpi' command.
  --zone           Selects a LED zone for the 'led' command.
  --timeout        Sets the timeout for IO operations in milliseconds.

For more information, run `man hidppctl.1'.
```

For most subcommands, an exact receiver and device must be specified.
You can query this information using the `status` subcommand. Run
`hidppctl --help` for the complete list of subcommands and
`hidppctl <subcommand> --help` for the details of a single one.

| Area              | Subcommands                                                          |
|-------------------|----------------------------------------------------------------------|
| Discovery         | `status`, `list-features`, `list-events`, `list-keycodes`, `cache`   |
| Buttons and keys  | `keymap`, `remap`, `divert`, `action`, `disable-keys`, `fn-inversion`|
| Hosts             | `host`, `platform`, `nickname`                                       |
| Power             | `battery`, `solar`, `mode`                                           |
| Pointing devices  | `dpi`, `pointer`, `report-rate`, `scroll`, `smartshift`, `wheel`, `thumbwheel`, `crown`, `touchpad`, `touch-mouse` |
| Lighting          | `backlight`, `illumination`, `brightness`, `led`, `perkey`           |
| Audio             | `sidetone`, `eq`                                                     |
| Finding devices   | `search` lists connected devices and offers to save an alias and the cache |
| Aliases           | `alias` saves device picking options under a name for `--alias`      |
| Events            | `poll` prints events of every supported feature                      |

Subcommands without arguments show the current state of the feature, for
example `hidppctl -r 046d:c548 -d 1 smartshift`. Settings are changed with
extra arguments, for example `smartshift set mode=ratchet torque=60`.

Repeating `-r`, `-d` and friends gets tedious, so they can be saved under a name:

```sh
hidppctl -r 046d:c548 -d 1 alias set mouse
hidppctl --alias mouse dpi
```

Both the program and the library can simulate some input, specifically keyboard presses,
using **ydotool** or **uinput** on Linux and the Win32 API on Windows systems.

For documentation, view the provided manual pages: `hidpp.3` and `hidppctl.1`.

## Building

**hidppctl** relies on Meson for building. Following command will install
the tool, C library with headers, manual pages and it's dependencies.

```sh
meson install
```

## License

See [COPYING](./COPYING) file for more information.
