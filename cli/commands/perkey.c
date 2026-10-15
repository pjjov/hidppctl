/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

#define MAX_ZONES 256

static const struct name_value persistence[] = {
    { "volatile", HIDPP_LED_VOLATILE },
    { "stored", HIDPP_LED_NON_VOLATILE },
    { NULL, 0 },
};

/* "5", "10-20" or "all" */
static int parse_ids(
    hidppctl_t *ctl,
    hidpp_device_t *dev,
    char **args,
    int count,
    uint8_t *ids,
    size_t *total
) {
    size_t n = 0;
    uint8_t zones[MAX_ZONES];
    int available = hidpp_perkey_zones(dev, zones, MAX_ZONES);

    for (int i = 0; i < count; i++) {
        long first, last;
        char *end;

        if (cli_is(args[i], "all")) {
            for (int z = 0; z < available; z++)
                ids[n++] = zones[z];
            continue;
        }

        first = strtol(args[i], &end, 0);
        last = first;
        if (*end == '-')
            last = strtol(end + 1, &end, 0);

        if (end == args[i] || *end || first < 1 || last > 254 || last < first)
            return cli_usage(
                ctl, "Invalid zone range '%s'; zones are 1 to 254.", args[i]
            );

        for (long z = first; z <= last; z++) {
            if (n >= MAX_ZONES)
                return cli_usage(ctl, "Too many zones.");
            ids[n++] = z;
        }
    }

    *total = n;
    return HIDPP_OK;
}

int cmd_perkey(hidppctl_t *ctl) {
    pf_cli_t *cli = ctl->cli;
    hidpp_device_t *dev = ctl->device;
    char **argv = ctl->options->paramv;
    int argc = ctl->options->paramc;
    uint8_t ids[MAX_ZONES];
    struct hidpp_color color;
    size_t count;
    int rc, persist = HIDPP_LED_VOLATILE;

    if (argc == 0 || cli_is(argv[0], "zones")) {
        if ((rc = hidpp_perkey_zones(dev, ids, MAX_ZONES)) < 0)
            return cli_fail(ctl, rc, "list the lighting zones");

        pf_cli_printf(cli, "Device has %d lighting zones:", rc);
        for (int i = 0; i < rc; i++)
            pf_cli_printf(cli, " %u", ids[i]);
        pf_cli_printf(cli, "\n");
        return HIDPP_OK;
    }

    if (cli_is(argv[0], "set") && argc >= 3) {
        if (cli_color(ctl, argv[1], &color))
            return HIDPP_EINVAL;

        /* An optional trailing "stored" asks for the colors to be saved. */
        if (cli_is(argv[argc - 1], "stored")
            || cli_is(argv[argc - 1], "volatile")) {
            cli_named(
                ctl, argv[argc - 1], "persistence", persistence, &persist
            );
            argc--;
        }

        if (argc < 3 || parse_ids(ctl, dev, argv + 2, argc - 2, ids, &count))
            return HIDPP_EINVAL;

        if ((rc = hidpp_perkey_set_color(dev, ids, count, color)))
            return cli_fail(ctl, rc, "set the colors");
        if ((rc = hidpp_perkey_frame_end(dev, persist, 0, 0)))
            return cli_fail(ctl, rc, "show the colors");
        return HIDPP_OK;
    }

    return cli_usage(
        ctl,
        "Expected 'zones' or 'set <RRGGBB> <zones|first-last|all>... "
        "[volatile|stored]'."
    );
}
