/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

static const struct name_value acceleration[] = {
    { "none", HIDPP_POINTER_ACCEL_NONE },
    { "low", HIDPP_POINTER_ACCEL_LOW },
    { "medium", HIDPP_POINTER_ACCEL_MEDIUM },
    { "high", HIDPP_POINTER_ACCEL_HIGH },
    { NULL, 0 },
};

int cmd_pointer(hidppctl_t *ctl) {
    pf_cli_t *cli = ctl->cli;
    struct hidpp_pointer_info *info = hidpp_pointer_info(ctl->device);

    if (!info)
        return cli_unsupported(ctl, "mouse pointer information");

    pf_cli_printf(cli, "Resolution: %u DPI\n", info->resolution);
    pf_cli_printf(
        cli,
        "Suggested acceleration: %s\n",
        cli_str(cli_name(acceleration, info->acceleration))
    );
    pf_cli_printf(
        cli,
        "Suggests OS ballistics: %s\n",
        cli_yesno(info->suggestOsBallistics)
    );
    pf_cli_printf(
        cli,
        "Suggests vertical tuning: %s\n",
        cli_yesno(info->suggestVerticalTuning)
    );

    return HIDPP_OK;
}
