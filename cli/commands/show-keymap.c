/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

int cmd_show_keymap(hidppctl_t *ctl) {
    pf_cli_t *cli = ctl->cli;

    hidpp_receiver_t *rcv = ctl->receiver;
    hidpp_device_t *dev = ctl->device;
    hidpp_keymap_t *map = hidpp_keymap(dev);

    if (!rcv || !dev)
        return HIDPP_EIO;

    if (!map) {
        pf_cli_printf(
            cli, "Selected device doesn't support keymap features!\n"
        );
        return HIDPP_OK;
    }

    uint16_t controls[256];
    size_t ctrlCount = hidpp_keymap_list(map, controls, 256);

    pf_cli_printf(cli, "Device has %u remappable controls:\n", ctrlCount);

    for (int i = 0; i < ctrlCount; i++) {
        uint16_t ctrl = hidpp_keymap_id(map, i);
        pf_cli_printf(cli, "  [0x%.4x] %s\n", ctrl, hidpp_keymap_name(ctrl));
    }

    return HIDPP_OK;
}