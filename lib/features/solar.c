/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

/** Section: HID++ feature documentation

    Solar keyboard dashboard (id: 0x4301)
    Big endian integers are exchanged.

    -----------------------
    Fn 0 - set light measure
    -----------------------
    request:
        uint8_t maxReports (0 cancels the events)
        uint8_t reportPeriod (seconds, 0 cancels the events)
    response:
        (echoes the request)

    -----------------------
    Fn 1 - set LED (lit for a firmware defined duration)
    -----------------------
    request:
        uint8_t led (0 = off, 1 = red, 2 = orange, 3 = green)
    response:

    -----------------------
    Events
    -----------------------
    0 - battery broadcast (every 90 s while in use)
    1 - battery and light measure (requested by Fn 0)
    2 - light check button pressed
        uint8_t batteryLevel (percent)
        uint16_t lightLevel (lux)
*/

#define SOLAR_ID 0x4301

struct hidpp_feat_solar {
    struct hidpp_feat_base base;
};

static int ensure_init(struct hidpp_feat_solar **out, hidpp_device_t *dev) {
    void *feat;
    int rc = hidpp__feature_init(dev, HIDPP_FEAT_SOLAR, &feat);
    *out = feat;
    return rc;
}

static int parse_event(
    hidpp_device_t *dev,
    void *feat,
    uint16_t id,
    uint8_t func,
    const hidpp_packet_t *pkt,
    struct hidpp_event *e
) {
    const uint8_t *p = pkt->params;
    (void)dev, (void)feat;

    if (id != SOLAR_ID || func > HIDPP_SOLAR_EVENT_BUTTON)
        return HIDPP_ENOENT;

    e->type = HIDPP_EVENT_SOLAR;
    e->as.solar.kind = func;
    e->as.solar.battery = p[0];
    e->as.solar.lux = HIDPP_WORD(p[1], p[2]);
    return HIDPP_OK;
}

const struct hidpp_feat_vt hidpp_feat_solar_vt = {
    FEAT_VT_SIZE(struct hidpp_feat_solar),
    .ids = { SOLAR_ID },
    .parseEvent = parse_event,
};

int hidpp_solar_measure(hidpp_device_t *dev, uint8_t count, uint8_t period) {
    struct hidpp_feat_solar *sol;
    int rc;

    if ((rc = ensure_init(&sol, dev)))
        return rc;

    hidpp_packet_t req, res;
    make_packet(&req, dev, sol->base.featIndex, 0);
    req.params[0] = count;
    req.params[1] = period;

    return hidpp_device_request(dev, &req, &res);
}

int hidpp_solar_led(hidpp_device_t *dev, uint8_t led) {
    struct hidpp_feat_solar *sol;
    int rc;

    if ((rc = ensure_init(&sol, dev)))
        return rc;

    if (led > HIDPP_SOLAR_LED_GREEN)
        return HIDPP_EINVAL;

    hidpp_packet_t req, res;
    make_packet(&req, dev, sol->base.featIndex, 1);
    req.params[0] = led;

    return hidpp_device_request(dev, &req, &res);
}
