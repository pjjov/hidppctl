/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

static const struct name_value origins[] = {
    { "lower-left", HIDPP_ORIGIN_LOWER_LEFT },
    { "lower-right", HIDPP_ORIGIN_LOWER_RIGHT },
    { "upper-left", HIDPP_ORIGIN_UPPER_LEFT },
    { "upper-right", HIDPP_ORIGIN_UPPER_RIGHT },
    { NULL, 0 },
};

static const struct name_value pad_reports[] = {
    { "raw", HIDPP_TOUCHPAD_RAW },
    { "force", HIDPP_TOUCHPAD_FORCE },
    { "enhanced", HIDPP_TOUCHPAD_ENHANCED },
    { "width-height", HIDPP_TOUCHPAD_WIDTH_HEIGHT },
    { "native-gestures", HIDPP_TOUCHPAD_NATIVE_GESTURES },
    { "major-minor", HIDPP_TOUCHPAD_MAJOR_MINOR },
    { "width-height8", HIDPP_TOUCHPAD_WIDTH_HEIGHT8 },
    { NULL, 0 },
};

static const struct name_value mouse_modes[] = {
    { "native", HIDPP_TOUCH_MOUSE_NATIVE },
    { "raw", HIDPP_TOUCH_MOUSE_RAW },
    { "raw-gestures", HIDPP_TOUCH_MOUSE_RAW_GESTURES },
    { "raw-always", HIDPP_TOUCH_MOUSE_RAW_ALWAYS },
    { "raw-gestures-z", HIDPP_TOUCH_MOUSE_RAW_GESTURES_Z },
    { NULL, 0 },
};

int cmd_touchpad(hidppctl_t *ctl) {
    pf_cli_t *cli = ctl->cli;
    char **argv = ctl->options->paramv;
    struct hidpp_touchpad_info *info = hidpp_touchpad_info(ctl->device);
    char buf[160];
    int rc, mode;

    if (!info)
        return cli_unsupported(ctl, "a raw touchpad");

    if (ctl->options->paramc == 0) {
        pf_cli_printf(
            cli,
            "Size: %u x %u (%u DPI)\n",
            info->width,
            info->height,
            info->dpi
        );
        pf_cli_printf(
            cli, "Origin: %s\n", cli_str(cli_name(origins, info->origin))
        );
        pf_cli_printf(cli, "Fingers: %u\n", info->maxFingers);
        pf_cli_printf(cli, "Pen: %s\n", cli_yesno(info->hasPen));
        pf_cli_printf(
            cli,
            "Z range: 0x%.2x, area range: %u\n",
            info->zRange,
            info->areaRange
        );
        pf_cli_printf(
            cli, "Timestamp unit: %u x 0.1 ms\n", info->timestampUnit
        );
        pf_cli_printf(cli, "Mapping version: %u\n", info->mappingVersion);

        if ((mode = hidpp_touchpad_get_mode(ctl->device)) >= 0) {
            cli_flag_names(pad_reports, mode, buf, sizeof(buf));
            pf_cli_printf(cli, "Raw reports: %s\n", buf);
        }

        return HIDPP_OK;
    }

    if (cli_is(argv[0], "mode") && ctl->options->paramc == 2) {
        if (cli_flags(ctl, argv[1], "report", pad_reports, &mode))
            return HIDPP_EINVAL;
        if ((rc = hidpp_touchpad_set_mode(ctl->device, mode)))
            return cli_fail(ctl, rc, "set the touchpad mode");
        return HIDPP_OK;
    }

    return cli_usage(ctl, "Expected no arguments or 'mode <reports>'.");
}

int cmd_touch_mouse(hidppctl_t *ctl) {
    pf_cli_t *cli = ctl->cli;
    char **argv = ctl->options->paramv;
    struct hidpp_touch_mouse_info *info = hidpp_touch_mouse_info(ctl->device);
    int rc, mode;

    if (!info)
        return cli_unsupported(ctl, "a touch mouse");

    if (ctl->options->paramc == 0) {
        pf_cli_printf(
            cli, "Size: %u x %u (%u DPI)\n", info->xMax, info->yMax, info->dpi
        );
        pf_cli_printf(
            cli, "Origin: %s\n", cli_str(cli_name(origins, info->origin))
        );
        pf_cli_printf(cli, "Fingers: %u\n", info->maxFingers);
        pf_cli_printf(cli, "Contact range: %u\n", info->contactMax);

        if ((mode = hidpp_touch_mouse_get_mode(ctl->device)) >= 0)
            pf_cli_printf(
                cli, "Mode: %s\n", cli_str(cli_name(mouse_modes, mode))
            );

        return HIDPP_OK;
    }

    if (cli_is(argv[0], "mode") && ctl->options->paramc == 2) {
        if (cli_named(ctl, argv[1], "mode", mouse_modes, &mode))
            return HIDPP_EINVAL;
        if ((rc = hidpp_touch_mouse_set_mode(ctl->device, mode)))
            return cli_fail(ctl, rc, "set the touch mouse mode");
        return HIDPP_OK;
    }

    return cli_usage(ctl, "Expected no arguments or 'mode <mode>'.");
}
