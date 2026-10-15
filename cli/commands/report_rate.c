/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

#include <ctype.h>

#define MAX_RATES 16

static const struct name_value connections[] = {
    { "wired", HIDPP_CONNECTION_WIRED },
    { "wireless", HIDPP_CONNECTION_WIRELESS },
    { "current", HIDPP_CONNECTION_CURRENT },
    { NULL, 0 },
};

/* "1000hz", "1ms", "500us" or a number of microseconds. */
static int parse_interval(hidppctl_t *ctl, const char *s, long *out) {
    char *end;
    double v = strtod(s, &end);

    if (end == s || v <= 0)
        return cli_usage(ctl, "Invalid report rate '%s'.", s);

    if (cli_is(end, "hz"))
        v = 1000000.0 / v;
    else if (cli_is(end, "ms"))
        v *= 1000.0;
    else if (*end && !cli_is(end, "us"))
        return cli_usage(ctl, "Invalid report rate '%s'.", s);

    if (v < 1 || v > 65535)
        return cli_usage(ctl, "The report interval must be 1 to 65535 us.");

    *out = (long)(v + 0.5);
    return HIDPP_OK;
}

static void print_interval(hidppctl_t *ctl, unsigned us) {
    pf_cli_printf(ctl->cli, "%u us (%u Hz)", us, us ? 1000000 / us : 0);
}

int cmd_report_rate(hidppctl_t *ctl) {
    pf_cli_t *cli = ctl->cli;
    char **argv = ctl->options->paramv;
    struct hidpp_report_rate_info *info = hidpp_report_rate_info(ctl->device);
    uint16_t rates[MAX_RATES];
    long us;
    int rc, n;

    if (!info)
        return cli_unsupported(ctl, "report rate control");

    if (ctl->options->paramc == 0) {
        pf_cli_printf(cli, "Extended: %s\n", cli_yesno(info->isExtended));

        for (int c = HIDPP_CONNECTION_WIRED; c <= HIDPP_CONNECTION_WIRELESS;
             c++) {
            if (!info->isExtended && c != HIDPP_CONNECTION_WIRED)
                break;

            pf_cli_printf(cli, "%s:\n", cli_name(connections, c));
            n = hidpp_report_rate_list(ctl->device, c, rates, MAX_RATES);
            if (n > 0) {
                pf_cli_printf(cli, "  Available:");
                for (int i = 0; i < n; i++)
                    pf_cli_printf(
                        cli, " %uHz", rates[i] ? 1000000 / rates[i] : 0
                    );
                pf_cli_printf(cli, "\n");
            }
        }

        if ((rc = hidpp_report_rate_get(ctl->device, HIDPP_CONNECTION_CURRENT))
            >= 0) {
            pf_cli_printf(cli, "Current: ");
            print_interval(ctl, rc);
            pf_cli_printf(cli, "\n");
        }

        return HIDPP_OK;
    }

    if (cli_is(argv[0], "set") && ctl->options->paramc == 2) {
        if (parse_interval(ctl, argv[1], &us))
            return HIDPP_EINVAL;
        if ((rc = hidpp_report_rate_set(ctl->device, us)))
            return cli_fail(ctl, rc, "set the report rate");
        return HIDPP_OK;
    }

    return cli_usage(ctl, "Expected no arguments or 'set <rate>'.");
}
