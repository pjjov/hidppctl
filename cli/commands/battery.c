/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

int cmd_battery(hidppctl_t *ctl) {
    pf_cli_t *cli = ctl->cli;
    hidpp_device_t *dev = ctl->device;

    struct hidpp_battery_info *info = hidpp_battery_info(dev);
    struct hidpp_battery_status *status = hidpp_battery_status(dev);

    if (!info && !status)
        return cli_unsupported(ctl, "battery reporting");

    if (status) {
        pf_cli_printf(cli, "Status: %s\n", cli_str(status->statusName));
        pf_cli_printf(cli, "Level: %s\n", cli_str(status->levelName));
        pf_cli_printf(
            cli,
            "Discharge level: %u%% (next level at %u%%)\n",
            status->dischargeLevel,
            status->dischargeNextLevel
        );
    }

    if (info) {
        pf_cli_printf(cli, "Capabilities:\n");
        pf_cli_printf(cli, "  Levels: %u\n", info->levelCount);
        pf_cli_printf(cli, "  Critical level: %u%%\n", info->criticalLevel);
        pf_cli_printf(cli, "  Nominal life: %u\n", info->nominalLife);
        pf_cli_printf(
            cli, "  Rechargeable: %s\n", cli_yesno(info->isRechargable)
        );
        pf_cli_printf(cli, "  Mileage: %s\n", cli_yesno(info->enabledMileage));
        pf_cli_printf(
            cli, "  OSD disabled: %s\n", cli_yesno(info->disabledOSD)
        );
    }

    return HIDPP_OK;
}
