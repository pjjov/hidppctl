/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

/** Section: HID++ feature documentation

    Crown (id: 0x4600)
    Big endian integers are exchanged. Times are in steps of 10 ms.

    -----------------------
    Fn 0 - get info
    -----------------------
    request:
    response:
        uint8_t capabilities
        - bit 0 - button
        - bit 1 - button long press
        - bit 2 - mechanized ratchet, no manual control
        - bit 3 - configurable rotation timeout
        - bit 4 - configurable short-long timeout
        - bit 5 - configurable double tap speed
        uint8_t capabilities
        - bit 0 - proximity
        - bit 1 - touch
        - bit 2 - tap gesture
        - bit 3 - double tap gesture
        uint16_t slots (per revolution)
        uint16_t ratchets (per revolution)

    -----------------------
    Fn 1 - get mode
    -----------------------
    request:
    response:
        uint8_t reporting (1 = HID, 2 = diverted)
        uint8_t ratchetMode (1 = free, 2 = ratchet)
        uint8_t rotationTimeout
        uint8_t shortLongTimeout
        uint8_t doubleTapSpeed

    -----------------------
    Fn 2 - set mode (0 = no change)
    -----------------------
    request:
        uint8_t reporting
        uint8_t ratchetMode
        uint8_t rotationTimeout (clipped at 0x40)
        uint8_t shortLongTimeout
        uint8_t doubleTapSpeed
    response:
        (echoes the request)

    -----------------------
    Event 0 - crown event
    -----------------------
        uint8_t rotationState (inactive, start, active, stop)
        int8_t slotRotation
        int8_t ratchetRotation
        uint8_t proximity (inactive, start, active, stop)
        uint8_t touch (inactive, start, active, stop)
        uint8_t gesture (0 = none, 1 = tap, 2 = double tap)
        uint8_t button
        uint8_t reserved[7]
        int16_t speed (slots per second)
*/

#define CROWN_ID 0x4600

#define TIME_UNIT 10                /* ms */
#define MAX_ROTATION_TIMEOUT 0x40   /* in time units */
#define MAX_TIME (0xFF * TIME_UNIT) /* ms */

/* clang-format off */

#define CAP_BUTTON_FLAG           0x0100
#define CAP_LONG_PRESS_FLAG       0x0200
#define CAP_MECHANIZED_FLAG       0x0400
#define CAP_ROTATION_TIMEOUT_FLAG 0x0800
#define CAP_SHORT_LONG_FLAG       0x1000
#define CAP_DOUBLE_TAP_SPEED_FLAG 0x2000
#define CAP_PROXIMITY_FLAG        0x0001
#define CAP_TOUCH_FLAG            0x0002
#define CAP_TAP_FLAG              0x0004
#define CAP_DOUBLE_TAP_FLAG       0x0008

/* clang-format on */

struct hidpp_feat_crown {
    struct hidpp_feat_base base;
    struct hidpp_crown_info info;
};

static void unpack_flags(struct hidpp_crown_info *info) {
    uint16_t caps = info->capabilities;

    info->hasButton = !!(caps & CAP_BUTTON_FLAG);
    info->hasLongPress = !!(caps & CAP_LONG_PRESS_FLAG);
    info->isMechanized = !!(caps & CAP_MECHANIZED_FLAG);
    info->canSetRotationTimeout = !!(caps & CAP_ROTATION_TIMEOUT_FLAG);
    info->canSetShortLongTimeout = !!(caps & CAP_SHORT_LONG_FLAG);
    info->canSetDoubleTapSpeed = !!(caps & CAP_DOUBLE_TAP_SPEED_FLAG);
    info->hasProximity = !!(caps & CAP_PROXIMITY_FLAG);
    info->hasTouch = !!(caps & CAP_TOUCH_FLAG);
    info->hasTap = !!(caps & CAP_TAP_FLAG);
    info->hasDoubleTap = !!(caps & CAP_DOUBLE_TAP_FLAG);
}

static int query_info(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_crown *cr = feat;
    struct hidpp_crown_info *info = &cr->info;
    hidpp_packet_t req, res;
    make_packet(&req, dev, cr->base.featIndex, 0);

    int rc;

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return is_transient(dev, rc) ? rc : HIDPP_OK;

    info->capabilities = HIDPP_WORD(res.params[0], res.params[1]);
    info->slots = HIDPP_WORD(res.params[2], res.params[3]);
    info->ratchets = HIDPP_WORD(res.params[4], res.params[5]);
    unpack_flags(info);
    info->initialized = HIDPP_TRUE;
    return HIDPP_OK;
}

static int ensure_init(struct hidpp_feat_crown **out, hidpp_device_t *dev) {
    void *feat;
    int rc = hidpp__feature_init(dev, HIDPP_FEAT_CROWN, &feat);
    *out = feat;
    return rc;
}

static size_t save_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_crown *cr = feat;
    uint8_t *start = b;
    (void)dev;

    SAVE_BYTE(b, cr->info.initialized);
    SAVE_WORD(b, cr->info.capabilities);
    SAVE_WORD(b, cr->info.slots);
    SAVE_WORD(b, cr->info.ratchets);

    return b - start;
}

static size_t load_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_crown *cr = feat;
    uint8_t *start = b;
    (void)dev;

    cr->info.initialized = LOAD_BYTE(b);
    cr->info.capabilities = LOAD_WORD(b);
    cr->info.slots = LOAD_WORD(b);
    cr->info.ratchets = LOAD_WORD(b);
    unpack_flags(&cr->info);

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
    const uint8_t *p = pkt->params;
    (void)dev, (void)feat;

    if (id != CROWN_ID || func != 0)
        return HIDPP_ENOENT;

    e->type = HIDPP_EVENT_CROWN;
    e->as.crown.rotation = p[0];
    e->as.crown.slots = (int8_t)p[1];
    e->as.crown.ratchets = (int8_t)p[2];
    e->as.crown.proximity = p[3];
    e->as.crown.touch = p[4];
    e->as.crown.gesture = p[5];
    e->as.crown.button = p[6];
    e->as.crown.speed = (int16_t)HIDPP_WORD(p[14], p[15]);
    return HIDPP_OK;
}

const struct hidpp_feat_vt hidpp_feat_crown_vt = {
    FEAT_VT_SIZE(struct hidpp_feat_crown),
    .ids = { CROWN_ID },
    .query = query_info,
    .saveCache = save_cache,
    .loadCache = load_cache,
    .parseEvent = parse_event,
};

/* A zero time is left unchanged, others must be configurable. */
static int check_time(uint16_t ms, hidpp_bool_t settable, uint16_t max) {
    if (ms == 0)
        return HIDPP_OK;

    if (!settable || ms % TIME_UNIT || ms > max)
        return HIDPP_EINVAL;

    return HIDPP_OK;
}

struct hidpp_crown_info *hidpp_crown_info(hidpp_device_t *dev) {
    struct hidpp_feat_crown *cr;

    if (!dev || ensure_init(&cr, dev))
        return NULL;

    return &cr->info;
}

int hidpp_crown_get(hidpp_device_t *dev, struct hidpp_crown_mode *out) {
    struct hidpp_feat_crown *cr;
    int rc;

    if (!out)
        return HIDPP_EINVAL;

    if ((rc = ensure_init(&cr, dev)))
        return rc;

    hidpp_packet_t req, res;
    make_packet(&req, dev, cr->base.featIndex, 1);

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    out->reporting = res.params[0];
    out->ratchet = res.params[1];
    out->rotationTimeout = (uint16_t)(res.params[2] * TIME_UNIT);
    out->shortLongTimeout = (uint16_t)(res.params[3] * TIME_UNIT);
    out->doubleTapSpeed = (uint16_t)(res.params[4] * TIME_UNIT);
    return HIDPP_OK;
}

int hidpp_crown_set(hidpp_device_t *dev, const struct hidpp_crown_mode *mode) {
    struct hidpp_feat_crown *cr;
    int rc;

    if (!mode)
        return HIDPP_EINVAL;

    if ((rc = ensure_init(&cr, dev)))
        return rc;

    const struct hidpp_crown_info *info = &cr->info;

    if (mode->reporting > HIDPP_CROWN_DIVERTED
        || mode->ratchet > HIDPP_WHEEL_MODE_RATCHET)
        return HIDPP_EINVAL;

    /* only a mechanized ratchet can be switched by software */
    if (mode->ratchet && !info->isMechanized)
        return HIDPP_EINVAL;

    if ((rc = check_time(
             mode->rotationTimeout,
             info->canSetRotationTimeout,
             MAX_ROTATION_TIMEOUT * TIME_UNIT
         ))
        || (rc = check_time(
                mode->shortLongTimeout, info->canSetShortLongTimeout, MAX_TIME
            ))
        || (rc = check_time(
                mode->doubleTapSpeed, info->canSetDoubleTapSpeed, MAX_TIME
            )))
        return rc;

    hidpp_packet_t req, res;
    make_packet(&req, dev, cr->base.featIndex, 2);
    req.kind = HIDPP_KIND_LONG;
    req.params[0] = mode->reporting;
    req.params[1] = mode->ratchet;
    req.params[2] = (uint8_t)(mode->rotationTimeout / TIME_UNIT);
    req.params[3] = (uint8_t)(mode->shortLongTimeout / TIME_UNIT);
    req.params[4] = (uint8_t)(mode->doubleTapSpeed / TIME_UNIT);

    return hidpp_device_request(dev, &req, &res);
}
