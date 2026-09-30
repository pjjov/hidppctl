/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

static int cmd_status_all(hidppctl_t *ctl) {
    pf_cli_t *cli = ctl->cli;

    struct hidpp_receiver_info *info, all[32];
    size_t len = hidpp_enumerate(0, 0, all, 32);

    if (len == 0) {
        pf_cli_printf(cli, "No HID++ receivers found!\n");
        return HIDPP_OK;
    }

    for (size_t i = 0; i < len; i++) {
        info = &all[i];

        pf_cli_printf(
            cli,
            "HID++ receiver '%ls' from '%ls'\n",
            info->product,
            info->manufacturer
        );
        pf_cli_printf(
            cli,
            "  ID: %.4x:%.4x (%s)\n",
            info->vendorId,
            info->productId,
            info->path
        );

        pf_cli_printf(
            cli,
            "  Interface and usage: %d %u/%u\n",
            info->interfaceNumber,
            info->usage,
            info->usagePage
        );

        hidpp_free_info(info);
    }

    return HIDPP_OK;
}

static int cmd_status_rcv(hidppctl_t *ctl) {
    pf_cli_t *cli = ctl->cli;

    hidpp_receiver_t *rcv = ctl->receiver;

    hidpp_device_t *dev;
    struct hidpp_receiver_info info;
    struct hidpp_device_info dinfo;

    if (!rcv || hidpp_receiver_info(rcv, &info))
        return HIDPP_EIO;

    pf_cli_printf(cli, "HID++ receiver '%ls'\n", info.product);
    pf_cli_printf(
        cli, "  ID: %.4x:%.4x (%s)\n", info.vendorId, info.productId, info.path
    );
    pf_cli_printf(cli, "  Serial number: %ls\n", info.serial);
    pf_cli_printf(cli, "  Manufacturer: %ls\n", info.manufacturer);
    pf_cli_printf(cli, "  Release number %u\n", info.releaseNumber);
    pf_cli_printf(
        cli, "  Usage and page: %u, %u\n", info.usage, info.usagePage
    );
    pf_cli_printf(cli, "  Interface: %d\n", info.interfaceNumber);

    for (int i = 1; i < 7; i++) {
        if (!(dev = hidpp_device_open(rcv, i))) {
            pf_cli_printf(cli, "  Device %d disconnected\n", i);
            continue;
        }

        hidpp_device_info(dev, &dinfo);
        pf_cli_printf(cli, "  Device %d connected\n", i);
        pf_cli_printf(cli, "    Version: %u.%u\n", dinfo.major, dinfo.minor);
        hidpp_device_close(dev);
    }

    hidpp_free_info(&info);
    hidpp_close(rcv);
    return HIDPP_OK;
}

static int cmd_status_dev(hidppctl_t *ctl) {
    pf_cli_t *cli = ctl->cli;

    hidpp_receiver_t *rcv = ctl->receiver;
    hidpp_device_t *dev = ctl->device;
    struct hidpp_device_info info;

    if (hidpp_device_info(dev, &info)) {
        pf_cli_errorf(
            cli, "Unable to read device information: %ls", hidpp_error(rcv)
        );
        hidpp_device_close(dev);
        hidpp_close(rcv);
        return HIDPP_EIO;
    }

    pf_cli_printf(cli, "Device %d connected '%s'\n", info.index);
    pf_cli_printf(cli, "  Version: %u.%u\n", info.major, info.minor);

    hidpp_device_close(dev);
    hidpp_close(rcv);
    return HIDPP_OK;
}

int cmd_status(hidppctl_t *ctl) {
    struct hidppctl_opt *opt = ctl->options;

    switch (opt->subject) {
        /* clang-format off */
    case HIDPPCTL_RECEIVER: return cmd_status_rcv(ctl);
    case HIDPPCTL_DEVICE:   return cmd_status_dev(ctl);
    case HIDPPCTL_ALL:
    default:                return cmd_status_all(ctl);
        /* clang-format on */
    }
}