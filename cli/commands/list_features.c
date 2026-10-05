/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

int cmd_list_features(hidppctl_t *ctl) {
    pf_cli_t *cli = ctl->cli;

    hidpp_receiver_t *rcv = ctl->receiver;
    hidpp_device_t *dev = ctl->device;
    struct hidpp_device_info info;

    if (hidpp_device_info(dev, &info)) {
        pf_cli_errorf(
            cli, "Unable to read device information: %ls", hidpp_error(rcv)
        );
        return HIDPP_EIO;
    }

    uint16_t features[256];
    size_t featCount = hidpp_feature_list(dev, features, 256);

    pf_cli_printf(cli, "Device supports %u HID++ features:\n", featCount);

    for (int i = 0; i < featCount; i++) {
        uint16_t feat = hidpp_feature_id(dev, i);
        pf_cli_printf(cli, "  [0x%.4x] %s\n", feat, hidpp_feature_name(feat));
    }

    return HIDPP_OK;
}