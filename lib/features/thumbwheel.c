/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

/** Section: HID++ feature documentation

    Thumbwheel (id: 0x2150)
    Big endian integers are exchanged. The documentation lists the fields
    without their layout; bits are assigned in the order they are listed.

    -----------------------
    Fn 0 - get thumbwheel info
    -----------------------
    request:
    response:
        uint16_t nativeRes (ratchets per revolution in HID mode)
        uint16_t divertedRes (increments per revolution in HID++ mode)
        uint8_t defaultDirection (bit 0, 1 = positive to the right/front)
        uint8_t capabilities
        - bit 0 - time stamp
        - bit 1 - touch
        - bit 2 - proxy
        - bit 3 - single tap
        uint16_t timeUnit

    -----------------------
    Fn 1 - get thumbwheel status
    -----------------------
    request:
    response:
        uint8_t reportingMode (0 = native HID, 1 = diverted HID++)
        uint8_t flags
        - bit 0 - inverted direction
        - bit 1 - touch
        - bit 2 - proxy

    -----------------------
    Fn 2 - set thumbwheel reporting
    -----------------------
    request:
        uint8_t reportingMode
        uint8_t invertedDirection
    response:

    -----------------------
    Event 0 - thumbwheel event (in diverted mode)
    -----------------------
        int16_t rotation
        uint16_t timeStamp
        uint8_t rotationStatus (0 inactive, 1 start, 2 active, 3 stop)
        uint8_t flags
        - bit 0 - touch
        - bit 1 - proxy
        - bit 2 - single tap
*/

#define THUMBWHEEL_ID 0x2150

/* clang-format off */

#define CAP_TIMESTAMP_FLAG  0x01
#define CAP_TOUCH_FLAG      0x02
#define CAP_PROXY_FLAG      0x04
#define CAP_SINGLE_TAP_FLAG 0x08

#define STATUS_INVERT_FLAG  0x01
#define STATUS_TOUCH_FLAG   0x02
#define STATUS_PROXY_FLAG   0x04

/* clang-format on */

struct hidpp_feat_thumbwheel {
    hidpp_bool_t initialized;
    hidpp_bool_t unsupported;

    uint8_t featIndex;
    struct hidpp_thumbwheel_info info;
};

static void unpack_flags(struct hidpp_thumbwheel_info *info) {
    info->hasTimestamp = !!(info->capabilities & CAP_TIMESTAMP_FLAG);
    info->hasTouch = !!(info->capabilities & CAP_TOUCH_FLAG);
    info->hasProxy = !!(info->capabilities & CAP_PROXY_FLAG);
    info->hasSingleTap = !!(info->capabilities & CAP_SINGLE_TAP_FLAG);
}

static int query_info(hidpp_device_t *dev, struct hidpp_feat_thumbwheel *tw) {
    struct hidpp_thumbwheel_info *info = &tw->info;
    hidpp_packet_t req, res;
    make_packet(&req, dev, tw->featIndex, 0);

    int rc;

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return is_transient(dev, rc) ? rc : HIDPP_OK;

    info->nativeResolution = HIDPP_WORD(res.params[0], res.params[1]);
    info->divertedResolution = HIDPP_WORD(res.params[2], res.params[3]);
    info->defaultDirection = res.params[4] & 1;
    info->capabilities = res.params[5];
    info->timeUnit = HIDPP_WORD(res.params[6], res.params[7]);
    unpack_flags(info);
    info->initialized = HIDPP_TRUE;
    return HIDPP_OK;
}

static int ensure_init(
    struct hidpp_feat_thumbwheel **out, hidpp_device_t *dev
) {
    struct hidpp_feat_thumbwheel *tw;

    if (!out || !dev)
        return HIDPP_EINVAL;

    tw = dev->features[HIDPP_FEAT_THUMBWHEEL];
    *out = tw;

    if (tw->initialized)
        return tw->unsupported ? HIDPP_ENOSYS : HIDPP_OK;

    memset(tw, 0, sizeof(*tw));
    int featIndex = feature_lookup(dev, THUMBWHEEL_ID);
    int rc;

    if (featIndex < 0)
        return featIndex;

    tw->featIndex = (uint8_t)featIndex;
    tw->unsupported = featIndex == 0;

    if (!tw->unsupported && (rc = query_info(dev, tw)))
        return rc;

    tw->initialized = HIDPP_TRUE;

    return tw->unsupported ? HIDPP_ENOSYS : HIDPP_OK;
}

static void clear_cache(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_thumbwheel *tw = feat;
    (void)dev;
    tw->initialized = HIDPP_FALSE;
}

static void collect_cache(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_thumbwheel *tw;
    (void)feat;
    ensure_init(&tw, dev);
}

static size_t save_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_thumbwheel *tw = feat;
    uint8_t *start = b;
    (void)dev;

    SAVE_BYTE(b, tw->initialized);
    SAVE_BYTE(b, tw->unsupported);

    if (!tw->initialized || tw->unsupported)
        return b - start;

    SAVE_BYTE(b, tw->featIndex);
    SAVE_BYTE(b, tw->info.initialized);
    SAVE_WORD(b, tw->info.nativeResolution);
    SAVE_WORD(b, tw->info.divertedResolution);
    SAVE_BYTE(b, tw->info.defaultDirection);
    SAVE_BYTE(b, tw->info.capabilities);
    SAVE_WORD(b, tw->info.timeUnit);

    return b - start;
}

static size_t load_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_thumbwheel *tw = feat;
    uint8_t *start = b;
    (void)dev;

    tw->initialized = LOAD_BYTE(b);
    tw->unsupported = LOAD_BYTE(b);

    if (!tw->initialized || tw->unsupported)
        return b - start;

    tw->featIndex = LOAD_BYTE(b);
    tw->info.initialized = LOAD_BYTE(b);
    tw->info.nativeResolution = LOAD_WORD(b);
    tw->info.divertedResolution = LOAD_WORD(b);
    tw->info.defaultDirection = LOAD_BYTE(b);
    tw->info.capabilities = LOAD_BYTE(b);
    tw->info.timeUnit = LOAD_WORD(b);
    unpack_flags(&tw->info);

    return b - start;
}

const struct hidpp_feat_vt hidpp_feat_thumbwheel_vt = {
    .size = sizeof(struct hidpp_feat_thumbwheel),
    .alignment = _Alignof(struct hidpp_feat_thumbwheel),
    .maxCacheSize = sizeof(struct hidpp_feat_thumbwheel),
    .minCacheSize = 2 * sizeof(uint8_t),
    .collectCache = collect_cache,
    .saveCache = save_cache,
    .loadCache = load_cache,
    .clearCache = clear_cache,
};

struct hidpp_thumbwheel_info *hidpp_thumbwheel_info(hidpp_device_t *dev) {
    struct hidpp_feat_thumbwheel *tw;

    if (!dev || ensure_init(&tw, dev))
        return NULL;

    return &tw->info;
}

int hidpp_thumbwheel_status(
    hidpp_device_t *dev, struct hidpp_thumbwheel_status *out
) {
    struct hidpp_feat_thumbwheel *tw;
    int rc;

    if (!out)
        return HIDPP_EINVAL;

    if ((rc = ensure_init(&tw, dev)))
        return rc;

    hidpp_packet_t req, res;
    make_packet(&req, dev, tw->featIndex, 1);

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    out->diverted = res.params[0] == 1;
    out->inverted = !!(res.params[1] & STATUS_INVERT_FLAG);
    out->touch = !!(res.params[1] & STATUS_TOUCH_FLAG);
    out->proxy = !!(res.params[1] & STATUS_PROXY_FLAG);
    return HIDPP_OK;
}

int hidpp_thumbwheel_set(hidpp_device_t *dev, int diverted, int inverted) {
    struct hidpp_feat_thumbwheel *tw;
    int rc;

    if ((rc = ensure_init(&tw, dev)))
        return rc;

    hidpp_packet_t req, res;
    make_packet(&req, dev, tw->featIndex, 2);
    req.params[0] = diverted ? 1 : 0;
    req.params[1] = inverted ? 1 : 0;

    return hidpp_device_request(dev, &req, &res);
}
