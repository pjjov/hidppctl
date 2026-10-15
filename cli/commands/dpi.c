/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

#include <ctype.h>

#define MAX_RANGES 16
#define MAX_LEVELS 16

static const struct name_value lods[] = {
    { "none", HIDPP_DPI_LOD_NONE },
    { "low", HIDPP_DPI_LOD_LOW },
    { "medium", HIDPP_DPI_LOD_MEDIUM },
    { "high", HIDPP_DPI_LOD_HIGH },
    { NULL, 0 },
};

static const struct name_value holds[] = {
    { "timer", HIDPP_DPI_LED_TIMER },
    { "event", HIDPP_DPI_LED_EVENT },
    { "on", HIDPP_DPI_LED_ON },
    { "off", HIDPP_DPI_LED_OFF },
    { NULL, 0 },
};

static void show_sensor(hidppctl_t *ctl, uint8_t index) {
    pf_cli_t *cli = ctl->cli;
    hidpp_device_t *dev = ctl->device;
    struct hidpp_dpi_sensor *s = hidpp_dpi_sensor(dev, index);
    struct hidpp_dpi_range ranges[MAX_RANGES];
    struct hidpp_dpi dpi;
    uint16_t levels[MAX_LEVELS];
    uint8_t lodList[MAX_LEVELS];
    int n;

    if (!s)
        return;

    pf_cli_printf(cli, "Sensor %u:\n", index);
    pf_cli_printf(
        cli,
        "  Separate Y: %s, lift off distance: %s, calibration: %s, profiles: "
        "%s\n",
        cli_yesno(s->hasY),
        cli_yesno(s->hasLod),
        cli_yesno(s->hasCalibration),
        cli_yesno(s->hasProfile)
    );

    if (!hidpp_dpi_get(dev, index, &dpi)) {
        pf_cli_printf(cli, "  Resolution: %u", dpi.x);
        if (s->hasY)
            pf_cli_printf(cli, " x %u", dpi.y);
        pf_cli_printf(cli, " DPI (default %u", dpi.defaultX);
        if (s->hasY)
            pf_cli_printf(cli, " x %u", dpi.defaultY);
        pf_cli_printf(cli, ")\n");

        if (s->hasLod)
            pf_cli_printf(
                cli,
                "  Lift off distance: %s\n",
                cli_str(cli_name(lods, dpi.lod))
            );
    }

    n = hidpp_dpi_ranges(dev, index, HIDPP_DPI_X, ranges, MAX_RANGES);
    if (n > 0) {
        pf_cli_printf(cli, "  Ranges:");
        for (int i = 0; i < n; i++) {
            if (ranges[i].step)
                pf_cli_printf(
                    cli,
                    " %u-%u/%u",
                    ranges[i].min,
                    ranges[i].max,
                    ranges[i].step
                );
            else
                pf_cli_printf(cli, " %u", ranges[i].min);
        }
        pf_cli_printf(cli, "\n");
    }

    n = hidpp_dpi_levels(dev, index, HIDPP_DPI_X, levels, MAX_LEVELS);
    if (n > 0) {
        pf_cli_printf(cli, "  Levels:");
        for (int i = 0; i < n; i++)
            pf_cli_printf(cli, " %u", levels[i]);
        pf_cli_printf(cli, "\n");
    }

    n = hidpp_dpi_lods(dev, index, lodList, MAX_LEVELS);
    if (n > 0) {
        pf_cli_printf(cli, "  Lift off distances:");
        for (int i = 0; i < n; i++)
            pf_cli_printf(cli, " %s", cli_str(cli_name(lods, lodList[i])));
        pf_cli_printf(cli, "\n");
    }
}

static int dpi_show(hidppctl_t *ctl) {
    int count = hidpp_dpi_sensor_count(ctl->device);

    if (count < 0)
        return cli_unsupported(ctl, "adjustable DPI");

    pf_cli_printf(ctl->cli, "Sensors: %d\n", count);
    for (int i = 0; i < count; i++)
        show_sensor(ctl, i);

    return HIDPP_OK;
}

static int dpi_set(hidppctl_t *ctl) {
    char **argv = ctl->options->paramv;
    int argc = ctl->options->paramc;
    long x, y = 0;
    int lod = 0, rc;

    if (argc < 2 || argc > 4)
        return cli_usage(ctl, "Expected 'set <x> [y] [lod]'.");
    if (cli_long(ctl, argv[1], "resolution", 1, HIDPP_DPI_MAX, &x))
        return HIDPP_EINVAL;

    /* The third argument is either the Y resolution or a lift off distance. */
    for (int i = 2; i < argc; i++) {
        if (isdigit((unsigned char)argv[i][0])) {
            if (cli_long(ctl, argv[i], "resolution", 1, HIDPP_DPI_MAX, &y))
                return HIDPP_EINVAL;
        } else if (cli_named(ctl, argv[i], "lift off distance", lods, &lod)) {
            return HIDPP_EINVAL;
        }
    }

    rc = hidpp_dpi_set(ctl->device, ctl->options->sensor, x, y, lod);
    if (rc)
        return cli_fail(ctl, rc, "set the resolution");
    return HIDPP_OK;
}

static int dpi_led(hidppctl_t *ctl) {
    char **argv = ctl->options->paramv;
    int argc = ctl->options->paramc;
    long level, button = 1;
    int hold = HIDPP_DPI_LED_TIMER, rc;

    if (argc < 2 || argc > 4)
        return cli_usage(ctl, "Expected 'led <level> [hold] [button]'.");
    if (cli_long(ctl, argv[1], "level", 1, 255, &level))
        return HIDPP_EINVAL;
    if (argc >= 3 && cli_named(ctl, argv[2], "hold", holds, &hold))
        return HIDPP_EINVAL;
    if (argc == 4 && cli_long(ctl, argv[3], "button", 1, 255, &button))
        return HIDPP_EINVAL;

    rc = hidpp_dpi_show(ctl->device, ctl->options->sensor, level, hold, button);
    if (rc)
        return cli_fail(ctl, rc, "show the DPI level");
    return HIDPP_OK;
}

static int dpi_calibration(hidppctl_t *ctl) {
    char **argv = ctl->options->paramv;
    int argc = ctl->options->paramc;
    struct hidpp_dpi_calibration_info info;
    long v;
    int rc;

    if (argc == 1) {
        if ((rc = hidpp_dpi_calibration_info(
                 ctl->device, ctl->options->sensor, &info
             )))
            return cli_fail(ctl, rc, "read the calibration information");

        pf_cli_printf(
            ctl->cli,
            "Mouse: %u x %u mm, calibrated at %u x %u DPI\n",
            info.mouseWidth,
            info.mouseLength,
            info.dpiX,
            info.dpiY
        );
        return HIDPP_OK;
    }

    /* calibration <x|y> <correction> */
    if (argc != 3 || !(cli_is(argv[1], "x") || cli_is(argv[1], "y")))
        return cli_usage(
            ctl, "Expected 'calibration' or 'calibration <x|y> <correction>'."
        );
    if (cli_long(ctl, argv[2], "correction", -1023, 1023, &v))
        return HIDPP_EINVAL;

    rc = hidpp_dpi_set_calibration(
        ctl->device,
        ctl->options->sensor,
        cli_is(argv[1], "x") ? HIDPP_DPI_X : HIDPP_DPI_Y,
        v
    );
    if (rc)
        return cli_fail(ctl, rc, "store the calibration");
    return HIDPP_OK;
}

int cmd_dpi(hidppctl_t *ctl) {
    char **argv = ctl->options->paramv;

    if (ctl->options->paramc == 0 || cli_is(argv[0], "show"))
        return dpi_show(ctl);
    if (cli_is(argv[0], "set"))
        return dpi_set(ctl);
    if (cli_is(argv[0], "led"))
        return dpi_led(ctl);
    if (cli_is(argv[0], "calibration"))
        return dpi_calibration(ctl);

    return cli_usage(ctl, "Expected 'show', 'set', 'led' or 'calibration'.");
}
