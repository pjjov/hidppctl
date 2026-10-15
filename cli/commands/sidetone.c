/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

static int parse_channels(hidppctl_t *ctl, const char *s, int *mask) {
    int flags = 0;

    if (cli_is(s, "all")) {
        *mask = 0xFF;
        return HIDPP_OK;
    }

    /* "1,2,5" */
    for (const char *c = s; *c;) {
        char *end;
        long n = strtol(c, &end, 10);

        if (end == c || n < 1 || n > 8 || (*end && *end != ','))
            return cli_usage(
                ctl, "Channels must be 'all' or a list of 1 to 8."
            );

        flags |= 1 << (n - 1);
        c = *end ? end + 1 : end;
    }

    *mask = flags;
    return HIDPP_OK;
}

int cmd_sidetone(hidppctl_t *ctl) {
    pf_cli_t *cli = ctl->cli;
    char **argv = ctl->options->paramv;
    int argc = ctl->options->paramc;
    int rc, mask, mute;
    long n;

    if (argc == 0) {
        if ((rc = hidpp_sidetone_get(ctl->device)) < 0)
            return cli_fail(ctl, rc, "read the sidetone level");

        pf_cli_printf(cli, "Level: %d\n", rc);

        if ((rc = hidpp_sidetone_mute(ctl->device)) >= 0) {
            pf_cli_printf(cli, "Muted channels:");
            for (int i = 0; i < 8; i++)
                if (rc & (1 << i))
                    pf_cli_printf(cli, " %d", i + 1);
            pf_cli_printf(cli, "%s\n", rc ? "" : " none");
        }

        return HIDPP_OK;
    }

    if (cli_is(argv[0], "set") && argc == 2) {
        if (cli_long(ctl, argv[1], "level", 0, 100, &n))
            return HIDPP_EINVAL;
        if ((rc = hidpp_sidetone_set(ctl->device, n)))
            return cli_fail(ctl, rc, "set the sidetone level");
        return HIDPP_OK;
    }

    if (cli_is(argv[0], "mute") && argc == 3) {
        if (parse_channels(ctl, argv[1], &mask)
            || cli_bool(ctl, argv[2], "mute", &mute))
            return HIDPP_EINVAL;
        if ((rc = hidpp_sidetone_set_mute(ctl->device, mask, mute ? mask : 0)))
            return cli_fail(ctl, rc, "change the sidetone mute");
        return HIDPP_OK;
    }

    return cli_usage(
        ctl,
        "Expected no arguments, 'set <level>' or 'mute <channels> <on|off>'."
    );
}
