/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

int cmd_nickname(hidppctl_t *ctl) {
    pf_cli_t *cli = ctl->cli;
    hidpp_device_t *dev = ctl->device;
    char **argv = ctl->options->paramv;
    int rc;

    if (ctl->options->paramc == 0) {
        const char *nick = hidpp_nickname_get(dev);

        if (!nick)
            return cli_unsupported(ctl, "nicknames");

        pf_cli_printf(cli, "Nickname: %s\n", nick);
        pf_cli_printf(
            cli, "Default: %s\n", cli_str(hidpp_nickname_default(dev))
        );
        return HIDPP_OK;
    }

    if (cli_is(argv[0], "reset") && ctl->options->paramc == 1) {
        if ((rc = hidpp_nickname_reset(dev)))
            return cli_fail(ctl, rc, "reset the nickname");
        return HIDPP_OK;
    }

    if (cli_is(argv[0], "set") && ctl->options->paramc == 2) {
        if ((rc = hidpp_nickname_set(dev, argv[1])))
            return cli_fail(ctl, rc, "set the nickname");
        return HIDPP_OK;
    }

    return cli_usage(ctl, "Expected 'set <name>' or 'reset'.");
}
