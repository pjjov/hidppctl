/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

static int host_list(hidppctl_t *ctl) {
    pf_cli_t *cli = ctl->cli;
    hidpp_device_t *dev = ctl->device;
    struct hidpp_host_info *info = hidpp_host_info(dev);

    if (!info)
        return cli_unsupported(ctl, "multiple hosts");

    int current = hidpp_host_current(dev);

    pf_cli_printf(cli, "Hosts: %u\n", info->count);
    if (current >= 0)
        pf_cli_printf(cli, "Connected to host: %d\n", current);

    pf_cli_printf(cli, "Can switch: %s\n", cli_yesno(info->canSwitch));

    if (info->hasHostInfo) {
        pf_cli_printf(
            cli,
            "Can: name %s, rename %s, move %s, delete %s\n",
            cli_yesno(info->canGetName),
            cli_yesno(info->canSetName),
            cli_yesno(info->canMove),
            cli_yesno(info->canDelete)
        );
    }

    for (unsigned i = 0; i < info->count && info->hasHostInfo; i++) {
        struct hidpp_host_status st;
        char name[64] = "";

        if (hidpp_host_status(dev, i, &st))
            continue;

        if (info->canGetName && st.isPaired)
            hidpp_host_name(dev, i, name, sizeof(name));

        pf_cli_printf(
            cli,
            "  [%u]%s %s via %s%s%s%s\n",
            i,
            (int)i == current ? "*" : "",
            st.isPaired ? "paired" : "empty",
            cli_str(st.busName),
            name[0] ? " '" : "",
            name,
            name[0] ? "'" : ""
        );
    }

    return HIDPP_OK;
}

static int host_name(hidppctl_t *ctl) {
    hidpp_device_t *dev = ctl->device;
    char **argv = ctl->options->paramv;
    uint8_t host;
    int rc;

    if (ctl->options->paramc < 2 || ctl->options->paramc > 3)
        return cli_usage(ctl, "Expected 'name <host> [new-name]'.");
    if (cli_host(ctl, argv[1], &host))
        return HIDPP_EINVAL;

    if (ctl->options->paramc == 3) {
        if ((rc = hidpp_host_set_name(dev, host, argv[2])))
            return cli_fail(ctl, rc, "rename the host");
        return HIDPP_OK;
    }

    char name[64];
    if ((rc = hidpp_host_name(dev, host, name, sizeof(name))) < 0)
        return cli_fail(ctl, rc, "read the host's name");

    pf_cli_printf(ctl->cli, "%s\n", name);
    return HIDPP_OK;
}

static int host_os(hidppctl_t *ctl) {
    hidpp_device_t *dev = ctl->device;
    char **argv = ctl->options->paramv;
    int argc = ctl->options->paramc;
    struct hidpp_host_os os = { 0 };
    uint8_t host;
    long v;
    int rc;

    if (argc != 2 && argc != 6)
        return cli_usage(
            ctl, "Expected 'os <host> [<type> <version> <revision> <build>]'."
        );
    if (cli_host(ctl, argv[1], &host))
        return HIDPP_EINVAL;

    if (argc == 2) {
        if ((rc = hidpp_host_os(dev, host, &os)))
            return cli_fail(ctl, rc, "read the host's OS");

        pf_cli_printf(
            ctl->cli,
            "Type %u, version %u, revision %u, build %u\n",
            os.type,
            os.version,
            os.revision,
            os.build
        );
        return HIDPP_OK;
    }

    if (cli_long(ctl, argv[2], "type", 0, 255, &v))
        return HIDPP_EINVAL;
    os.type = v;
    if (cli_long(ctl, argv[3], "version", 0, 255, &v))
        return HIDPP_EINVAL;
    os.version = v;
    if (cli_long(ctl, argv[4], "revision", 0, 65535, &v))
        return HIDPP_EINVAL;
    os.revision = v;
    if (cli_long(ctl, argv[5], "build", 0, 65535, &v))
        return HIDPP_EINVAL;
    os.build = v;

    if ((rc = hidpp_host_set_os(dev, host, &os)))
        return cli_fail(ctl, rc, "store the host's OS");
    return HIDPP_OK;
}

static int host_cookie(hidppctl_t *ctl) {
    hidpp_device_t *dev = ctl->device;
    char **argv = ctl->options->paramv;
    int argc = ctl->options->paramc;
    uint8_t host;
    long v;
    int rc;

    if (argc < 2 || argc > 3)
        return cli_usage(ctl, "Expected 'cookie <host> [value]'.");
    if (cli_host(ctl, argv[1], &host))
        return HIDPP_EINVAL;

    if (argc == 2) {
        if ((rc = hidpp_host_cookie(dev, host)) < 0)
            return cli_fail(ctl, rc, "read the host's cookie");
        pf_cli_printf(ctl->cli, "%d\n", rc);
        return HIDPP_OK;
    }

    if (cli_long(ctl, argv[2], "cookie", 0, 255, &v))
        return HIDPP_EINVAL;
    if ((rc = hidpp_host_set_cookie(dev, host, v)))
        return cli_fail(ctl, rc, "write the host's cookie");
    return HIDPP_OK;
}

static int host_descriptor(hidppctl_t *ctl) {
    hidpp_device_t *dev = ctl->device;
    char **argv = ctl->options->paramv;
    struct hidpp_host_descriptor desc;
    struct hidpp_host_status st;
    uint8_t host;
    int rc;

    if (ctl->options->paramc != 2)
        return cli_usage(ctl, "Expected 'descriptor <host>'.");
    if (cli_host(ctl, argv[1], &host))
        return HIDPP_EINVAL;
    if ((rc = hidpp_host_status(dev, host, &st)))
        return cli_fail(ctl, rc, "read the host's status");

    for (unsigned page = 0; page < st.numPages; page++) {
        if ((rc = hidpp_host_descriptor(dev, host, page, &desc)))
            return cli_fail(ctl, rc, "read the host's descriptor");

        pf_cli_printf(ctl->cli, "Page %u (bus %u): ", page, desc.busType);
        cli_hex(ctl, desc.data, sizeof(desc.data));
        pf_cli_printf(ctl->cli, "\n");
    }

    return HIDPP_OK;
}

int cmd_host(hidppctl_t *ctl) {
    char **argv = ctl->options->paramv;
    int argc = ctl->options->paramc;
    hidpp_device_t *dev = ctl->device;
    uint8_t host, other;
    long v;
    int rc;

    if (argc == 0 || cli_is(argv[0], "list"))
        return host_list(ctl);

    if (cli_is(argv[0], "current")) {
        if ((rc = hidpp_host_current(dev)) < 0)
            return cli_fail(ctl, rc, "query the current host");
        pf_cli_printf(ctl->cli, "%d\n", rc);
        return HIDPP_OK;
    }

    if (cli_is(argv[0], "name"))
        return host_name(ctl);
    if (cli_is(argv[0], "os"))
        return host_os(ctl);
    if (cli_is(argv[0], "cookie"))
        return host_cookie(ctl);
    if (cli_is(argv[0], "descriptor"))
        return host_descriptor(ctl);

    if (cli_is(argv[0], "switch") || cli_is(argv[0], "delete")) {
        if (argc != 2)
            return cli_usage(ctl, "Expected '%s <host>'.", argv[0]);
        if (cli_host(ctl, argv[1], &host))
            return HIDPP_EINVAL;

        if (cli_is(argv[0], "switch")) {
            if ((rc = hidpp_host_switch(dev, host)))
                return cli_fail(ctl, rc, "switch the host");
        } else if ((rc = hidpp_host_delete(dev, host))) {
            return cli_fail(ctl, rc, "delete the host");
        }

        return HIDPP_OK;
    }

    if (cli_is(argv[0], "move")) {
        if (argc != 3)
            return cli_usage(ctl, "Expected 'move <host> <new-index>'.");
        if (cli_host(ctl, argv[1], &host))
            return HIDPP_EINVAL;
        if (cli_long(ctl, argv[2], "new index", 0, 254, &v))
            return HIDPP_EINVAL;

        other = (uint8_t)v;
        if ((rc = hidpp_host_move(dev, host, other)))
            return cli_fail(ctl, rc, "move the host");
        return HIDPP_OK;
    }

    return cli_usage(ctl, "Unknown host action '%s'.", argv[0]);
}
