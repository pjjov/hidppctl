/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

int cmd_keymap(hidppctl_t *ctl) {
    pf_cli_t *cli = ctl->cli;

    hidpp_receiver_t *rcv = ctl->receiver;
    hidpp_device_t *dev = ctl->device;
    hidpp_keymap_t *map = hidpp_keymap(dev);

    if (!rcv || !dev)
        return HIDPP_EIO;

    if (!map)
        return cli_unsupported(ctl, "keymap features");

    uint16_t controls[256];
    size_t ctrlCount = hidpp_keymap_list(map, controls, 256);

    pf_cli_printf(cli, "Device has %zu remappable controls:\n", ctrlCount);

    for (size_t i = 0; i < ctrlCount; i++) {
        uint16_t ctrl = controls[i];
        struct hidpp_keymap_info *info = hidpp_keymap_info(map, ctrl);
        struct hidpp_keymap_state state;

        pf_cli_printf(
            cli, "  [0x%.4x] %s", ctrl, cli_str(hidpp_keymap_name(ctrl))
        );

        if (info) {
            pf_cli_printf(
                cli,
                " (%s%s%s)",
                info->isDivertable ? "divertable" : "fixed",
                info->isReprogrammable ? ", remappable" : "",
                info->isFnTogglable ? ", fn" : ""
            );
        }

        if (!hidpp_keymap_state(map, ctrl, &state)) {
            if (state.remapId && state.remapId != ctrl)
                pf_cli_printf(
                    cli,
                    " -> [0x%.4x] %s",
                    state.remapId,
                    cli_str(hidpp_keymap_name(state.remapId))
                );
            if (state.isDiverted)
                pf_cli_printf(cli, " [diverted]");
        }

        pf_cli_printf(cli, "\n");
    }

    return HIDPP_OK;
}
