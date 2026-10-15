/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

static int cmd_divert_init(hidppctl_t *ctl, hidpp_keymap_t *map) {
    struct diversion *diversions = ctl->options->divert.items;

    if (!map) {
        pf_cli_errorf(
            ctl->cli, "Specified device doesn't support diversion!\n"
        );
        return HIDPP_EIO;
    }

    for (int i = 0; i < ctl->options->paramc; i++) {
        uint16_t ctrl = diversions[i].ctrl;

        if (hidpp_keymap_divert(map, ctrl, HIDPP_TRUE)) {
            pf_cli_errorf(
                ctl->cli, "Unable to divert the control with id 0x%.4x!\n", ctrl
            );
            return HIDPP_EIO;
        }
    }

    return HIDPP_OK;
}

static int cmd_divert_term(hidppctl_t *ctl, hidpp_keymap_t *map) {
    struct diversion *diversions = ctl->options->divert.items;

    for (int i = 0; i < ctl->options->paramc; i++) {
        uint16_t ctrl = diversions[i].ctrl;

        if (hidpp_keymap_divert(map, ctrl, HIDPP_FALSE)) {
            pf_cli_errorf(
                ctl->cli,
                "Unable to undivert the control with id 0x%.4x!\n",
                ctrl
            );
        }
    }

    return HIDPP_OK;
}

static void cmd_divert_set(
    struct diversion *div, hidpp_input_t *input, int value
) {
    if (div->state == value)
        return;

    hidpp_input_set(input, div->key, div->mods, div->count, value);
    div->state = value;
}

static int cmd_divert_poll(
    hidppctl_t *ctl, hidpp_device_t *dev, hidpp_keymap_t *map
) {
    struct diversion *diversions = ctl->options->divert.items;
    struct hidpp_event e;

    if (hidpp_device_poll(dev, &e))
        return HIDPP_EIO;

    if (e.type != HIDPP_EVENT_BUTTON)
        return HIDPP_ENOSYS;

    for (int i = 0; i < ctl->options->paramc; i++) {
        struct diversion *div = &diversions[i];
        int value = 0;

        for (int j = 0; j < 4; j++)
            if (e.as.buttons[j] == div->ctrl)
                value = 1;

        if (value != div->state)
            cmd_divert_set(div, ctl->input, value);
    }

    return HIDPP_OK;
}

int cmd_divert(hidppctl_t *ctl) {
    hidpp_device_t *dev = ctl->device;
    hidpp_keymap_t *map = hidpp_keymap(dev);

    if (cmd_divert_init(ctl, map))
        return HIDPP_EIO;

    cli_catch_signals();
    pf_cli_cprintf(ctl->cli, PF_CLI_BOLD, "Type Ctrl+C to stop the program.\n");
    pf_cli_color(ctl->cli, PF_CLI_RESET);

    while (!cli_terminated())
        cmd_divert_poll(ctl, dev, map);

    cmd_divert_term(ctl, map);

    return HIDPP_OK;
}