/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

int cmd_remap(hidppctl_t *ctl) {
    uint16_t ctrl = ctl->options->remap.ctrlId;
    uint16_t remap = ctl->options->remap.remapId;

    hidpp_receiver_t *rcv = ctl->receiver;
    hidpp_device_t *dev = ctl->device;
    hidpp_keymap_t *map;

    if (!(map = hidpp_keymap(dev))) {
        pf_cli_errorf(
            ctl->cli, "Specified device doesn't support control remapping.\n"
        );
        return HIDPP_EIO;
    }

    if (hidpp_keymap_remap(map, ctrl, remap)) {
        pf_cli_errorf(
            ctl->cli, "Unable to remap control: %ls\n", hidpp_error(rcv)
        );
        return HIDPP_EIO;
    }

    return HIDPP_OK;
}
