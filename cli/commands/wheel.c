/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

static int wheel_show(hidppctl_t *ctl) {
    pf_cli_t *cli = ctl->cli;
    struct hidpp_wheel_info *info = hidpp_wheel_info(ctl->device);
    struct hidpp_wheel_mode mode;
    int ratchet;

    if (!info)
        return cli_unsupported(ctl, "a high resolution wheel");

    pf_cli_printf(cli, "Version: %u\n", info->version);
    pf_cli_printf(cli, "Counts per ratchet (high res): %u\n", info->multiplier);
    pf_cli_printf(
        cli, "Ratchets per rotation: %u\n", info->ratchetsPerRotation
    );
    pf_cli_printf(cli, "Diameter: %u mm\n", info->diameter);
    pf_cli_printf(
        cli,
        "Can: invert %s, read ratchet %s, collect analytics %s\n",
        cli_yesno(info->hasInvert),
        cli_yesno(info->hasSwitch),
        cli_yesno(info->hasAnalytics)
    );

    if (!hidpp_wheel_get_mode(ctl->device, &mode)) {
        pf_cli_printf(cli, "Diverted: %s\n", cli_onoff(mode.diverted));
        pf_cli_printf(cli, "High resolution: %s\n", cli_onoff(mode.highRes));
        pf_cli_printf(cli, "Inverted: %s\n", cli_onoff(mode.inverted));
        pf_cli_printf(cli, "Analytics: %s\n", cli_onoff(mode.analytics));
    }

    if (info->hasSwitch && (ratchet = hidpp_wheel_ratchet(ctl->device)) >= 0)
        pf_cli_printf(cli, "Ratchet: %s\n", ratchet ? "engaged" : "free wheel");

    return HIDPP_OK;
}

static int wheel_set(hidppctl_t *ctl) {
    char **argv = ctl->options->paramv;
    struct hidpp_wheel_mode mode;
    int rc, v;

    if (ctl->options->paramc < 2)
        return cli_usage(
            ctl,
            "Expected 'set <option>=<on|off>...' where option is one of: "
            "diverted, highres, inverted, analytics."
        );

    if ((rc = hidpp_wheel_get_mode(ctl->device, &mode)))
        return cli_fail(ctl, rc, "read the wheel mode");

    for (int i = 1; i < ctl->options->paramc; i++) {
        char *val, *key = cli_split(argv[i], &val);

        if (!val)
            return cli_usage(ctl, "Expected '%s=<on|off>'.", key);
        if (cli_bool(ctl, val, key, &v))
            return HIDPP_EINVAL;

        if (cli_is(key, "diverted"))
            mode.diverted = v;
        else if (cli_is(key, "highres"))
            mode.highRes = v;
        else if (cli_is(key, "inverted"))
            mode.inverted = v;
        else if (cli_is(key, "analytics"))
            mode.analytics = v;
        else
            return cli_usage(ctl, "Unknown wheel option '%s'.", key);
    }

    if ((rc = hidpp_wheel_set_mode(ctl->device, &mode)))
        return cli_fail(ctl, rc, "change the wheel mode");
    return HIDPP_OK;
}

int cmd_wheel(hidppctl_t *ctl) {
    char **argv = ctl->options->paramv;
    uint8_t data[64];
    int rc;

    if (ctl->options->paramc == 0)
        return wheel_show(ctl);
    if (cli_is(argv[0], "set"))
        return wheel_set(ctl);

    if (cli_is(argv[0], "analytics")) {
        if ((rc = hidpp_wheel_analytics(ctl->device, data, sizeof(data))) < 0)
            return cli_fail(ctl, rc, "read the analytics");

        cli_hex(ctl, data, rc);
        pf_cli_printf(ctl->cli, "\n");
        return HIDPP_OK;
    }

    return cli_usage(ctl, "Expected no arguments, 'set ...' or 'analytics'.");
}
