/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

int cmd_fn_inversion(hidppctl_t *ctl) {
    char **argv = ctl->options->paramv;
    struct hidpp_fn_inversion fn;
    int rc, v;

    if (ctl->options->paramc == 1) {
        if (cli_bool(ctl, argv[0], "Fn inversion", &v))
            return HIDPP_EINVAL;
        if ((rc = hidpp_fn_inversion_set(ctl->device, ctl->options->host, v)))
            return cli_fail(ctl, rc, "change the Fn inversion");
        return HIDPP_OK;
    }

    if ((rc = hidpp_fn_inversion_get(ctl->device, ctl->options->host, &fn)))
        return cli_fail(ctl, rc, "read the Fn inversion");

    pf_cli_printf(ctl->cli, "Host: %u\n", fn.host);
    pf_cli_printf(
        ctl->cli,
        "Function keys act as: %s\n",
        fn.enabled ? "F-keys" : "special keys"
    );
    pf_cli_printf(
        ctl->cli, "Default: %s\n", fn.defaultEnabled ? "F-keys" : "special keys"
    );
    pf_cli_printf(ctl->cli, "Fn lock: %s\n", cli_yesno(fn.hasFnLock));
    return HIDPP_OK;
}
