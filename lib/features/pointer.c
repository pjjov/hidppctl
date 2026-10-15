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
    struct hidpp_feat_base base;
    struct hidpp_pointer_info info;
};

static void unpack_flags(struct hidpp_pointer_info *info) {
    info->acceleration = info->flags & ACCELERATION_MASK;
    info->suggestOsBallistics = !!(info->flags & OS_BALLISTICS_FLAG);
    info->suggestVerticalTuning = !!(info->flags & VERTICAL_TUNING_FLAG);
}

static int query_info(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_pointer *ptr = feat;
    struct hidpp_pointer_info *info = &ptr->info;
    hidpp_packet_t req, res;
    make_packet(&req, dev, ptr->base.featIndex, 0);

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
    void *feat;
    int rc = hidpp__feature_init(dev, HIDPP_FEAT_POINTER, &feat);
    *out = feat;
    return rc;
}

static size_t save_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_pointer *ptr = feat;
    uint8_t *start = b;
    (void)dev;

    SAVE_BYTE(b, ptr->info.initialized);
    SAVE_WORD(b, ptr->info.resolution);
    SAVE_BYTE(b, ptr->info.flags);

    return b - start;
}

static size_t load_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_pointer *ptr = feat;
    uint8_t *start = b;
    (void)dev;

    ptr->info.initialized = LOAD_BYTE(b);
    ptr->info.resolution = LOAD_WORD(b);
    ptr->info.flags = LOAD_BYTE(b);
    unpack_flags(&ptr->info);

    return b - start;
}

const struct hidpp_feat_vt hidpp_feat_pointer_vt = {
    FEAT_VT_SIZE(struct hidpp_feat_pointer),
    .ids = { POINTER_ID },
    .query = query_info,
    .saveCache = save_cache,
    .loadCache = load_cache,
};

struct hidpp_pointer_info *hidpp_pointer_info(hidpp_device_t *dev) {
    struct hidpp_feat_pointer *ptr;

    if (!dev || ensure_init(&ptr, dev))
        return NULL;

    return &ptr->info;
}
