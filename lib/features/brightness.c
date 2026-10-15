/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

/** Section: HID++ feature documentation

    Brightness control (id: 0x8040, versions 0 and 1)
    Big endian integers are exchanged.

    -----------------------
    Fn 0 - get info
    -----------------------
    request:
    response:
        uint16_t maxBrightness
        uint8_t steps (LSB)
        uint8_t capabilities
        - bit 0 - hardware brightness control
        - bit 1 - events
        - bit 2 - illumination (Fn 3 and 4, version 1)
        - bit 3 - hardware on/off control (version 1)
        - bit 4 - transient, settings are not persisted (version 1)
        uint16_t minBrightness (version 1)
        uint8_t steps (MSB, version 1, reserved and zero before)

    -----------------------
    Fn 1 - get brightness
    -----------------------
    request:
    response:
        uint16_t brightness

    -----------------------
    Fn 2 - set brightness
    -----------------------
    request:
        uint16_t brightness
    response:

    -----------------------
    Fn 3 - get illumination (version 1)
    -----------------------
    request:
    response:
        uint8_t state (bit 0)

    -----------------------
    Fn 4 - set illumination (version 1)
    -----------------------
    request:
        uint8_t state (bit 0)
    response:

    -----------------------
    Events (user initiated changes)
    -----------------------
    0 - brightness change:     uint16_t brightness
    1 - illumination change:   uint8_t state (version 1)
*/

#define BRIGHTNESS_ID 0x8040

/* clang-format off */

#define CAP_HW_BRIGHTNESS_FLAG 0x01
#define CAP_EVENTS_FLAG        0x02
#define CAP_ILLUMINATION_FLAG  0x04
#define CAP_HW_ON_OFF_FLAG     0x08
#define CAP_TRANSIENT_FLAG     0x10

/* clang-format on */

struct hidpp_feat_brightness {
    hidpp_bool_t initialized;
    hidpp_bool_t unsupported;

    uint8_t featIndex;
    struct hidpp_brightness_info info;
};

static void unpack_flags(struct hidpp_brightness_info *info) {
    uint8_t caps = info->capabilities;

    info->hasHwBrightness = !!(caps & CAP_HW_BRIGHTNESS_FLAG);
    info->hasEvents = !!(caps & CAP_EVENTS_FLAG);
    info->hasState = info->version >= 1 && !!(caps & CAP_ILLUMINATION_FLAG);
    info->hasHwOnOff = info->version >= 1 && !!(caps & CAP_HW_ON_OFF_FLAG);
    info->isTransient = info->version >= 1 && !!(caps & CAP_TRANSIENT_FLAG);
}

static int query_info(hidpp_device_t *dev, struct hidpp_feat_brightness *br) {
    struct hidpp_brightness_info *info = &br->info;
    hidpp_packet_t req, res;
    make_packet(&req, dev, br->featIndex, 0);

    int rc;

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return is_transient(dev, rc) ? rc : HIDPP_OK;

    info->max = HIDPP_WORD(res.params[0], res.params[1]);
    info->capabilities = res.params[3];

    if (info->version >= 1) {
        info->min = HIDPP_WORD(res.params[4], res.params[5]);
        info->steps = HIDPP_WORD(res.params[6], res.params[2]);
    } else {
        info->steps = res.params[2];
    }

    unpack_flags(info);
    info->initialized = HIDPP_TRUE;
    return HIDPP_OK;
}

static int ensure_init(
    struct hidpp_feat_brightness **out, hidpp_device_t *dev
) {
    struct hidpp_feat_brightness *br;

    if (!out || !dev)
        return HIDPP_EINVAL;

    br = dev->features[HIDPP_FEAT_BRIGHTNESS];
    *out = br;

    if (br->initialized)
        return br->unsupported ? HIDPP_ENOSYS : HIDPP_OK;

    memset(br, 0, sizeof(*br));
    int featIndex = feature_lookup(dev, BRIGHTNESS_ID);
    int rc;

    if (featIndex < 0)
        return featIndex;

    br->featIndex = (uint8_t)featIndex;
    br->unsupported = featIndex == 0;

    if (!br->unsupported) {
        br->info.version = feature_version(dev, BRIGHTNESS_ID);

        if ((rc = query_info(dev, br)))
            return rc;
    }

    br->initialized = HIDPP_TRUE;

    return br->unsupported ? HIDPP_ENOSYS : HIDPP_OK;
}

static void clear_cache(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_brightness *br = feat;
    (void)dev;
    br->initialized = HIDPP_FALSE;
}

static void collect_cache(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_brightness *br;
    (void)feat;
    ensure_init(&br, dev);
}

static size_t save_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_brightness *br = feat;
    uint8_t *start = b;
    (void)dev;

    SAVE_BYTE(b, br->initialized);
    SAVE_BYTE(b, br->unsupported);

    if (!br->initialized || br->unsupported)
        return b - start;

    SAVE_BYTE(b, br->featIndex);
    SAVE_BYTE(b, br->info.initialized);
    SAVE_BYTE(b, br->info.version);
    SAVE_WORD(b, br->info.min);
    SAVE_WORD(b, br->info.max);
    SAVE_WORD(b, br->info.steps);
    SAVE_BYTE(b, br->info.capabilities);

    return b - start;
}

static size_t load_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_brightness *br = feat;
    uint8_t *start = b;
    (void)dev;

    br->initialized = LOAD_BYTE(b);
    br->unsupported = LOAD_BYTE(b);

    if (!br->initialized || br->unsupported)
        return b - start;

    br->featIndex = LOAD_BYTE(b);
    br->info.initialized = LOAD_BYTE(b);
    br->info.version = LOAD_BYTE(b);
    br->info.min = LOAD_WORD(b);
    br->info.max = LOAD_WORD(b);
    br->info.steps = LOAD_WORD(b);
    br->info.capabilities = LOAD_BYTE(b);
    unpack_flags(&br->info);

    return b - start;
}

const struct hidpp_feat_vt hidpp_feat_brightness_vt = {
    .size = sizeof(struct hidpp_feat_brightness),
    .alignment = _Alignof(struct hidpp_feat_brightness),
    .maxCacheSize = sizeof(struct hidpp_feat_brightness),
    .minCacheSize = 2 * sizeof(uint8_t),
    .collectCache = collect_cache,
    .saveCache = save_cache,
    .loadCache = load_cache,
    .clearCache = clear_cache,
};

struct hidpp_brightness_info *hidpp_brightness_info(hidpp_device_t *dev) {
    struct hidpp_feat_brightness *br;

    if (!dev || ensure_init(&br, dev))
        return NULL;

    return &br->info;
}

int hidpp_brightness_get(hidpp_device_t *dev) {
    struct hidpp_feat_brightness *br;
    int rc;

    if ((rc = ensure_init(&br, dev)))
        return rc;

    hidpp_packet_t req, res;
    make_packet(&req, dev, br->featIndex, 1);

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    return HIDPP_WORD(res.params[0], res.params[1]);
}

int hidpp_brightness_set(hidpp_device_t *dev, uint16_t value) {
    struct hidpp_feat_brightness *br;
    int rc;

    if ((rc = ensure_init(&br, dev)))
        return rc;

    const struct hidpp_brightness_info *info = &br->info;

    if (info->initialized && (value < info->min || value > info->max))
        return HIDPP_EINVAL;

    hidpp_packet_t req, res;
    make_packet(&req, dev, br->featIndex, 2);
    req.params[0] = HIDPP_MSB(value);
    req.params[1] = HIDPP_LSB(value);

    return hidpp_device_request(dev, &req, &res);
}

int hidpp_brightness_state(hidpp_device_t *dev) {
    struct hidpp_feat_brightness *br;
    int rc;

    if ((rc = ensure_init(&br, dev)))
        return rc;

    if (!br->info.hasState)
        return HIDPP_ENOSYS;

    hidpp_packet_t req, res;
    make_packet(&req, dev, br->featIndex, 3);

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    return res.params[0] & 1;
}

int hidpp_brightness_set_state(hidpp_device_t *dev, int on) {
    struct hidpp_feat_brightness *br;
    int rc;

    if ((rc = ensure_init(&br, dev)))
        return rc;

    if (!br->info.hasState)
        return HIDPP_ENOSYS;

    hidpp_packet_t req, res;
    make_packet(&req, dev, br->featIndex, 4);
    req.params[0] = on ? 1 : 0;

    return hidpp_device_request(dev, &req, &res);
}
