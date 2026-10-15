/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

/** Section: HID++ feature documentation

    Sidetone adjustment for audio devices (id: 0x8300, versions 0 and 1)

    -----------------------
    Fn 0 - get sidetone level
    -----------------------
    request:
    response:
        uint8_t level (0-100)

    -----------------------
    Fn 1 - set sidetone level
    -----------------------
    request:
        uint8_t level (0-100)
    response:

    -----------------------
    Fn 2 - get sidetone mute (version 1)
    -----------------------
    request:
    response:
        uint8_t muteStatuses (bit N - channel N + 1 is muted)

    -----------------------
    Fn 3 - set sidetone mute (version 1)
    -----------------------
    request:
        uint8_t changeMask
        uint8_t muteStatuses
    response:

    -----------------------
    Event 0 - sidetone change (version 1, by the user)
    -----------------------
        uint8_t levelChannel (1-8, 0 if only the mute statuses changed)
        uint8_t level
        uint8_t muteStatuses
*/

#define SIDETONE_ID 0x8300

#define MAX_LEVEL 100

struct hidpp_feat_sidetone {
    hidpp_bool_t initialized;
    hidpp_bool_t unsupported;

    uint8_t featIndex;
    uint8_t version;
};

static int ensure_init(struct hidpp_feat_sidetone **out, hidpp_device_t *dev) {
    struct hidpp_feat_sidetone *st;

    if (!out || !dev)
        return HIDPP_EINVAL;

    st = dev->features[HIDPP_FEAT_SIDETONE];
    *out = st;

    if (st->initialized)
        return st->unsupported ? HIDPP_ENOSYS : HIDPP_OK;

    memset(st, 0, sizeof(*st));
    int featIndex = feature_lookup(dev, SIDETONE_ID);

    if (featIndex < 0)
        return featIndex;

    st->featIndex = (uint8_t)featIndex;
    st->unsupported = featIndex == 0;

    if (!st->unsupported)
        st->version = feature_version(dev, SIDETONE_ID);

    st->initialized = HIDPP_TRUE;

    return st->unsupported ? HIDPP_ENOSYS : HIDPP_OK;
}

static void clear_cache(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_sidetone *st = feat;
    (void)dev;
    st->initialized = HIDPP_FALSE;
}

static void collect_cache(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_sidetone *st;
    (void)feat;
    ensure_init(&st, dev);
}

static size_t save_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_sidetone *st = feat;
    uint8_t *start = b;
    (void)dev;

    SAVE_BYTE(b, st->initialized);
    SAVE_BYTE(b, st->unsupported);

    if (!st->initialized || st->unsupported)
        return b - start;

    SAVE_BYTE(b, st->featIndex);
    SAVE_BYTE(b, st->version);

    return b - start;
}

static size_t load_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_sidetone *st = feat;
    uint8_t *start = b;
    (void)dev;

    st->initialized = LOAD_BYTE(b);
    st->unsupported = LOAD_BYTE(b);

    if (!st->initialized || st->unsupported)
        return b - start;

    st->featIndex = LOAD_BYTE(b);
    st->version = LOAD_BYTE(b);

    return b - start;
}

const struct hidpp_feat_vt hidpp_feat_sidetone_vt = {
    .size = sizeof(struct hidpp_feat_sidetone),
    .alignment = _Alignof(struct hidpp_feat_sidetone),
    .maxCacheSize = sizeof(struct hidpp_feat_sidetone),
    .minCacheSize = 2 * sizeof(uint8_t),
    .collectCache = collect_cache,
    .saveCache = save_cache,
    .loadCache = load_cache,
    .clearCache = clear_cache,
};

static int ensure_mute(struct hidpp_feat_sidetone **st, hidpp_device_t *dev) {
    int rc;

    if ((rc = ensure_init(st, dev)))
        return rc;

    return (*st)->version >= 1 ? HIDPP_OK : HIDPP_ENOSYS;
}

int hidpp_sidetone_get(hidpp_device_t *dev) {
    struct hidpp_feat_sidetone *st;
    int rc;

    if ((rc = ensure_init(&st, dev)))
        return rc;

    hidpp_packet_t req, res;
    make_packet(&req, dev, st->featIndex, 0);

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    return res.params[0];
}

int hidpp_sidetone_set(hidpp_device_t *dev, uint8_t level) {
    struct hidpp_feat_sidetone *st;
    int rc;

    if ((rc = ensure_init(&st, dev)))
        return rc;

    if (level > MAX_LEVEL)
        return HIDPP_EINVAL;

    hidpp_packet_t req, res;
    make_packet(&req, dev, st->featIndex, 1);
    req.params[0] = level;

    return hidpp_device_request(dev, &req, &res);
}

int hidpp_sidetone_mute(hidpp_device_t *dev) {
    struct hidpp_feat_sidetone *st;
    int rc;

    if ((rc = ensure_mute(&st, dev)))
        return rc;

    hidpp_packet_t req, res;
    make_packet(&req, dev, st->featIndex, 2);

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    return res.params[0];
}

int hidpp_sidetone_set_mute(hidpp_device_t *dev, uint8_t mask, uint8_t mute) {
    struct hidpp_feat_sidetone *st;
    int rc;

    if ((rc = ensure_mute(&st, dev)))
        return rc;

    hidpp_packet_t req, res;
    make_packet(&req, dev, st->featIndex, 3);
    req.params[0] = mask;
    req.params[1] = mute & mask;

    return hidpp_device_request(dev, &req, &res);
}
