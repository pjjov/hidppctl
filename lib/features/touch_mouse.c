/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

/** Section: HID++ feature documentation

    Touch mouse raw touch points (id: 0x6110)
    Big endian integers are exchanged.

    -----------------------
    Fn 0 - get touchpad info
    -----------------------
    request:
    response:
        uint16_t xMax (dots)
        uint16_t yMax (dots)
        uint16_t resolution (dpi, the same for X and Y)
        uint8_t origin (1 lower left, 2 lower right, 3 upper left,
                        4 upper right)
        uint8_t maxFingers
        uint8_t contactMax (range of the touch point width and height)

    -----------------------
    Fn 1 - get raw mode
    -----------------------
    request:
    response:
        uint8_t mode
        - 0 - native gestures
        - 1 - raw data, filtered
        - 2 - raw data, not filtered, and native gestures
        - 3 - raw data, not filtered, sent even when lifted
        - 4 - as 2, with Z instead of W

    -----------------------
    Fn 2 - set raw mode
    -----------------------
    request:
        uint8_t mode
    response:

    -----------------------
    Events (parsed into HIDPP_EVENT_TOUCH_MOUSE_POINTS and _STATUS)
    -----------------------
    0 - raw touch points (4 points of 4 bytes, 0xFF if lifted):
        uint8_t xHigh (bits 11-4)
        uint8_t yHigh (bits 11-4)
        uint8_t low (bits 7-4 - y bits 3-0, bits 3-0 - x bits 3-0)
        uint8_t contact (bits 7-4 - height, bits 3-0 - width)
    1 - status:
        uint8_t flags (bit 0 - button down, bit 1 - mouse lifted)
*/

#define TOUCH_MOUSE_ID 0x6110

struct hidpp_feat_touch_mouse {
    hidpp_bool_t initialized;
    hidpp_bool_t unsupported;

    uint8_t featIndex;
    struct hidpp_touch_mouse_info info;
};

static int query_info(hidpp_device_t *dev, struct hidpp_feat_touch_mouse *tm) {
    struct hidpp_touch_mouse_info *info = &tm->info;
    hidpp_packet_t req, res;
    make_packet(&req, dev, tm->featIndex, 0);

    int rc;

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return is_transient(dev, rc) ? rc : HIDPP_OK;

    info->xMax = HIDPP_WORD(res.params[0], res.params[1]);
    info->yMax = HIDPP_WORD(res.params[2], res.params[3]);
    info->dpi = HIDPP_WORD(res.params[4], res.params[5]);
    info->origin = res.params[6];
    info->maxFingers = res.params[7];
    info->contactMax = res.params[8];
    info->initialized = HIDPP_TRUE;
    return HIDPP_OK;
}

static int ensure_init(
    struct hidpp_feat_touch_mouse **out, hidpp_device_t *dev
) {
    struct hidpp_feat_touch_mouse *tm;

    if (!out || !dev)
        return HIDPP_EINVAL;

    tm = dev->features[HIDPP_FEAT_TOUCH_MOUSE];
    *out = tm;

    if (tm->initialized)
        return tm->unsupported ? HIDPP_ENOSYS : HIDPP_OK;

    memset(tm, 0, sizeof(*tm));
    int featIndex = feature_lookup(dev, TOUCH_MOUSE_ID);
    int rc;

    if (featIndex < 0)
        return featIndex;

    tm->featIndex = (uint8_t)featIndex;
    tm->unsupported = featIndex == 0;

    if (!tm->unsupported && (rc = query_info(dev, tm)))
        return rc;

    tm->initialized = HIDPP_TRUE;

    return tm->unsupported ? HIDPP_ENOSYS : HIDPP_OK;
}

static void clear_cache(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_touch_mouse *tm = feat;
    (void)dev;
    tm->initialized = HIDPP_FALSE;
}

static void collect_cache(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_touch_mouse *tm;
    (void)feat;
    ensure_init(&tm, dev);
}

static size_t save_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_touch_mouse *tm = feat;
    uint8_t *start = b;
    (void)dev;

    SAVE_BYTE(b, tm->initialized);
    SAVE_BYTE(b, tm->unsupported);

    if (!tm->initialized || tm->unsupported)
        return b - start;

    SAVE_BYTE(b, tm->featIndex);
    SAVE_BYTE(b, tm->info.initialized);
    SAVE_WORD(b, tm->info.xMax);
    SAVE_WORD(b, tm->info.yMax);
    SAVE_WORD(b, tm->info.dpi);
    SAVE_BYTE(b, tm->info.origin);
    SAVE_BYTE(b, tm->info.maxFingers);
    SAVE_BYTE(b, tm->info.contactMax);

    return b - start;
}

static size_t load_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_touch_mouse *tm = feat;
    uint8_t *start = b;
    (void)dev;

    tm->initialized = LOAD_BYTE(b);
    tm->unsupported = LOAD_BYTE(b);

    if (!tm->initialized || tm->unsupported)
        return b - start;

    tm->featIndex = LOAD_BYTE(b);
    tm->info.initialized = LOAD_BYTE(b);
    tm->info.xMax = LOAD_WORD(b);
    tm->info.yMax = LOAD_WORD(b);
    tm->info.dpi = LOAD_WORD(b);
    tm->info.origin = LOAD_BYTE(b);
    tm->info.maxFingers = LOAD_BYTE(b);
    tm->info.contactMax = LOAD_BYTE(b);

    return b - start;
}

const struct hidpp_feat_vt hidpp_feat_touch_mouse_vt = {
    .size = sizeof(struct hidpp_feat_touch_mouse),
    .alignment = _Alignof(struct hidpp_feat_touch_mouse),
    .maxCacheSize = sizeof(struct hidpp_feat_touch_mouse),
    .minCacheSize = 2 * sizeof(uint8_t),
    .collectCache = collect_cache,
    .saveCache = save_cache,
    .loadCache = load_cache,
    .clearCache = clear_cache,
};

struct hidpp_touch_mouse_info *hidpp_touch_mouse_info(hidpp_device_t *dev) {
    struct hidpp_feat_touch_mouse *tm;

    if (!dev || ensure_init(&tm, dev))
        return NULL;

    return &tm->info;
}

int hidpp_touch_mouse_get_mode(hidpp_device_t *dev) {
    struct hidpp_feat_touch_mouse *tm;
    int rc;

    if ((rc = ensure_init(&tm, dev)))
        return rc;

    hidpp_packet_t req, res;
    make_packet(&req, dev, tm->featIndex, 1);

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    return res.params[0];
}

int hidpp_touch_mouse_set_mode(hidpp_device_t *dev, uint8_t mode) {
    struct hidpp_feat_touch_mouse *tm;
    int rc;

    if ((rc = ensure_init(&tm, dev)))
        return rc;

    if (mode > HIDPP_TOUCH_MOUSE_RAW_GESTURES_Z)
        return HIDPP_EINVAL;

    hidpp_packet_t req, res;
    make_packet(&req, dev, tm->featIndex, 2);
    req.params[0] = mode;

    return hidpp_device_request(dev, &req, &res);
}
