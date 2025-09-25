/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2025 Предраг Јовановић
    SPDX-FileCopyrightText: 2025 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#ifndef HIDPPCTL_PROTOCOL_H
#define HIDPPCTL_PROTOCOL_H

enum {
    MSG_IGNORE = 0,
    MSG_SHUTDOWN,
};

struct message {
    int kind;
    union {
        char reserved[248];
    } as;
};

#endif
