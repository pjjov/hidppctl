/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

int cmd_scroll(hidppctl_t *ctl) {
    pf_cli_t *cli = ctl->cli;
    hidpp_device_t *dev = ctl->device;
    char **argv = ctl->options->paramv;
    struct hidpp_scroll_info *info = hidpp_scroll_info(dev);
    int rc, value;

    if (!info)
        return cli_unsupported(ctl, "scroll wheel configuration");

    if (ctl->options->paramc == 2 && cli_is(argv[0], "highres")) {
        if (cli_bool(ctl, argv[1], "highres", &value))
            return HIDPP_EINVAL;
        if ((rc = hidpp_scroll_set_highres(dev, value)))
            return cli_fail(ctl, rc, "change high resolution scrolling");
        return HIDPP_OK;
    }

    if (ctl->options->paramc != 0)
        return cli_usage(ctl, "Expected no arguments or 'highres <on|off>'.");

    if (info->hasRollerInfo) {
        pf_cli_printf(cli, "Roller: %s\n", cli_str(info->rollerTypeName));
        pf_cli_printf(cli, "Ratchets per turn: %u\n", info->ratchetsPerTurn);
        if (info->scrollLines == 0)
            pf_cli_printf(cli, "Lines per ratchet: OS setting\n");
        else if (info->scrollLines == 0xFF)
            pf_cli_printf(cli, "Lines per ratchet: one page\n");
        else
            pf_cli_printf(cli, "Lines per ratchet: %u\n", info->scrollLines);
        pf_cli_printf(
            cli, "Counts per ratchet (high res): %u\n", info->multiplier
        );
    }

    if (info->hasHighRes) {
        if ((value = hidpp_scroll_highres(dev)) >= 0)
            pf_cli_printf(cli, "High resolution: %s\n", cli_onoff(value));
    }

    return HIDPP_OK;
}
