/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

const char *hidppctl_event_names[HIDPP__EVENT_MAX] = {
    "NONE",
    "UNKNOWN",
    "BUTTON",
    "MOUSE",
    "BATTERY",
    "WHEEL",
    "RATCHET",
    "RATCHET_SWITCH",
    "TOUCH_PAD_POINTS",
    "TOUCH_MOUSE_POINTS",
    "TOUCH_MOUSE_STATUS",
};

int cmd_list_events(hidppctl_t *ctl) {
    pf_cli_t *cli = ctl->cli;
    pf_cli_printf(cli, "Supported event types:");

    size_t max = cli->out.columns;
    size_t length = max;

    for (int i = 0; i < HIDPP__EVENT_MAX; i++) {
        size_t curr = strlen(hidppctl_event_names[i]) + 4;
        length += curr;

        if (length >= max) {
            pf_cli_printf(cli, "\n  ");
            length = 2 + curr;
        }

        pf_cli_printf(cli, "'%s', ", hidppctl_event_names[i]);
    }

    pf_cli_printf(cli, "\n");
    return HIDPP_OK;
}