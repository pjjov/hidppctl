/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

/** Section: HID++ feature documentation

    High resolution wheel (id: 0x2121, versions 0 and 1)
    The bit positions of the capability flags are garbled in the available
    documentation; they follow the ones known to work with real devices.

    -----------------------
    Fn 0 - get wheel capability
    -----------------------
    request:
    response:
        uint8_t multiplier
        uint8_t flags
        - bit 2 - hasSwitch (ratchet switch)
        - bit 3 - hasInvert
        - bit 4 - hasAnalyticsData (version 1)
        uint8_t ratchetsPerRotation (version 1)
        uint8_t wheelDiameter (version 1, millimeters)

    -----------------------
    Fn 1 - get wheel mode
    -----------------------
    request:
    response:
        uint8_t mode
        - bit 0 - target (0 = HID, 1 = HID++ notification)
        - bit 1 - resolution (0 = low, 1 = high)
        - bit 2 - invert
        - bit 3 - analytics (version 1)

    -----------------------
    Fn 2 - set wheel mode
    -----------------------
    request:
        uint8_t mode
    response:
        uint8_t mode (echo)

    -----------------------
    Fn 3 - get ratchet switch state
    -----------------------
    request:
    response:
        uint8_t state (bit 0, 0 = free wheel, 1 = ratchet engaged)

    -----------------------
    Fn 4 - get analytics data (version 1)
    -----------------------
    request:
    response:
        uint8_t data[] (project specific, cleared once read)

    -----------------------
    Events
    -----------------------
    0 - wheel movement:
        uint8_t flags (bit 4 - resolution, bits 0-3 - periods)
        int16_t deltaV
    1 - ratchet switch:
        uint8_t state (bit 0)
*/

#define HIRES_WHEEL_ID 0x2121
#define RATCHET_ID 0x2130

/* clang-format off */

#define CAP_SWITCH_FLAG      0x04
#define CAP_INVERT_FLAG      0x08
#define CAP_ANALYTICS_FLAG   0x10

#define MODE_TARGET_FLAG     0x01
#define MODE_RESOLUTION_FLAG 0x02
#define MODE_INVERT_FLAG     0x04
#define MODE_ANALYTICS_FLAG  0x08

/* clang-format on */

struct hidpp_feat_hires_wheel {
    struct hidpp_feat_base base;
    struct hidpp_wheel_info info;
};

static void unpack_flags(struct hidpp_wheel_info *info) {
    info->hasInvert = !!(info->flags & CAP_INVERT_FLAG);
    info->hasSwitch = !!(info->flags & CAP_SWITCH_FLAG);
    info->hasAnalytics = info->version >= 1
        && !!(info->flags & CAP_ANALYTICS_FLAG);
}

static int query_info(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_hires_wheel *hw = feat;
    hw->info.version = hw->base.version;
    struct hidpp_wheel_info *info = &hw->info;
    hidpp_packet_t req, res;
    make_packet(&req, dev, hw->base.featIndex, 0);

    int rc;

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return is_transient(dev, rc) ? rc : HIDPP_OK;

    info->multiplier = res.params[0];
    info->flags = res.params[1];

    if (info->version >= 1) {
        info->ratchetsPerRotation = res.params[2];
        info->diameter = res.params[3];
    }

    unpack_flags(info);
    info->initialized = HIDPP_TRUE;
    return HIDPP_OK;
}

static int ensure_init(
    struct hidpp_feat_hires_wheel **out, hidpp_device_t *dev
) {
    void *feat;
    int rc = hidpp__feature_init(dev, HIDPP_FEAT_HIRES_WHEEL, &feat);
    *out = feat;
    return rc;
}

static size_t save_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_hires_wheel *hw = feat;
    uint8_t *start = b;
    (void)dev;

    SAVE_BYTE(b, hw->info.initialized);
    SAVE_BYTE(b, hw->info.multiplier);
    SAVE_BYTE(b, hw->info.flags);
    SAVE_BYTE(b, hw->info.ratchetsPerRotation);
    SAVE_BYTE(b, hw->info.diameter);

    return b - start;
}

static size_t load_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_hires_wheel *hw = feat;
    uint8_t *start = b;
    (void)dev;

    hw->info.initialized = LOAD_BYTE(b);
    hw->info.version = hw->base.version;
    hw->info.multiplier = LOAD_BYTE(b);
    hw->info.flags = LOAD_BYTE(b);
    hw->info.ratchetsPerRotation = LOAD_BYTE(b);
    hw->info.diameter = LOAD_BYTE(b);
    unpack_flags(&hw->info);

    return b - start;
}

/*  The ratchet notifications of the older ratchet wheel feature (0x2130)
    are decoded here too, it has no object of its own. */
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

    if (id == RATCHET_ID && func == 0) {
        e->type = HIDPP_EVENT_RATCHET;
        e->as.ratchet.deltaV = (int8_t)p[0];
        e->as.ratchet.deltaH = (int8_t)p[1];
        return HIDPP_OK;
    }

    if (id != HIRES_WHEEL_ID)
        return HIDPP_ENOENT;

    if (func == 0) {
        e->type = HIDPP_EVENT_WHEEL;
        e->as.wheel.flags = p[0];
        e->as.wheel.delta = (int16_t)HIDPP_WORD(p[1], p[2]);
        e->as.wheel.highRes = !!(p[0] & 0x10);
        e->as.wheel.periods = HIDPP_LSN(p[0]);
        return HIDPP_OK;
    } else if (func == 1) {
        e->type = HIDPP_EVENT_RATCHET_SWITCH;
        e->as.ratchetSwitch = p[0];
        return HIDPP_OK;
    }

    return HIDPP_ENOENT;
}

const struct hidpp_feat_vt hidpp_feat_hires_wheel_vt = {
    FEAT_VT_SIZE(struct hidpp_feat_hires_wheel),
    .ids = { HIRES_WHEEL_ID },
    .query = query_info,
    .saveCache = save_cache,
    .loadCache = load_cache,
    .parseEvent = parse_event,
};

struct hidpp_wheel_info *hidpp_wheel_info(hidpp_device_t *dev) {
    struct hidpp_feat_hires_wheel *hw;

    if (!dev || ensure_init(&hw, dev))
        return NULL;

    return &hw->info;
}

int hidpp_wheel_get_mode(hidpp_device_t *dev, struct hidpp_wheel_mode *out) {
    struct hidpp_feat_hires_wheel *hw;
    int rc;

    if (!out)
        return HIDPP_EINVAL;

    if ((rc = ensure_init(&hw, dev)))
        return rc;

    hidpp_packet_t req, res;
    make_packet(&req, dev, hw->base.featIndex, 1);

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    uint8_t mode = res.params[0];
    out->diverted = !!(mode & MODE_TARGET_FLAG);
    out->highRes = !!(mode & MODE_RESOLUTION_FLAG);
    out->inverted = !!(mode & MODE_INVERT_FLAG);
    out->analytics = hw->info.version >= 1 && !!(mode & MODE_ANALYTICS_FLAG);
    return HIDPP_OK;
}

int hidpp_wheel_set_mode(
    hidpp_device_t *dev, const struct hidpp_wheel_mode *mode
) {
    struct hidpp_feat_hires_wheel *hw;
    int rc;

    if (!mode)
        return HIDPP_EINVAL;

    if ((rc = ensure_init(&hw, dev)))
        return rc;

    if ((mode->inverted && !hw->info.hasInvert)
        || (mode->analytics && !hw->info.hasAnalytics))
        return HIDPP_EINVAL;

    hidpp_packet_t req, res;
    make_packet(&req, dev, hw->base.featIndex, 2);
    req.params[0] |= mode->diverted ? MODE_TARGET_FLAG : 0;
    req.params[0] |= mode->highRes ? MODE_RESOLUTION_FLAG : 0;
    req.params[0] |= mode->inverted ? MODE_INVERT_FLAG : 0;
    req.params[0] |= mode->analytics ? MODE_ANALYTICS_FLAG : 0;

    return hidpp_device_request(dev, &req, &res);
}

int hidpp_wheel_ratchet(hidpp_device_t *dev) {
    struct hidpp_feat_hires_wheel *hw;
    int rc;

    if ((rc = ensure_init(&hw, dev)))
        return rc;

    if (!hw->info.hasSwitch)
        return HIDPP_ENOSYS;

    hidpp_packet_t req, res;
    make_packet(&req, dev, hw->base.featIndex, 3);

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    return res.params[0] & 1;
}

int hidpp_wheel_analytics(hidpp_device_t *dev, uint8_t *out, size_t size) {
    struct hidpp_feat_hires_wheel *hw;
    int rc;

    if (!out)
        return HIDPP_EINVAL;

    if ((rc = ensure_init(&hw, dev)))
        return rc;

    if (!hw->info.hasAnalytics)
        return HIDPP_ENOSYS;

    hidpp_packet_t req, res;
    make_packet(&req, dev, hw->base.featIndex, 4);

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    size_t length = packet_length(res.kind);
    length = length > 4 ? length - 4 : 0;

    size_t count = size < length ? size : length;
    memcpy(out, res.params, count);
    return (int)count;
}
