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
    hidpp_bool_t initialized;
    hidpp_bool_t unsupported;

    uint8_t featIndex;
};

static int ensure_init(struct hidpp_feat_solar **out, hidpp_device_t *dev) {
    struct hidpp_feat_solar *sol;

    if (!out || !dev)
        return HIDPP_EINVAL;

    sol = dev->features[HIDPP_FEAT_SOLAR];
    *out = sol;

    if (sol->initialized)
        return sol->unsupported ? HIDPP_ENOSYS : HIDPP_OK;

    memset(sol, 0, sizeof(*sol));
    int featIndex = feature_lookup(dev, SOLAR_ID);

    if (featIndex < 0)
        return featIndex;

    sol->featIndex = (uint8_t)featIndex;
    sol->unsupported = featIndex == 0;
    sol->initialized = HIDPP_TRUE;

    return sol->unsupported ? HIDPP_ENOSYS : HIDPP_OK;
}

static void clear_cache(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_solar *sol = feat;
    (void)dev;
    sol->initialized = HIDPP_FALSE;
}

static void collect_cache(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_solar *sol;
    (void)feat;
    ensure_init(&sol, dev);
}

static size_t save_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_solar *sol = feat;
    uint8_t *start = b;
    (void)dev;

    SAVE_BYTE(b, sol->initialized);
    SAVE_BYTE(b, sol->unsupported);

    if (!sol->initialized || sol->unsupported)
        return b - start;

    SAVE_BYTE(b, sol->featIndex);

    return b - start;
}

static size_t load_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_solar *sol = feat;
    uint8_t *start = b;
    (void)dev;

    sol->initialized = LOAD_BYTE(b);
    sol->unsupported = LOAD_BYTE(b);

    if (!sol->initialized || sol->unsupported)
        return b - start;

    sol->featIndex = LOAD_BYTE(b);

    return b - start;
}

const struct hidpp_feat_vt hidpp_feat_solar_vt = {
    .size = sizeof(struct hidpp_feat_solar),
    .alignment = _Alignof(struct hidpp_feat_solar),
    .maxCacheSize = sizeof(struct hidpp_feat_solar),
    .minCacheSize = 2 * sizeof(uint8_t),
    .collectCache = collect_cache,
    .saveCache = save_cache,
    .loadCache = load_cache,
    .clearCache = clear_cache,
};

int hidpp_solar_measure(hidpp_device_t *dev, uint8_t count, uint8_t period) {
    struct hidpp_feat_solar *sol;
    int rc;

    if ((rc = ensure_init(&sol, dev)))
        return rc;

    hidpp_packet_t req, res;
    make_packet(&req, dev, sol->featIndex, 0);
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
    make_packet(&req, dev, sol->featIndex, 1);
    req.params[0] = led;

    return hidpp_device_request(dev, &req, &res);
}
