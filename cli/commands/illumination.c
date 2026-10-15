/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

static const struct name_value controls[] = {
    { "brightness", HIDPP_ILLUMINATION_BRIGHTNESS },
    { "temperature", HIDPP_ILLUMINATION_COLOR_TEMPERATURE },
    { NULL, 0 },
};

static const char *control_unit(int control) {
    return control == HIDPP_ILLUMINATION_BRIGHTNESS ? "lm" : "K";
}

static void show_control(hidppctl_t *ctl, int control) {
    pf_cli_t *cli = ctl->cli;
    struct hidpp_illumination_info *info;
    struct hidpp_illumination_levels lv;
    const char *unit = control_unit(control);
    int value;

    if (!(info = hidpp_illumination_info(ctl->device, control)))
        return;

    pf_cli_printf(cli, "%s:\n", cli_name(controls, control));

    if ((value = hidpp_illumination_value(ctl->device, control)) >= 0)
        pf_cli_printf(cli, "  Value: %d %s\n", value, unit);

    pf_cli_printf(
        cli,
        "  Range: %u to %u %s, step %u\n",
        info->min,
        info->max,
        unit,
        info->res
    );

    pf_cli_printf(
        cli,
        "  Settable: %s, events: %s\n",
        cli_yesno(info->isSettable),
        cli_yesno(info->hasEvents)
    );

    if (info->hasLinearLevels || info->hasNonLinearLevels) {
        if (hidpp_illumination_levels(ctl->device, control, &lv))
            return;

        if (lv.linear) {
            pf_cli_printf(
                cli,
                "  Levels: %u from %u to %u, step %u\n",
                lv.count,
                lv.min,
                lv.max,
                lv.step
            );
        } else {
            pf_cli_printf(cli, "  Levels:");
            for (unsigned i = 0; i < lv.count; i++)
                pf_cli_printf(cli, " %u", lv.values[i]);
            pf_cli_printf(cli, "\n");
        }
    }
}

static int illumination_show(hidppctl_t *ctl) {
    int state = hidpp_illumination_get(ctl->device);

    if (state < 0)
        return cli_unsupported(ctl, "illumination");

    pf_cli_printf(ctl->cli, "Light: %s\n", cli_onoff(state));
    show_control(ctl, HIDPP_ILLUMINATION_BRIGHTNESS);
    show_control(ctl, HIDPP_ILLUMINATION_COLOR_TEMPERATURE);

    int max = hidpp_illumination_effective_max(ctl->device);
    if (max > 0)
        pf_cli_printf(ctl->cli, "Brightness is limited to %d lm.\n", max);

    return HIDPP_OK;
}

static int illumination_levels(hidppctl_t *ctl) {
    char **argv = ctl->options->paramv;
    int argc = ctl->options->paramc;
    struct hidpp_illumination_levels lv = { 0 };
    int control, rc;
    long n[3];

    if (argc < 3)
        return cli_usage(
            ctl,
            "Expected 'levels <control> reset', 'levels <control> list "
            "<values>...' or 'levels <control> linear <min> <max> <step>'."
        );
    if (cli_named(ctl, argv[1], "control", controls, &control))
        return HIDPP_EINVAL;

    if (cli_is(argv[2], "reset")) {
        if ((rc = hidpp_illumination_reset_levels(ctl->device, control)))
            return cli_fail(ctl, rc, "reset the levels");
        return HIDPP_OK;
    }

    if (cli_is(argv[2], "linear") && argc == 6) {
        for (int i = 0; i < 3; i++)
            if (cli_long(ctl, argv[3 + i], "level", 0, 65535, &n[i]))
                return HIDPP_EINVAL;

        if (n[2] == 0 || n[1] < n[0])
            return cli_usage(ctl, "Invalid linear range.");

        lv.linear = HIDPP_TRUE;
        lv.min = n[0];
        lv.max = n[1];
        lv.step = n[2];
        lv.count = (n[1] - n[0]) / n[2] + 1;
    } else if (cli_is(argv[2], "list") && argc > 3) {
        if (argc - 3 > HIDPP_ILLUMINATION_MAX_LEVELS)
            return cli_usage(
                ctl,
                "At most %d levels are allowed.",
                HIDPP_ILLUMINATION_MAX_LEVELS
            );

        for (int i = 3; i < argc; i++) {
            if (cli_long(ctl, argv[i], "level", 0, 65535, &n[0]))
                return HIDPP_EINVAL;
            lv.values[lv.count++] = n[0];
        }
    } else {
        return cli_usage(ctl, "Invalid levels specification.");
    }

    if ((rc = hidpp_illumination_set_levels(ctl->device, control, &lv)))
        return cli_fail(ctl, rc, "set the levels");
    return HIDPP_OK;
}

int cmd_illumination(hidppctl_t *ctl) {
    char **argv = ctl->options->paramv;
    int argc = ctl->options->paramc;
    int rc, control, state;
    long v;

    if (argc == 0 || cli_is(argv[0], "show"))
        return illumination_show(ctl);

    if (cli_is(argv[0], "on") || cli_is(argv[0], "off")) {
        state = cli_is(argv[0], "on");
        if ((rc = hidpp_illumination_set(ctl->device, state)))
            return cli_fail(ctl, rc, "switch the light");
        return HIDPP_OK;
    }

    if (cli_is(argv[0], "levels"))
        return illumination_levels(ctl);

    if (cli_named(ctl, argv[0], "control", controls, &control) || control > 1)
        return cli_usage(
            ctl,
            "Expected 'show', 'on', 'off', 'levels ...', 'brightness "
            "<lumen>' or 'temperature <kelvin>'."
        );

    if (argc != 2)
        return cli_usage(ctl, "Expected '%s <value>'.", argv[0]);
    if (cli_long(ctl, argv[1], "value", 0, 65535, &v))
        return HIDPP_EINVAL;

    if ((rc = hidpp_illumination_set_value(ctl->device, control, v)))
        return cli_fail(ctl, rc, "set the value");
    return HIDPP_OK;
}
