/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

static const struct name_value platforms[] = {
    { "apple", HIDPP_PLATFORM_APPLE },
    { "other", HIDPP_PLATFORM_OTHER },
    { NULL, 0 },
};

static const struct name_value systems[] = {
    { "tizen", HIDPP_OS_TIZEN },
    { "windows", HIDPP_OS_WINDOWS },
    { "windows-embedded", HIDPP_OS_WINDOWS_EMBEDDED },
    { "linux", HIDPP_OS_LINUX },
    { "chrome", HIDPP_OS_CHROME },
    { "android", HIDPP_OS_ANDROID },
    { "macos", HIDPP_OS_MACOS },
    { "ios", HIDPP_OS_IOS },
    { "webos", HIDPP_OS_WEBOS },
    { NULL, 0 },
};

static int platform_show(hidppctl_t *ctl) {
    pf_cli_t *cli = ctl->cli;
    hidpp_device_t *dev = ctl->device;
    struct hidpp_platform_info *info = hidpp_platform_info(dev);
    struct hidpp_platform_descriptor desc;
    struct hidpp_host_platform hp;
    char buf[128];

    if (!info)
        return cli_unsupported(ctl, "platform selection");

    pf_cli_printf(
        cli,
        "Kind: %s\n",
        info->isMultiPlatform ? "multi platform" : "dual platform"
    );
    pf_cli_printf(cli, "Platforms: %u\n", info->platformCount);
    pf_cli_printf(cli, "OS detection: %s\n", cli_yesno(info->hasOsDetection));
    pf_cli_printf(cli, "Can be set: %s\n", cli_yesno(info->canSet));

    for (unsigned i = 0; i < info->descriptorCount; i++) {
        if (hidpp_platform_descriptor(dev, i, &desc))
            continue;

        cli_flag_names(systems, desc.osMask, buf, sizeof(buf));
        pf_cli_printf(
            cli,
            "  Platform %u: %s (%u.%u to %u.%u)\n",
            desc.platform,
            buf,
            desc.fromVersion,
            desc.fromRevision,
            desc.toVersion,
            desc.toRevision
        );
    }

    if (!hidpp_platform_get(dev, ctl->options->host, &hp)) {
        pf_cli_printf(cli, "Host %u uses platform %u\n", hp.host, hp.platform);
        pf_cli_printf(
            cli,
            "  Selected by: %s\n",
            hp.source == 0 ? "default" : (hp.source == 1 ? "user" : "software")
        );
    }

    return HIDPP_OK;
}

int cmd_platform(hidppctl_t *ctl) {
    char **argv = ctl->options->paramv;
    int platform, rc;

    if (ctl->options->paramc == 0)
        return platform_show(ctl);

    if (cli_is(argv[0], "set") && ctl->options->paramc == 2) {
        if (cli_named(ctl, argv[1], "platform", platforms, &platform))
            return HIDPP_EINVAL;
        if ((rc = hidpp_platform_set(
                 ctl->device, ctl->options->host, platform
             )))
            return cli_fail(ctl, rc, "set the platform");
        return HIDPP_OK;
    }

    return cli_usage(ctl, "Expected no arguments or 'set <platform>'.");
}
