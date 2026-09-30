/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

extern struct {
    const char name[12];
    int code;
} hidpp_input_table[];

int cmd_list_keycodes(hidppctl_t *ctl) {
    pf_cli_t *cli = ctl->cli;
    pf_cli_printf(
        cli, "Aside from numeric values, key codes also have following aliases:"
    );

    size_t max = cli->out.columns;
    size_t length = max;

    for (size_t i = 0; hidpp_input_table[i].code; i++) {
        size_t curr = strlen(hidpp_input_table[i].name) + 4;
        length += curr;

        if (length >= max) {
            pf_cli_printf(cli, "\n  ");
            length = 2 + curr;
        }

        pf_cli_printf(cli, "'%s', ", hidpp_input_table[i].name);
    }

    pf_cli_printf(cli, "\n");
    return HIDPP_OK;
}