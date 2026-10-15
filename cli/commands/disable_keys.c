/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

static const struct name_value keys[] = {
    { "caps-lock", HIDPP_DISABLE_CAPS_LOCK },
    { "num-lock", HIDPP_DISABLE_NUM_LOCK },
    { "scroll-lock", HIDPP_DISABLE_SCROLL_LOCK },
    { "insert", HIDPP_DISABLE_INSERT },
    { "windows", HIDPP_DISABLE_WINDOWS },
    { NULL, 0 },
};

#define MAX_USAGES 32

static int disable_usages(hidppctl_t *ctl, int enable) {
    char **argv = ctl->options->paramv;
    int argc = ctl->options->paramc;
    uint8_t usages[MAX_USAGES];
    int rc;

    if (argc - 2 < 1 || argc - 2 > MAX_USAGES)
        return cli_usage(ctl, "Expected 1 to %d HID usages.", MAX_USAGES);

    for (int i = 2; i < argc; i++) {
        long v;
        if (cli_long(ctl, argv[i], "HID usage", 0, 255, &v))
            return HIDPP_EINVAL;
        usages[i - 2] = v;
    }

    if (enable)
        rc = hidpp_enable_usages(ctl->device, usages, argc - 2);
    else
        rc = hidpp_disable_usages(ctl->device, usages, argc - 2);

    if (rc)
        return cli_fail(ctl, rc, "change disabled usages");
    return HIDPP_OK;
}

int cmd_disable_keys(hidppctl_t *ctl) {
    pf_cli_t *cli = ctl->cli;
    char **argv = ctl->options->paramv;
    int argc = ctl->options->paramc;
    struct hidpp_disable_keys_info *info = hidpp_disable_keys_info(ctl->device);
    char buf[128];
    int rc, mask;

    if (!info)
        return cli_unsupported(ctl, "disabling keys");

    if (argc == 0) {
        if (info->hasKeys) {
            cli_flag_names(keys, info->keys, buf, sizeof(buf));
            pf_cli_printf(cli, "Keys which can be disabled: %s\n", buf);

            if ((rc = hidpp_disable_keys_get(ctl->device)) >= 0) {
                cli_flag_names(keys, rc, buf, sizeof(buf));
                pf_cli_printf(cli, "Disabled keys: %s\n", buf);
            }
        }

        if (info->hasUsages)
            pf_cli_printf(
                cli, "Up to %u HID usages can be disabled.\n", info->maxUsages
            );

        return HIDPP_OK;
    }

    if (cli_is(argv[0], "set") && argc == 2) {
        if (cli_flags(ctl, argv[1], "key", keys, &mask))
            return HIDPP_EINVAL;
        if ((rc = hidpp_disable_keys_set(ctl->device, mask)))
            return cli_fail(ctl, rc, "disable keys");
        return HIDPP_OK;
    }

    if (cli_is(argv[0], "usages") && argc >= 2) {
        if (cli_is(argv[1], "disable"))
            return disable_usages(ctl, 0);
        if (cli_is(argv[1], "enable"))
            return disable_usages(ctl, 1);

        if (cli_is(argv[1], "clear") && argc == 2) {
            if ((rc = hidpp_enable_all_usages(ctl->device)))
                return cli_fail(ctl, rc, "enable all usages");
            return HIDPP_OK;
        }
    }

    return cli_usage(
        ctl,
        "Expected 'set <keys>' or 'usages <disable|enable|clear> [usages]...'."
    );
}
