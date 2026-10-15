/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

/** Section: HID++ feature documentation

    Mouse pointer basic optical sensors (id: 0x2200)
    Big endian integers are exchanged.

    -----------------------
    Fn 0 - get mouse pointer info
    -----------------------
    request:
    response:
        uint16_t resolution (typical DPI, +/- 20% depending on the surface)
        uint8_t flags
        - bits 0-1 - suggested pointer acceleration (none, low, med, high)
        - bit 2 - suggest OS ballistics
        - bit 3 - suggest vertical tuning (trackballs)
*/

#define POINTER_ID 0x2200

/* clang-format off */

#define ACCELERATION_MASK      0x03
#define OS_BALLISTICS_FLAG     0x04
#define VERTICAL_TUNING_FLAG   0x08

/* clang-format on */

struct hidpp_feat_pointer {
    hidpp_bool_t initialized;
    hidpp_bool_t unsupported;

    uint8_t featIndex;
    struct hidpp_pointer_info info;
};

static void unpack_flags(struct hidpp_pointer_info *info) {
    info->acceleration = info->flags & ACCELERATION_MASK;
    info->suggestOsBallistics = !!(info->flags & OS_BALLISTICS_FLAG);
    info->suggestVerticalTuning = !!(info->flags & VERTICAL_TUNING_FLAG);
}

static int query_info(hidpp_device_t *dev, struct hidpp_feat_pointer *ptr) {
    struct hidpp_pointer_info *info = &ptr->info;
    hidpp_packet_t req, res;
    make_packet(&req, dev, ptr->featIndex, 0);

    int rc;

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return is_transient(dev, rc) ? rc : HIDPP_OK;

    info->resolution = HIDPP_WORD(res.params[0], res.params[1]);
    info->flags = res.params[2];
    unpack_flags(info);
    info->initialized = HIDPP_TRUE;
    return HIDPP_OK;
}

static int ensure_init(struct hidpp_feat_pointer **out, hidpp_device_t *dev) {
    struct hidpp_feat_pointer *ptr;

    if (!out || !dev)
        return HIDPP_EINVAL;

    ptr = dev->features[HIDPP_FEAT_POINTER];
    *out = ptr;

    if (ptr->initialized)
        return ptr->unsupported ? HIDPP_ENOSYS : HIDPP_OK;

    memset(ptr, 0, sizeof(*ptr));
    int featIndex = feature_lookup(dev, POINTER_ID);
    int rc;

    if (featIndex < 0)
        return featIndex;

    ptr->featIndex = (uint8_t)featIndex;
    ptr->unsupported = featIndex == 0;

    if (!ptr->unsupported && (rc = query_info(dev, ptr)))
        return rc;

    ptr->initialized = HIDPP_TRUE;

    return ptr->unsupported ? HIDPP_ENOSYS : HIDPP_OK;
}

static void clear_cache(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_pointer *ptr = feat;
    (void)dev;
    ptr->initialized = HIDPP_FALSE;
}

static void collect_cache(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_pointer *ptr;
    (void)feat;
    ensure_init(&ptr, dev);
}

static size_t save_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_pointer *ptr = feat;
    uint8_t *start = b;
    (void)dev;

    SAVE_BYTE(b, ptr->initialized);
    SAVE_BYTE(b, ptr->unsupported);

    if (!ptr->initialized || ptr->unsupported)
        return b - start;

    SAVE_BYTE(b, ptr->featIndex);
    SAVE_BYTE(b, ptr->info.initialized);
    SAVE_WORD(b, ptr->info.resolution);
    SAVE_BYTE(b, ptr->info.flags);

    return b - start;
}

static size_t load_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_pointer *ptr = feat;
    uint8_t *start = b;
    (void)dev;

    ptr->initialized = LOAD_BYTE(b);
    ptr->unsupported = LOAD_BYTE(b);

    if (!ptr->initialized || ptr->unsupported)
        return b - start;

    ptr->featIndex = LOAD_BYTE(b);
    ptr->info.initialized = LOAD_BYTE(b);
    ptr->info.resolution = LOAD_WORD(b);
    ptr->info.flags = LOAD_BYTE(b);
    unpack_flags(&ptr->info);

    return b - start;
}

const struct hidpp_feat_vt hidpp_feat_pointer_vt = {
    .size = sizeof(struct hidpp_feat_pointer),
    .alignment = _Alignof(struct hidpp_feat_pointer),
    .maxCacheSize = sizeof(struct hidpp_feat_pointer),
    .minCacheSize = 2 * sizeof(uint8_t),
    .collectCache = collect_cache,
    .saveCache = save_cache,
    .loadCache = load_cache,
    .clearCache = clear_cache,
};

struct hidpp_pointer_info *hidpp_pointer_info(hidpp_device_t *dev) {
    struct hidpp_feat_pointer *ptr;

    if (!dev || ensure_init(&ptr, dev))
        return NULL;

    return &ptr->info;
}
