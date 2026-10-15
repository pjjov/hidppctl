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
    struct hidpp_feat_base base;
    uint8_t penSupport;
    struct hidpp_touchpad_info info;
};

static void unpack_flags(struct hidpp_feat_touchpad *tp) {
    tp->info.hasPen = tp->penSupport == 1;
}

static int query_info(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_touchpad *tp = feat;
    struct hidpp_touchpad_info *info = &tp->info;
    hidpp_packet_t req, res;
    make_packet(&req, dev, tp->base.featIndex, 0);

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
    void *feat;
    int rc = hidpp__feature_init(dev, HIDPP_FEAT_TOUCHPAD, &feat);
    *out = feat;
    return rc;
}

static size_t save_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_touchpad *tp = feat;
    uint8_t *start = b;
    (void)dev;

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

static int parse_event(
    hidpp_device_t *dev,
    void *feat,
    uint16_t id,
    uint8_t func,
    const hidpp_packet_t *pkt,
    struct hidpp_event *e
) {
    (void)dev, (void)feat;

    if (id != TOUCHPAD_ID || func != 0)
        return HIDPP_ENOENT;

    e->type = HIDPP_EVENT_TOUCH_PAD_POINTS;
    e->as.touchPadPoints.timestamp = HIDPP_WORD(pkt->params[0], pkt->params[1]);

    /* 7 bytes per point after the time stamp */
    for (int i = 0; i < 2; i++) {
        struct hidpp_touch_pad_point *pt = &e->as.touchPadPoints.data[i];
        const uint8_t *params = &pkt->params[i * 7 + 2];

        pt->type = (params[0] & 0xC0) >> 6;
        pt->status = (params[2] & 0xC0) >> 6;
        pt->x = (uint16_t)(((params[0] & 0x3F) << 8) | params[1]);
        pt->y = (uint16_t)(((params[2] & 0x3F) << 8) | params[3]);
        pt->force = params[4];
        pt->area = params[5];
        pt->flags = params[6];
        pt->finger = HIDPP_MSN(params[6]);
    }

    return HIDPP_OK;
}

const struct hidpp_feat_vt hidpp_feat_touchpad_vt = {
    FEAT_VT_SIZE(struct hidpp_feat_touchpad),
    .ids = { TOUCHPAD_ID },
    .query = query_info,
    .saveCache = save_cache,
    .loadCache = load_cache,
    .parseEvent = parse_event,
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
    make_packet(&req, dev, tp->base.featIndex, 1);

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
    make_packet(&req, dev, tp->base.featIndex, 2);
    req.params[0] = mode;

    return hidpp_device_request(dev, &req, &res);
}
