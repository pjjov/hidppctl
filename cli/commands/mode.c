/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

int cmd_mode(hidppctl_t *ctl) {
    pf_cli_t *cli = ctl->cli;
    char **argv = ctl->options->paramv;
    struct hidpp_mode_status_info *info = hidpp_mode_status_info(ctl->device);
    int rc, status;

    if (!info)
        return cli_unsupported(ctl, "mode status");

    if (ctl->options->paramc == 0) {
        pf_cli_printf(
            cli, "Hardware switch: %s\n", cli_yesno(info->hasHwSwitch)
        );
        pf_cli_printf(
            cli, "Software switch: %s\n", cli_yesno(info->hasSwSwitch)
        );
        pf_cli_printf(cli, "Can be set: %s\n", cli_yesno(info->canSet));

        if ((status = hidpp_mode_status_get(ctl->device)) >= 0)
            pf_cli_printf(
                cli,
                "Mode: %s\n",
                (status & HIDPP_MODE_PERFORMANCE) ? "performance" : "endurance"
            );

        return HIDPP_OK;
    }

    if (ctl->options->paramc == 1
        && (cli_is(argv[0], "performance") || cli_is(argv[0], "endurance"))) {
        status = cli_is(argv[0], "performance") ? HIDPP_MODE_PERFORMANCE : 0;

        if ((rc = hidpp_mode_status_set(
                 ctl->device, status, HIDPP_MODE_PERFORMANCE
             )))
            return cli_fail(ctl, rc, "change the mode");
        return HIDPP_OK;
    }

    return cli_usage(
        ctl, "Expected no arguments, 'performance' or 'endurance'."
    );
}
