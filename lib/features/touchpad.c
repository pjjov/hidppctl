/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

/** Section: HID++ feature documentation

    Touchpad raw XY (id: 0x6100)
    Big endian integers are exchanged.

    -----------------------
    Fn 0 - get touchpad info
    -----------------------
    request:
    response:
        uint16_t xSize
        uint16_t ySize
        uint8_t zDataRange (0 = none, 0x0F = 16 bits)
        uint8_t areaDataRange
        uint8_t timestampUnits (0.1 ms per increment)
        uint8_t maxFingerCount
        uint8_t origin (1 lower left, 2 lower right, 3 upper left,
                        4 upper right)
        uint8_t penSupport
        uint8_t reserved[2]
        uint8_t rawReportMappingVersion
        uint16_t dpi

    -----------------------
    Fn 1 - get raw report state
    -----------------------
    request:
    response:
        uint8_t reportBitmap
        - bit 0 - raw reporting
        - bit 1 - 16-bit force (deprecated)
        - bit 2 - enhanced reporting
        - bit 3 - width/height instead of area
        - bit 4 - native gestures
        - bit 5 - major/minor/orientation
        - bit 6 - 8-bit width and height instead of area

    -----------------------
    Fn 2 - set raw report state
    -----------------------
    request:
        uint8_t reportBitmap
    response:

    -----------------------
    Event 0 - dual XY data (parsed into HIDPP_EVENT_TOUCH_PAD_POINTS)
    -----------------------
*/

#define TOUCHPAD_ID 0x6100

struct hidpp_feat_touchpad {
    hidpp_bool_t initialized;
    hidpp_bool_t unsupported;

    uint8_t featIndex;
    uint8_t penSupport;
    struct hidpp_touchpad_info info;
};

static void unpack_flags(struct hidpp_feat_touchpad *tp) {
    tp->info.hasPen = tp->penSupport == 1;
}

static int query_info(hidpp_device_t *dev, struct hidpp_feat_touchpad *tp) {
    struct hidpp_touchpad_info *info = &tp->info;
    hidpp_packet_t req, res;
    make_packet(&req, dev, tp->featIndex, 0);

    int rc;

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return is_transient(dev, rc) ? rc : HIDPP_OK;

    info->width = HIDPP_WORD(res.params[0], res.params[1]);
    info->height = HIDPP_WORD(res.params[2], res.params[3]);
    info->zRange = res.params[4];
    info->areaRange = res.params[5];
    info->timestampUnit = res.params[6];
    info->maxFingers = res.params[7];
    info->origin = res.params[8];
    tp->penSupport = res.params[9];
    info->mappingVersion = res.params[12];
    info->dpi = HIDPP_WORD(res.params[13], res.params[14]);
    unpack_flags(tp);
    info->initialized = HIDPP_TRUE;
    return HIDPP_OK;
}

static int ensure_init(struct hidpp_feat_touchpad **out, hidpp_device_t *dev) {
    struct hidpp_feat_touchpad *tp;

    if (!out || !dev)
        return HIDPP_EINVAL;

    tp = dev->features[HIDPP_FEAT_TOUCHPAD];
    *out = tp;

    if (tp->initialized)
        return tp->unsupported ? HIDPP_ENOSYS : HIDPP_OK;

    memset(tp, 0, sizeof(*tp));
    int featIndex = feature_lookup(dev, TOUCHPAD_ID);
    int rc;

    if (featIndex < 0)
        return featIndex;

    tp->featIndex = (uint8_t)featIndex;
    tp->unsupported = featIndex == 0;

    if (!tp->unsupported && (rc = query_info(dev, tp)))
        return rc;

    tp->initialized = HIDPP_TRUE;

    return tp->unsupported ? HIDPP_ENOSYS : HIDPP_OK;
}

static void clear_cache(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_touchpad *tp = feat;
    (void)dev;
    tp->initialized = HIDPP_FALSE;
}

static void collect_cache(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_touchpad *tp;
    (void)feat;
    ensure_init(&tp, dev);
}

static size_t save_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_touchpad *tp = feat;
    uint8_t *start = b;
    (void)dev;

    SAVE_BYTE(b, tp->initialized);
    SAVE_BYTE(b, tp->unsupported);

    if (!tp->initialized || tp->unsupported)
        return b - start;

    SAVE_BYTE(b, tp->featIndex);
    SAVE_BYTE(b, tp->info.initialized);
    SAVE_WORD(b, tp->info.width);
    SAVE_WORD(b, tp->info.height);
    SAVE_BYTE(b, tp->info.zRange);
    SAVE_BYTE(b, tp->info.areaRange);
    SAVE_BYTE(b, tp->info.timestampUnit);
    SAVE_BYTE(b, tp->info.maxFingers);
    SAVE_BYTE(b, tp->info.origin);
    SAVE_BYTE(b, tp->penSupport);
    SAVE_BYTE(b, tp->info.mappingVersion);
    SAVE_WORD(b, tp->info.dpi);

    return b - start;
}

static size_t load_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_touchpad *tp = feat;
    uint8_t *start = b;
    (void)dev;

    tp->initialized = LOAD_BYTE(b);
    tp->unsupported = LOAD_BYTE(b);

    if (!tp->initialized || tp->unsupported)
        return b - start;

    tp->featIndex = LOAD_BYTE(b);
    tp->info.initialized = LOAD_BYTE(b);
    tp->info.width = LOAD_WORD(b);
    tp->info.height = LOAD_WORD(b);
    tp->info.zRange = LOAD_BYTE(b);
    tp->info.areaRange = LOAD_BYTE(b);
    tp->info.timestampUnit = LOAD_BYTE(b);
    tp->info.maxFingers = LOAD_BYTE(b);
    tp->info.origin = LOAD_BYTE(b);
    tp->penSupport = LOAD_BYTE(b);
    tp->info.mappingVersion = LOAD_BYTE(b);
    tp->info.dpi = LOAD_WORD(b);
    unpack_flags(tp);

    return b - start;
}

const struct hidpp_feat_vt hidpp_feat_touchpad_vt = {
    .size = sizeof(struct hidpp_feat_touchpad),
    .alignment = _Alignof(struct hidpp_feat_touchpad),
    .maxCacheSize = sizeof(struct hidpp_feat_touchpad),
    .minCacheSize = 2 * sizeof(uint8_t),
    .collectCache = collect_cache,
    .saveCache = save_cache,
    .loadCache = load_cache,
    .clearCache = clear_cache,
};

struct hidpp_touchpad_info *hidpp_touchpad_info(hidpp_device_t *dev) {
    struct hidpp_feat_touchpad *tp;

    if (!dev || ensure_init(&tp, dev))
        return NULL;

    return &tp->info;
}

int hidpp_touchpad_get_mode(hidpp_device_t *dev) {
    struct hidpp_feat_touchpad *tp;
    int rc;

    if ((rc = ensure_init(&tp, dev)))
        return rc;

    hidpp_packet_t req, res;
    make_packet(&req, dev, tp->featIndex, 1);

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    return res.params[0];
}

int hidpp_touchpad_set_mode(hidpp_device_t *dev, uint8_t mode) {
    struct hidpp_feat_touchpad *tp;
    int rc;

    if ((rc = ensure_init(&tp, dev)))
        return rc;

    if (mode & 0x80) /* reserved */
        return HIDPP_EINVAL;

    hidpp_packet_t req, res;
    make_packet(&req, dev, tp->featIndex, 2);
    req.params[0] = mode;

    return hidpp_device_request(dev, &req, &res);
}
