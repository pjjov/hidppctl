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
  -r, --receiver   Specifies which HID++ receiver to use.
  -d, --device     Specifies which HID++ device index to use.
  --timeout        Sets the timeout for IO operations in milliseconds.

Subcommands:
  info            shows information about HID++ devices.
  poll            polls specified devices for incoming events.
  divert          diverts events of reprogrammable buttons.
  remap           remaps device's control to a different one.

For more information, run `man hidppctl.1'.
```

For most subcommands, an exact receiver and device must be specified.
You can query this information using the `info` subcommand.

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
