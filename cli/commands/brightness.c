/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

int cmd_brightness(hidppctl_t *ctl) {
    pf_cli_t *cli = ctl->cli;
    char **argv = ctl->options->paramv;
    struct hidpp_brightness_info *info = hidpp_brightness_info(ctl->device);
    int rc, v;
    long n;

    if (!info)
        return cli_unsupported(ctl, "brightness control");

    if (ctl->options->paramc == 0) {
        pf_cli_printf(
            cli,
            "Range: %u to %u in %u steps\n",
            info->min,
            info->max,
            info->steps
        );
        pf_cli_printf(
            cli,
            "Has: hardware buttons %s, events %s, on/off state %s, hardware "
            "on/off %s\n",
            cli_yesno(info->hasHwBrightness),
            cli_yesno(info->hasEvents),
            cli_yesno(info->hasState),
            cli_yesno(info->hasHwOnOff)
        );
        pf_cli_printf(cli, "Persistent: %s\n", cli_yesno(!info->isTransient));

        if ((rc = hidpp_brightness_get(ctl->device)) >= 0)
            pf_cli_printf(cli, "Brightness: %d\n", rc);
        if (info->hasState && (rc = hidpp_brightness_state(ctl->device)) >= 0)
            pf_cli_printf(cli, "Lighting: %s\n", cli_onoff(rc));

        return HIDPP_OK;
    }

    if (ctl->options->paramc == 1
        && (cli_is(argv[0], "on") || cli_is(argv[0], "off"))) {
        v = cli_is(argv[0], "on");
        if ((rc = hidpp_brightness_set_state(ctl->device, v)))
            return cli_fail(ctl, rc, "switch the lighting");
        return HIDPP_OK;
    }

    if (ctl->options->paramc == 2 && cli_is(argv[0], "set")) {
        if (cli_long(ctl, argv[1], "brightness", info->min, info->max, &n))
            return HIDPP_EINVAL;
        if ((rc = hidpp_brightness_set(ctl->device, n)))
            return cli_fail(ctl, rc, "set the brightness");
        return HIDPP_OK;
    }

    return cli_usage(
        ctl, "Expected no arguments, 'on', 'off' or 'set <value>'."
    );
}
