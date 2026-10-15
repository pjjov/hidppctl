/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

int cmd_thumbwheel(hidppctl_t *ctl) {
    pf_cli_t *cli = ctl->cli;
    char **argv = ctl->options->paramv;
    struct hidpp_thumbwheel_info *info = hidpp_thumbwheel_info(ctl->device);
    struct hidpp_thumbwheel_status st;
    int rc, v;

    if (!info)
        return cli_unsupported(ctl, "a thumbwheel");

    if (ctl->options->paramc == 0) {
        pf_cli_printf(
            cli,
            "Native resolution: %u ratchets per revolution\n",
            info->nativeResolution
        );
        pf_cli_printf(
            cli,
            "Diverted resolution: %u per revolution\n",
            info->divertedResolution
        );
        pf_cli_printf(
            cli,
            "Positive direction: %s\n",
            info->defaultDirection ? "right or front" : "left or back"
        );
        pf_cli_printf(cli, "Time unit: %u\n", info->timeUnit);
        pf_cli_printf(
            cli,
            "Reports: timestamp %s, touch %s, proximity %s, single tap %s\n",
            cli_yesno(info->hasTimestamp),
            cli_yesno(info->hasTouch),
            cli_yesno(info->hasProxy),
            cli_yesno(info->hasSingleTap)
        );

        if (!(rc = hidpp_thumbwheel_status(ctl->device, &st))) {
            pf_cli_printf(cli, "Diverted: %s\n", cli_onoff(st.diverted));
            pf_cli_printf(cli, "Inverted: %s\n", cli_onoff(st.inverted));
            pf_cli_printf(cli, "Touched: %s\n", cli_yesno(st.touch));
            pf_cli_printf(cli, "Proximity: %s\n", cli_yesno(st.proxy));
        }

        return HIDPP_OK;
    }

    if (!cli_is(argv[0], "set") || ctl->options->paramc < 2)
        return cli_usage(
            ctl, "Expected 'set diverted=<on|off> inverted=<on|off>'."
        );

    if ((rc = hidpp_thumbwheel_status(ctl->device, &st)))
        return cli_fail(ctl, rc, "read the thumbwheel state");

    for (int i = 1; i < ctl->options->paramc; i++) {
        char *val, *key = cli_split(argv[i], &val);

        if (!val || cli_bool(ctl, val, key, &v))
            return cli_usage(ctl, "Expected '%s=<on|off>'.", key);

        if (cli_is(key, "diverted"))
            st.diverted = v;
        else if (cli_is(key, "inverted"))
            st.inverted = v;
        else
            return cli_usage(ctl, "Unknown thumbwheel option '%s'.", key);
    }

    if ((rc = hidpp_thumbwheel_set(ctl->device, st.diverted, st.inverted)))
        return cli_fail(ctl, rc, "change the thumbwheel");
    return HIDPP_OK;
}
