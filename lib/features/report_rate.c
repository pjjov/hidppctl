/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

/** Section: HID++ feature documentation

    Report rate (id: 0x8060)

    -----------------------
    Fn 0 - get report rate list
    -----------------------
    request:
    response:
        uint8_t rates (bit N - an interval of N + 1 ms is supported)

    -----------------------
    Fn 1 - get report rate
    -----------------------
    request:
    response:
        uint8_t interval (ms)

    -----------------------
    Fn 2 - set report rate (host mode only)
    -----------------------
    request:
        uint8_t interval (ms)
    response:

    Extended adjustable report rate (id: 0x8061)
    Big endian integers are exchanged. Connection types are 0 = wired and
    1 = gaming wireless. Rates are numbered 0 = 125 Hz (8 ms) to
    6 = 8 kHz (125 us), and bit N of a rate list stands for rate N.

    -----------------------
    Fn 0 - get device capabilities (static)
    -----------------------
    request:
        uint8_t connectionType
    response:
        uint16_t supportedRates

    -----------------------
    Fn 1 - get actual report rate list (of the current connection)
    -----------------------
    request:
    response:
        uint16_t actualRates

    -----------------------
    Fn 2 - get report rate
    -----------------------
    request:
        uint8_t connectionType
    response:
        uint8_t rate

    -----------------------
    Fn 3 - set report rate (host mode only, of the current connection)
    -----------------------
    request:
        uint8_t rate
    response:

    -----------------------
    Event 0 - report rate info
    -----------------------
        uint8_t connectionType
        uint8_t rate
*/

#define REPORT_RATE_ID 0x8060
#define EXTENDED_ID 0x8061

#define CONNECTION_COUNT 2
#define BASIC_RATE_COUNT 8    /* 1 ms to 8 ms */
#define EXTENDED_RATE_COUNT 7 /* 8 ms to 125 us */
#define SLOWEST_INTERVAL 8000 /* us */
#define MS 1000

struct hidpp_feat_report_rate {
    hidpp_bool_t initialized;
    hidpp_bool_t unsupported;

    uint8_t featIndex;
    uint8_t basicRates;                   /* 0x8060 */
    uint16_t supported[CONNECTION_COUNT]; /* 0x8061 */
    struct hidpp_report_rate_info info;
};

static int query_info(hidpp_device_t *dev, struct hidpp_feat_report_rate *rr) {
    hidpp_packet_t req, res;
    int rc;

    if (!rr->info.isExtended) {
        make_packet(&req, dev, rr->featIndex, 0);

        if (!(rc = hidpp_device_request(dev, &req, &res)))
            rr->basicRates = res.params[0];
        else if (is_transient(dev, rc))
            return rc;
    } else {
        make_packet(&req, dev, rr->featIndex, 0);

        /* unsupported connection types are refused for good */
        for (uint8_t c = 0; c < CONNECTION_COUNT; c++) {
            req.params[0] = c;

            if (!(rc = hidpp_device_request(dev, &req, &res)))
                rr->supported[c] = HIDPP_WORD(res.params[0], res.params[1]);
            else if (is_transient(dev, rc))
                return rc;
        }
    }

    rr->info.initialized = HIDPP_TRUE;
    return HIDPP_OK;
}

static int ensure_init(
    struct hidpp_feat_report_rate **out, hidpp_device_t *dev
) {
    struct hidpp_feat_report_rate *rr;

    if (!out || !dev)
        return HIDPP_EINVAL;

    rr = dev->features[HIDPP_FEAT_REPORT_RATE];
    *out = rr;

    if (rr->initialized)
        return rr->unsupported ? HIDPP_ENOSYS : HIDPP_OK;

    memset(rr, 0, sizeof(*rr));
    int featIndex = feature_lookup(dev, EXTENDED_ID);
    int rc;

    if (featIndex < 0)
        return featIndex;

    rr->info.isExtended = featIndex != 0;

    if (featIndex == 0 && (featIndex = feature_lookup(dev, REPORT_RATE_ID)) < 0)
        return featIndex;

    rr->featIndex = (uint8_t)featIndex;
    rr->unsupported = featIndex == 0;

    if (!rr->unsupported && (rc = query_info(dev, rr)))
        return rc;

    rr->initialized = HIDPP_TRUE;

    return rr->unsupported ? HIDPP_ENOSYS : HIDPP_OK;
}

static void clear_cache(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_report_rate *rr = feat;
    (void)dev;
    rr->initialized = HIDPP_FALSE;
}

static void collect_cache(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_report_rate *rr;
    (void)feat;
    ensure_init(&rr, dev);
}

static size_t save_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_report_rate *rr = feat;
    uint8_t *start = b;
    (void)dev;

    SAVE_BYTE(b, rr->initialized);
    SAVE_BYTE(b, rr->unsupported);

    if (!rr->initialized || rr->unsupported)
        return b - start;

    SAVE_BYTE(b, rr->featIndex);
    SAVE_BYTE(b, rr->info.initialized);
    SAVE_BYTE(b, rr->info.isExtended);
    SAVE_BYTE(b, rr->basicRates);

    for (int c = 0; c < CONNECTION_COUNT; c++)
        SAVE_WORD(b, rr->supported[c]);

    return b - start;
}

static size_t load_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_report_rate *rr = feat;
    uint8_t *start = b;
    (void)dev;

    rr->initialized = LOAD_BYTE(b);
    rr->unsupported = LOAD_BYTE(b);

    if (!rr->initialized || rr->unsupported)
        return b - start;

    rr->featIndex = LOAD_BYTE(b);
    rr->info.initialized = LOAD_BYTE(b);
    rr->info.isExtended = LOAD_BYTE(b);
    rr->basicRates = LOAD_BYTE(b);

    for (int c = 0; c < CONNECTION_COUNT; c++)
        rr->supported[c] = LOAD_WORD(b);

    return b - start;
}

const struct hidpp_feat_vt hidpp_feat_report_rate_vt = {
    .size = sizeof(struct hidpp_feat_report_rate),
    .alignment = _Alignof(struct hidpp_feat_report_rate),
    .maxCacheSize = sizeof(struct hidpp_feat_report_rate),
    .minCacheSize = 2 * sizeof(uint8_t),
    .collectCache = collect_cache,
    .saveCache = save_cache,
    .loadCache = load_cache,
    .clearCache = clear_cache,
};

/* Rate N of 0x8061 halves the interval N times. */
static uint16_t extended_interval(uint8_t rate) {
    return (uint16_t)(SLOWEST_INTERVAL >> rate);
}

static int extended_rate(uint16_t interval) {
    for (uint8_t rate = 0; rate < EXTENDED_RATE_COUNT; rate++)
        if (extended_interval(rate) == interval)
            return rate;

    return HIDPP_EINVAL;
}

/*  The feature does not tell how the device is connected. Devices behind
    a receiver are wireless and the ones addressed directly are taken to
    be wired. */
static uint8_t current_connection(hidpp_device_t *dev) {
    return dev->index == 0xFF ? HIDPP_CONNECTION_WIRED
                              : HIDPP_CONNECTION_WIRELESS;
}

static size_t list_extended(uint16_t mask, uint16_t *out, size_t max) {
    size_t count = 0;

    for (int rate = EXTENDED_RATE_COUNT - 1; rate >= 0; rate--)
        if ((mask & (1u << rate)) && count < max)
            out[count++] = extended_interval((uint8_t)rate);

    return count;
}

static size_t list_basic(uint8_t mask, uint16_t *out, size_t max) {
    size_t count = 0;

    for (int ms = 1; ms <= BASIC_RATE_COUNT; ms++)
        if ((mask & (1u << (ms - 1))) && count < max)
            out[count++] = (uint16_t)(ms * MS);

    return count;
}

struct hidpp_report_rate_info *hidpp_report_rate_info(hidpp_device_t *dev) {
    struct hidpp_feat_report_rate *rr;

    if (!dev || ensure_init(&rr, dev))
        return NULL;

    return &rr->info;
}

int hidpp_report_rate_list(
    hidpp_device_t *dev, uint8_t connection, uint16_t *out, size_t max
) {
    struct hidpp_feat_report_rate *rr;
    int rc;

    if (!out && max)
        return HIDPP_EINVAL;

    if ((rc = ensure_init(&rr, dev)))
        return rc;

    if (!rr->info.isExtended) {
        if (connection != HIDPP_CONNECTION_CURRENT)
            return HIDPP_EINVAL;

        return (int)list_basic(rr->basicRates, out, max);
    }

    if (connection != HIDPP_CONNECTION_CURRENT) {
        if (connection >= CONNECTION_COUNT)
            return HIDPP_EINVAL;

        return (int)list_extended(rr->supported[connection], out, max);
    }

    /* what the current connection allows can change at any time */
    hidpp_packet_t req, res;
    make_packet(&req, dev, rr->featIndex, 1);

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    uint16_t mask = HIDPP_WORD(res.params[0], res.params[1]);
    return (int)list_extended(mask, out, max);
}

int hidpp_report_rate_get(hidpp_device_t *dev, uint8_t connection) {
    struct hidpp_feat_report_rate *rr;
    hidpp_packet_t req, res;
    int rc;

    if ((rc = ensure_init(&rr, dev)))
        return rc;

    if (!rr->info.isExtended) {
        if (connection != HIDPP_CONNECTION_CURRENT)
            return HIDPP_EINVAL;

        make_packet(&req, dev, rr->featIndex, 1);

        if ((rc = hidpp_device_request(dev, &req, &res)))
            return rc;

        return res.params[0] * MS;
    }

    if (connection == HIDPP_CONNECTION_CURRENT)
        connection = current_connection(dev);

    if (connection >= CONNECTION_COUNT)
        return HIDPP_EINVAL;

    make_packet(&req, dev, rr->featIndex, 2);
    req.params[0] = connection;

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    if (res.params[0] >= EXTENDED_RATE_COUNT)
        return HIDPP_EIO;

    return extended_interval(res.params[0]);
}

int hidpp_report_rate_set(hidpp_device_t *dev, uint16_t interval) {
    struct hidpp_feat_report_rate *rr;
    hidpp_packet_t req, res;
    int rc;

    if ((rc = ensure_init(&rr, dev)))
        return rc;

    if (!rr->info.isExtended) {
        if (interval % MS || interval < MS || interval > BASIC_RATE_COUNT * MS)
            return HIDPP_EINVAL;

        uint8_t ms = (uint8_t)(interval / MS);

        if (rr->basicRates && !(rr->basicRates & (1u << (ms - 1))))
            return HIDPP_EINVAL;

        make_packet(&req, dev, rr->featIndex, 2);
        req.params[0] = ms;
        return hidpp_device_request(dev, &req, &res);
    }

    int rate = extended_rate(interval);

    if (rate < 0)
        return rate;

    /* the rate has to be one of the connection types the device has */
    uint16_t known = rr->supported[0] | rr->supported[1];

    if (known && !(known & (1u << rate)))
        return HIDPP_EINVAL;

    make_packet(&req, dev, rr->featIndex, 3);
    req.params[0] = (uint8_t)rate;
    return hidpp_device_request(dev, &req, &res);
}
