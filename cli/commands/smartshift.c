/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

static const struct name_value wheel_modes[] = {
    { "freespin", HIDPP_WHEEL_MODE_FREESPIN },
    { "ratchet", HIDPP_WHEEL_MODE_RATCHET },
    { NULL, 0 },
};

static int smartshift_show(hidppctl_t *ctl) {
    pf_cli_t *cli = ctl->cli;
    struct hidpp_smartshift_info *info = hidpp_smartshift_info(ctl->device);
    struct hidpp_smartshift cfg;

    if (!info)
        return cli_unsupported(ctl, "SmartShift");

    pf_cli_printf(cli, "Enhanced: %s\n", cli_yesno(info->isEnhanced));
    pf_cli_printf(
        cli, "Tunable torque: %s\n", cli_yesno(info->hasTunableTorque)
    );
    pf_cli_printf(cli, "Maximum force: %u gf\n", info->maxForce);
    pf_cli_printf(cli, "Default torque: %u%%\n", info->defaultTorque);
    if (info->isEnhanced)
        pf_cli_printf(
            cli, "Default auto disengage: %u\n", info->defaultAutoDisengage
        );

    if (hidpp_smartshift_get(ctl->device, &cfg))
        return HIDPP_OK;

    pf_cli_printf(
        cli, "Mode: %s\n", cli_str(cli_name(wheel_modes, cfg.wheelMode))
    );

    if (cfg.autoDisengage == HIDPP_SMARTSHIFT_ALWAYS_ENGAGED)
        pf_cli_printf(cli, "Auto disengage: never\n");
    else
        pf_cli_printf(cli, "Auto disengage: %u\n", cfg.autoDisengage);

    if (info->hasTunableTorque)
        pf_cli_printf(cli, "Torque: %u%%\n", cfg.torque);

    return HIDPP_OK;
}

static int smartshift_set(hidppctl_t *ctl) {
    char **argv = ctl->options->paramv;
    struct hidpp_smartshift cfg = { 0 }; /* zero fields stay unchanged */
    int rc, v;
    long n;

    if (ctl->options->paramc < 2)
        return cli_usage(
            ctl,
            "Expected 'set <option>=<value>...' where option is one of: "
            "mode, disengage, default-disengage, torque."
        );

    for (int i = 1; i < ctl->options->paramc; i++) {
        char *val, *key = cli_split(argv[i], &val);

        if (!val)
            return cli_usage(ctl, "Expected '%s=<value>'.", key);

        if (cli_is(key, "mode")) {
            if (cli_named(ctl, val, key, wheel_modes, &v))
                return HIDPP_EINVAL;
            cfg.wheelMode = v;
        } else if (cli_is(key, "disengage")) {
            if (cli_is(val, "never")) {
                cfg.autoDisengage = HIDPP_SMARTSHIFT_ALWAYS_ENGAGED;
            } else {
                if (cli_long(ctl, val, key, 1, 254, &n))
                    return HIDPP_EINVAL;
                cfg.autoDisengage = n;
            }
        } else if (cli_is(key, "default-disengage")) {
            if (cli_long(ctl, val, key, 1, 254, &n))
                return HIDPP_EINVAL;
            cfg.autoDisengageDefault = n;
        } else if (cli_is(key, "torque")) {
            if (cli_long(ctl, val, key, 1, 100, &n))
                return HIDPP_EINVAL;
            cfg.torque = n;
        } else {
            return cli_usage(ctl, "Unknown SmartShift option '%s'.", key);
        }
    }

    if ((rc = hidpp_smartshift_set(ctl->device, &cfg)))
        return cli_fail(ctl, rc, "change SmartShift");
    return HIDPP_OK;
}

int cmd_smartshift(hidppctl_t *ctl) {
    char **argv = ctl->options->paramv;

    if (ctl->options->paramc == 0)
        return smartshift_show(ctl);
    if (cli_is(argv[0], "set"))
        return smartshift_set(ctl);

    return cli_usage(
        ctl, "Expected no arguments or 'set <option>=<value>...'."
    );
}
