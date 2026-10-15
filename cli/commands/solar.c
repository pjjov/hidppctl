/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

#include <time.h>

static const struct name_value leds[] = {
    { "off", HIDPP_SOLAR_LED_OFF },
    { "red", HIDPP_SOLAR_LED_RED },
    { "orange", HIDPP_SOLAR_LED_ORANGE },
    { "green", HIDPP_SOLAR_LED_GREEN },
    { NULL, 0 },
};

static int solar_measure(hidppctl_t *ctl) {
    char **argv = ctl->options->paramv;
    struct hidpp_event e;
    long count, period;
    int rc;

    if (ctl->options->paramc != 3)
        return cli_usage(ctl, "Expected 'measure <count> <period>'.");
    if (cli_long(ctl, argv[1], "count", 0, 255, &count)
        || cli_long(ctl, argv[2], "period", 0, 255, &period))
        return HIDPP_EINVAL;

    if ((rc = hidpp_solar_measure(ctl->device, count, period)))
        return cli_fail(ctl, rc, "request light measurements");

    time_t deadline = time(NULL) + (time_t)count * period + 5;
    cli_catch_signals();

    while (count > 0 && !cli_terminated() && time(NULL) < deadline) {
        if (hidpp_device_poll(ctl->device, &e) || e.type != HIDPP_EVENT_SOLAR)
            continue;
        if (e.as.solar.kind != HIDPP_SOLAR_EVENT_MEASURE)
            continue;

        pf_cli_printf(ctl->cli, "%u lux\n", e.as.solar.lux);
        count--;
    }

    return HIDPP_OK;
}

int cmd_solar(hidppctl_t *ctl) {
    char **argv = ctl->options->paramv;
    int led, rc;

    if (cli_is(argv[0], "measure"))
        return solar_measure(ctl);

    if (cli_is(argv[0], "led") && ctl->options->paramc == 2) {
        if (cli_named(ctl, argv[1], "LED", leds, &led))
            return HIDPP_EINVAL;
        if ((rc = hidpp_solar_led(ctl->device, led)))
            return cli_fail(ctl, rc, "light the LED");
        return HIDPP_OK;
    }

    return cli_usage(
        ctl, "Expected 'measure <count> <period>' or 'led <color>'."
    );
}
