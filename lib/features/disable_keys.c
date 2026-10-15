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

/* positions of the features in the ids of the vtable */
enum { KEYS, USAGES };

struct hidpp_feat_disable_keys {
    struct hidpp_feat_base base;
    struct hidpp_disable_keys_info info;
};

static void unpack_flags(struct hidpp_feat_disable_keys *dk) {
    dk->info.hasKeys = dk->base.indexes[KEYS] != 0;
    dk->info.hasUsages = dk->base.indexes[USAGES] != 0;
}

static int query_info(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_disable_keys *dk = feat;
    struct hidpp_disable_keys_info *info = &dk->info;
    hidpp_packet_t req, res;
    int rc;

    if (dk->base.indexes[KEYS]) {
        make_packet(&req, dev, dk->base.indexes[KEYS], 0);

        if (!(rc = hidpp_device_request(dev, &req, &res)))
            info->keys = res.params[0];
        else if (is_transient(dev, rc))
            return rc;
    }

    if (dk->base.indexes[USAGES]) {
        make_packet(&req, dev, dk->base.indexes[USAGES], 0);

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
    void *feat;
    int rc = hidpp__feature_init(dev, HIDPP_FEAT_DISABLE_KEYS, &feat);
    *out = feat;
    return rc;
}

static size_t save_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_disable_keys *dk = feat;
    uint8_t *start = b;
    (void)dev;

    SAVE_BYTE(b, dk->info.initialized);
    SAVE_BYTE(b, dk->info.keys);
    SAVE_BYTE(b, dk->info.maxUsages);

    return b - start;
}

static size_t load_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_disable_keys *dk = feat;
    uint8_t *start = b;
    (void)dev;

    dk->info.initialized = LOAD_BYTE(b);
    dk->info.keys = LOAD_BYTE(b);
    dk->info.maxUsages = LOAD_BYTE(b);
    unpack_flags(dk);

    return b - start;
}

const struct hidpp_feat_vt hidpp_feat_disable_keys_vt = {
    FEAT_VT_SIZE(struct hidpp_feat_disable_keys),
    .ids = { DISABLE_KEYS_ID, DISABLE_USAGES_ID },
    .allIds = HIDPP_TRUE,
    .query = query_info,
    .saveCache = save_cache,
    .loadCache = load_cache,
};

static int ensure_keys(
    struct hidpp_feat_disable_keys **dk, hidpp_device_t *dev
) {
    int rc;

    if ((rc = ensure_init(dk, dev)))
        return rc;

    return (*dk)->base.indexes[KEYS] ? HIDPP_OK : HIDPP_ENOSYS;
}

static int ensure_usages(
    struct hidpp_feat_disable_keys **dk, hidpp_device_t *dev
) {
    int rc;

    if ((rc = ensure_init(dk, dev)))
        return rc;

    return (*dk)->base.indexes[USAGES] ? HIDPP_OK : HIDPP_ENOSYS;
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
    make_packet(&req, dev, dk->base.indexes[KEYS], 1);

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
    make_packet(&req, dev, dk->base.indexes[KEYS], 2);
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

        make_packet(&req, dev, dk->base.indexes[USAGES], func);
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
    make_packet(&req, dev, dk->base.indexes[USAGES], 3);

    return hidpp_device_request(dev, &req, &res);
}
