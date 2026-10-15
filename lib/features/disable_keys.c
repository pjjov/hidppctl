/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

/** Section: HID++ feature documentation

    Disable keys (id: 0x4521)
    Every function exchanges the same bit mask of keys:
        - bit 0 - Caps Lock
        - bit 1 - Num Lock
        - bit 2 - Scroll Lock
        - bit 3 - Insert
        - bit 4 - Windows (Start)

    -----------------------
    Fn 0 - get capabilities
    -----------------------
    request:
    response:
        uint8_t disableableKeys

    -----------------------
    Fn 1 - get disabled keys
    -----------------------
    request:
    response:
        uint8_t disabledKeys

    -----------------------
    Fn 2 - set disabled keys
    -----------------------
    request:
        uint8_t keysToDisable
    response:
        uint8_t disabledKeys (echo)

    Disable keys by usage (id: 0x4522)

    -----------------------
    Fn 0 - get capabilities
    -----------------------
    request:
    response:
        uint8_t maxDisabledUsages

    -----------------------
    Fn 1 - disable keys (adds to the disabled ones)
    -----------------------
    request:
        uint8_t usages[] (keyboard page, terminated by 0 if shorter)
    response:

    -----------------------
    Fn 2 - enable keys
    -----------------------
    request:
        uint8_t usages[] (keyboard page, terminated by 0 if shorter)
    response:

    -----------------------
    Fn 3 - enable all keys
    -----------------------
    request:
    response:
*/

#define DISABLE_KEYS_ID 0x4521
#define DISABLE_USAGES_ID 0x4522

#define USAGES_PER_CALL (HIDPP_LEN_LONG - 4)

struct hidpp_feat_disable_keys {
    hidpp_bool_t initialized;
    hidpp_bool_t unsupported;

    uint8_t keysIndex;   /* 0 if 0x4521 is not implemented */
    uint8_t usagesIndex; /* 0 if 0x4522 is not implemented */
    struct hidpp_disable_keys_info info;
};

static void unpack_flags(struct hidpp_feat_disable_keys *dk) {
    dk->info.hasKeys = dk->keysIndex != 0;
    dk->info.hasUsages = dk->usagesIndex != 0;
}

static int query_info(hidpp_device_t *dev, struct hidpp_feat_disable_keys *dk) {
    struct hidpp_disable_keys_info *info = &dk->info;
    hidpp_packet_t req, res;
    int rc;

    if (dk->keysIndex) {
        make_packet(&req, dev, dk->keysIndex, 0);

        if (!(rc = hidpp_device_request(dev, &req, &res)))
            info->keys = res.params[0];
        else if (is_transient(dev, rc))
            return rc;
    }

    if (dk->usagesIndex) {
        make_packet(&req, dev, dk->usagesIndex, 0);

        if (!(rc = hidpp_device_request(dev, &req, &res)))
            info->maxUsages = res.params[0];
        else if (is_transient(dev, rc))
            return rc;
    }

    unpack_flags(dk);
    info->initialized = HIDPP_TRUE;
    return HIDPP_OK;
}

static int ensure_init(
    struct hidpp_feat_disable_keys **out, hidpp_device_t *dev
) {
    struct hidpp_feat_disable_keys *dk;

    if (!out || !dev)
        return HIDPP_EINVAL;

    dk = dev->features[HIDPP_FEAT_DISABLE_KEYS];
    *out = dk;

    if (dk->initialized)
        return dk->unsupported ? HIDPP_ENOSYS : HIDPP_OK;

    memset(dk, 0, sizeof(*dk));

    int keys = feature_lookup(dev, DISABLE_KEYS_ID);
    int usages = feature_lookup(dev, DISABLE_USAGES_ID);
    int rc;

    if (keys < 0 || usages < 0)
        return keys < 0 ? keys : usages;

    dk->keysIndex = (uint8_t)keys;
    dk->usagesIndex = (uint8_t)usages;
    dk->unsupported = !dk->keysIndex && !dk->usagesIndex;

    if (!dk->unsupported && (rc = query_info(dev, dk)))
        return rc;

    dk->initialized = HIDPP_TRUE;

    return dk->unsupported ? HIDPP_ENOSYS : HIDPP_OK;
}

static void clear_cache(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_disable_keys *dk = feat;
    (void)dev;
    dk->initialized = HIDPP_FALSE;
}

static void collect_cache(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_disable_keys *dk;
    (void)feat;
    ensure_init(&dk, dev);
}

static size_t save_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_disable_keys *dk = feat;
    uint8_t *start = b;
    (void)dev;

    SAVE_BYTE(b, dk->initialized);
    SAVE_BYTE(b, dk->unsupported);

    if (!dk->initialized || dk->unsupported)
        return b - start;

    SAVE_BYTE(b, dk->keysIndex);
    SAVE_BYTE(b, dk->usagesIndex);
    SAVE_BYTE(b, dk->info.initialized);
    SAVE_BYTE(b, dk->info.keys);
    SAVE_BYTE(b, dk->info.maxUsages);

    return b - start;
}

static size_t load_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_disable_keys *dk = feat;
    uint8_t *start = b;
    (void)dev;

    dk->initialized = LOAD_BYTE(b);
    dk->unsupported = LOAD_BYTE(b);

    if (!dk->initialized || dk->unsupported)
        return b - start;

    dk->keysIndex = LOAD_BYTE(b);
    dk->usagesIndex = LOAD_BYTE(b);
    dk->info.initialized = LOAD_BYTE(b);
    dk->info.keys = LOAD_BYTE(b);
    dk->info.maxUsages = LOAD_BYTE(b);
    unpack_flags(dk);

    return b - start;
}

const struct hidpp_feat_vt hidpp_feat_disable_keys_vt = {
    .size = sizeof(struct hidpp_feat_disable_keys),
    .alignment = _Alignof(struct hidpp_feat_disable_keys),
    .maxCacheSize = sizeof(struct hidpp_feat_disable_keys),
    .minCacheSize = 2 * sizeof(uint8_t),
    .collectCache = collect_cache,
    .saveCache = save_cache,
    .loadCache = load_cache,
    .clearCache = clear_cache,
};

static int ensure_keys(
    struct hidpp_feat_disable_keys **dk, hidpp_device_t *dev
) {
    int rc;

    if ((rc = ensure_init(dk, dev)))
        return rc;

    return (*dk)->keysIndex ? HIDPP_OK : HIDPP_ENOSYS;
}

static int ensure_usages(
    struct hidpp_feat_disable_keys **dk, hidpp_device_t *dev
) {
    int rc;

    if ((rc = ensure_init(dk, dev)))
        return rc;

    return (*dk)->usagesIndex ? HIDPP_OK : HIDPP_ENOSYS;
}

struct hidpp_disable_keys_info *hidpp_disable_keys_info(hidpp_device_t *dev) {
    struct hidpp_feat_disable_keys *dk;

    if (!dev || ensure_init(&dk, dev))
        return NULL;

    return &dk->info;
}

int hidpp_disable_keys_get(hidpp_device_t *dev) {
    struct hidpp_feat_disable_keys *dk;
    int rc;

    if ((rc = ensure_keys(&dk, dev)))
        return rc;

    hidpp_packet_t req, res;
    make_packet(&req, dev, dk->keysIndex, 1);

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    return res.params[0];
}

int hidpp_disable_keys_set(hidpp_device_t *dev, uint8_t mask) {
    struct hidpp_feat_disable_keys *dk;
    int rc;

    if ((rc = ensure_keys(&dk, dev)))
        return rc;

    /* only the keys the device offers can be disabled */
    if (mask & ~dk->info.keys)
        return HIDPP_EINVAL;

    hidpp_packet_t req, res;
    make_packet(&req, dev, dk->keysIndex, 2);
    req.params[0] = mask;

    return hidpp_device_request(dev, &req, &res);
}

static int send_usages(
    hidpp_device_t *dev, uint8_t func, const uint8_t *usages, size_t count
) {
    struct hidpp_feat_disable_keys *dk;
    int rc;

    if (!usages && count)
        return HIDPP_EINVAL;

    if ((rc = ensure_usages(&dk, dev)))
        return rc;

    /* a zero usage would end the list early */
    for (size_t i = 0; i < count; i++)
        if (usages[i] == 0)
            return HIDPP_EINVAL;

    hidpp_packet_t req, res;

    for (size_t start = 0; start < count; start += USAGES_PER_CALL) {
        size_t n = count - start;

        if (n > USAGES_PER_CALL)
            n = USAGES_PER_CALL;

        make_packet(&req, dev, dk->usagesIndex, func);
        req.kind = HIDPP_KIND_LONG;
        memcpy(req.params, &usages[start], n);

        if ((rc = hidpp_device_request(dev, &req, &res)))
            return rc;
    }

    return HIDPP_OK;
}

int hidpp_disable_usages(
    hidpp_device_t *dev, const uint8_t *usages, size_t count
) {
    return send_usages(dev, 1, usages, count);
}

int hidpp_enable_usages(
    hidpp_device_t *dev, const uint8_t *usages, size_t count
) {
    return send_usages(dev, 2, usages, count);
}

int hidpp_enable_all_usages(hidpp_device_t *dev) {
    struct hidpp_feat_disable_keys *dk;
    int rc;

    if ((rc = ensure_usages(&dk, dev)))
        return rc;

    hidpp_packet_t req, res;
    make_packet(&req, dev, dk->usagesIndex, 3);

    return hidpp_device_request(dev, &req, &res);
}
