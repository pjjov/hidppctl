/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

static const struct name_value persistence[] = {
    { "volatile", HIDPP_EQ_VOLATILE },
    { "both", HIDPP_EQ_BOTH },
    { "stored", HIDPP_EQ_NON_VOLATILE },
    { NULL, 0 },
};

static int eq_show(hidppctl_t *ctl) {
    pf_cli_t *cli = ctl->cli;
    struct hidpp_eq_info *info = hidpp_eq_info(ctl->device);
    int8_t stored[HIDPP_EQ_MAX_BANDS], active[HIDPP_EQ_MAX_BANDS];
    int ns, na = -1, nr;

    if (!info)
        return cli_unsupported(ctl, "an equalizer");

    pf_cli_printf(cli, "Bands: %u\n", info->bandCount);
    pf_cli_printf(cli, "Gain range: %d to %d dB\n", info->dbMin, info->dbMax);

    ns = hidpp_eq_get(ctl->device, 0, stored, HIDPP_EQ_MAX_BANDS);
    if (info->hasLocation)
        na = hidpp_eq_get(ctl->device, 1, active, HIDPP_EQ_MAX_BANDS);

    pf_cli_printf(cli, "Band     Hz  stored  active\n");
    for (int i = 0; i < info->bandCount && i < HIDPP_EQ_MAX_BANDS; i++) {
        pf_cli_printf(cli, "%4d %6u", i + 1, info->frequencies[i]);
        if (i < ns)
            pf_cli_printf(cli, " %6d", stored[i]);
        if (i < na)
            pf_cli_printf(cli, " %7d", active[i]);
        pf_cli_printf(cli, "\n");
    }

    if (info->hasNoiseReduction
        && (nr = hidpp_eq_noise_reduction(ctl->device)) >= 0)
        pf_cli_printf(cli, "Noise reduction: %s\n", cli_onoff(nr));

    return HIDPP_OK;
}

int cmd_eq(hidppctl_t *ctl) {
    char **argv = ctl->options->paramv;
    int argc = ctl->options->paramc;
    int rc, v;

    if (argc == 0)
        return eq_show(ctl);

    if (cli_is(argv[0], "noise-reduction") && argc == 2) {
        if (cli_bool(ctl, argv[1], "noise reduction", &v))
            return HIDPP_EINVAL;
        if ((rc = hidpp_eq_set_noise_reduction(ctl->device, v)))
            return cli_fail(ctl, rc, "change noise reduction");
        return HIDPP_OK;
    }

    if (cli_is(argv[0], "set") && argc >= 3) {
        struct hidpp_eq_info *info = hidpp_eq_info(ctl->device);
        int8_t gains[HIDPP_EQ_MAX_BANDS];
        int persist;
        long n;

        if (!info)
            return cli_unsupported(ctl, "an equalizer");
        if (cli_named(ctl, argv[1], "persistence", persistence, &persist))
            return HIDPP_EINVAL;
        if (argc - 2 != info->bandCount)
            return cli_usage(
                ctl,
                "The device has %u bands; got %d gains.",
                info->bandCount,
                argc - 2
            );

        for (int i = 2; i < argc; i++) {
            if (cli_long(ctl, argv[i], "gain", info->dbMin, info->dbMax, &n))
                return HIDPP_EINVAL;
            gains[i - 2] = n;
        }

        if ((rc = hidpp_eq_set(ctl->device, gains, info->bandCount, persist)))
            return cli_fail(ctl, rc, "set the equalizer");
        return HIDPP_OK;
    }

    return cli_usage(
        ctl,
        "Expected no arguments, 'set <volatile|both|stored> [--] <gains>...' "
        "or "
        "'noise-reduction <on|off>'."
    );
}
