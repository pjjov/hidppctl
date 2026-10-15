/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

static const struct name_value reporting[] = {
    { "hid", HIDPP_CROWN_HID },
    { "diverted", HIDPP_CROWN_DIVERTED },
    { NULL, 0 },
};

static const struct name_value ratchets[] = {
    { "freespin", HIDPP_WHEEL_MODE_FREESPIN },
    { "ratchet", HIDPP_WHEEL_MODE_RATCHET },
    { NULL, 0 },
};

static int crown_show(hidppctl_t *ctl) {
    pf_cli_t *cli = ctl->cli;
    struct hidpp_crown_info *info = hidpp_crown_info(ctl->device);
    struct hidpp_crown_mode mode;

    if (!info)
        return cli_unsupported(ctl, "a crown");

    pf_cli_printf(cli, "Slots per revolution: %u\n", info->slots);
    pf_cli_printf(cli, "Ratchets per revolution: %u\n", info->ratchets);
    pf_cli_printf(cli, "Mechanized: %s\n", cli_yesno(info->isMechanized));
    pf_cli_printf(
        cli,
        "Has: button %s, long press %s, proximity %s, touch %s, tap %s, double "
        "tap %s\n",
        cli_yesno(info->hasButton),
        cli_yesno(info->hasLongPress),
        cli_yesno(info->hasProximity),
        cli_yesno(info->hasTouch),
        cli_yesno(info->hasTap),
        cli_yesno(info->hasDoubleTap)
    );

    if (hidpp_crown_get(ctl->device, &mode))
        return HIDPP_OK;

    pf_cli_printf(
        cli, "Reporting: %s\n", cli_str(cli_name(reporting, mode.reporting))
    );
    pf_cli_printf(
        cli, "Ratchet: %s\n", cli_str(cli_name(ratchets, mode.ratchet))
    );

    if (info->canSetRotationTimeout)
        pf_cli_printf(cli, "Rotation timeout: %u ms\n", mode.rotationTimeout);
    if (info->canSetShortLongTimeout)
        pf_cli_printf(
            cli, "Short/long press timeout: %u ms\n", mode.shortLongTimeout
        );
    if (info->canSetDoubleTapSpeed)
        pf_cli_printf(cli, "Double tap speed: %u ms\n", mode.doubleTapSpeed);

    return HIDPP_OK;
}

static int crown_set(hidppctl_t *ctl) {
    char **argv = ctl->options->paramv;
    struct hidpp_crown_mode mode = { 0 }; /* zero fields stay unchanged */
    int rc, v;
    long n;

    if (ctl->options->paramc < 2)
        return cli_usage(
            ctl,
            "Expected 'set <option>=<value>...' where option is one of: "
            "reporting, ratchet, rotation-timeout, short-long-timeout, "
            "double-tap-speed."
        );

    for (int i = 1; i < ctl->options->paramc; i++) {
        char *val, *key = cli_split(argv[i], &val);

        if (!val)
            return cli_usage(ctl, "Expected '%s=<value>'.", key);

        if (cli_is(key, "reporting")) {
            if (cli_named(ctl, val, key, reporting, &v))
                return HIDPP_EINVAL;
            mode.reporting = v;
        } else if (cli_is(key, "ratchet")) {
            if (cli_named(ctl, val, key, ratchets, &v))
                return HIDPP_EINVAL;
            mode.ratchet = v;
        } else if (cli_is(key, "rotation-timeout")) {
            if (cli_long(ctl, val, key, 10, 640, &n))
                return HIDPP_EINVAL;
            mode.rotationTimeout = n;
        } else if (cli_is(key, "short-long-timeout")) {
            if (cli_long(ctl, val, key, 10, 65535, &n))
                return HIDPP_EINVAL;
            mode.shortLongTimeout = n;
        } else if (cli_is(key, "double-tap-speed")) {
            if (cli_long(ctl, val, key, 10, 65535, &n))
                return HIDPP_EINVAL;
            mode.doubleTapSpeed = n;
        } else {
            return cli_usage(ctl, "Unknown crown option '%s'.", key);
        }
    }

    if ((rc = hidpp_crown_set(ctl->device, &mode)))
        return cli_fail(ctl, rc, "change the crown");
    return HIDPP_OK;
}

int cmd_crown(hidppctl_t *ctl) {
    char **argv = ctl->options->paramv;

    if (ctl->options->paramc == 0)
        return crown_show(ctl);
    if (cli_is(argv[0], "set"))
        return crown_set(ctl);

    return cli_usage(
        ctl, "Expected no arguments or 'set <option>=<value>...'."
    );
}
