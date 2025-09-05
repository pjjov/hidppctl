/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2025 Предраг Јовановић
    SPDX-FileCopyrightText: 2025 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "hidpp.h"

enum hidpp_report_types {
    HIDPP_SHORT = 0x10,
    HIDPP_LONG = 0x11,
};

#define HIDPP_MAKE(size, dev, featid, fn, ...) \
    { HIDPP_##size, (dev)->id, (featid), (fn << 4) | (dev)->swid, __VA_ARGS__ }

#define HIDPP_WORD(msb, lsb) (((uint16_t)(msb) << 8) | (uint16_t)(lsb))

typedef uint8_t hidpp_report[20];

struct hidpp_device {
    hid_device *handle;
};
