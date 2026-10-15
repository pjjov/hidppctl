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
    struct hidpp_feat_base base;
    struct hidpp_touch_mouse_info info;
};

static int query_info(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_touch_mouse *tm = feat;
    struct hidpp_touch_mouse_info *info = &tm->info;
    hidpp_packet_t req, res;
    make_packet(&req, dev, tm->base.featIndex, 0);

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
    void *feat;
    int rc = hidpp__feature_init(dev, HIDPP_FEAT_TOUCH_MOUSE, &feat);
    *out = feat;
    return rc;
}

static size_t save_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_touch_mouse *tm = feat;
    uint8_t *start = b;
    (void)dev;

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

    tm->info.initialized = LOAD_BYTE(b);
    tm->info.xMax = LOAD_WORD(b);
    tm->info.yMax = LOAD_WORD(b);
    tm->info.dpi = LOAD_WORD(b);
    tm->info.origin = LOAD_BYTE(b);
    tm->info.maxFingers = LOAD_BYTE(b);
    tm->info.contactMax = LOAD_BYTE(b);

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

    if (id != TOUCH_MOUSE_ID)
        return HIDPP_ENOENT;

    if (func == 0) {
        e->type = HIDPP_EVENT_TOUCH_MOUSE_POINTS;

        /*  4 bytes per point: x and y bits 11-4, their low nibbles (y in
            the high one) and the contact size (wy high) */
        for (int i = 0; i < 4; i++) {
            struct hidpp_touch_mouse_point *pt = &e->as.touchMousePoints[i];
            const uint8_t *params = &pkt->params[i * 4];

            pt->x = (uint16_t)((params[0] << 4) | HIDPP_LSN(params[2]));
            pt->y = (uint16_t)((params[1] << 4) | HIDPP_MSN(params[2]));
            pt->wx = HIDPP_LSN(params[3]);
            pt->wy = HIDPP_MSN(params[3]);
        }

        return HIDPP_OK;
    } else if (func == 1) {
        e->type = HIDPP_EVENT_TOUCH_MOUSE_STATUS;
        e->as.touchMouseStatus.flags = pkt->params[0];
        e->as.touchMouseStatus.mouseLifted = pkt->params[0] & 2;
        e->as.touchMouseStatus.buttonDown = pkt->params[0] & 1;
        return HIDPP_OK;
    }

    return HIDPP_ENOENT;
}

const struct hidpp_feat_vt hidpp_feat_touch_mouse_vt = {
    FEAT_VT_SIZE(struct hidpp_feat_touch_mouse),
    .ids = { TOUCH_MOUSE_ID },
    .query = query_info,
    .saveCache = save_cache,
    .loadCache = load_cache,
    .parseEvent = parse_event,
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
    make_packet(&req, dev, tm->base.featIndex, 1);

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
    make_packet(&req, dev, tm->base.featIndex, 2);
    req.params[0] = mode;

    return hidpp_device_request(dev, &req, &res);
}
