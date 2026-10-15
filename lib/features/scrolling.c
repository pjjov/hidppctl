/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

/** Section: HID++ feature documentation

    Vertical scrolling (id: 0x2100)

    -----------------------
    Fn 0 - get roller info
    -----------------------
    request:
    response:
        uint8_t rollerType
        uint8_t ratchetsPerTurn
        uint8_t scrollLines (0 = keep the OS setting, 0xFF = one page)

    High resolution scrolling (id: 0x2120)
    Its documentation is part of 0x2100's, but its functions are numbered
    from 0 like a feature of its own.

    -----------------------
    Fn 0 - get high resolution scrolling mode
    -----------------------
    request:
    response:
        uint8_t mode (0 = disabled, 1 = enabled)
        uint8_t multiplier

    -----------------------
    Fn 1 - set high resolution scrolling mode
    -----------------------
    request:
        uint8_t mode
    response:
        uint8_t mode
        uint8_t multiplier
*/

#define SCROLLING_ID 0x2100
#define HIGHRES_ID 0x2120

/* positions of the features in the ids of the vtable */
enum { ROLLER, HIGHRES };

struct hidpp_feat_scrolling {
    struct hidpp_feat_base base;
    struct hidpp_scroll_info info;
};

static const char *roller_type_name(uint8_t type) {
    switch (type) {
        /* clang-format off */
    case HIDPP_ROLLER_STANDARD:          return "Standard roller";
    case HIDPP_ROLLER_3G:                return "3G roller";
    case HIDPP_ROLLER_MICRO_RATCHET:     return "Micro-ratchet";
    case HIDPP_ROLLER_TOUCH_PAD:         return "Touch pad";
    case HIDPP_ROLLER_TOUCH_PAD_NATURAL: return "Touch pad, natural scrolling";
    default:                             return "Unknown";
        /* clang-format on */
    }
}

static void unpack_flags(struct hidpp_feat_scrolling *sc) {
    sc->info.hasHighRes = sc->base.indexes[HIGHRES] != 0;
    sc->info.rollerTypeName = roller_type_name(sc->info.rollerType);
}

static int query_info(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_scrolling *sc = feat;
    struct hidpp_scroll_info *info = &sc->info;
    hidpp_packet_t req, res;
    int rc;

    if (sc->base.indexes[ROLLER]) {
        make_packet(&req, dev, sc->base.indexes[ROLLER], 0);

        if (!(rc = hidpp_device_request(dev, &req, &res))) {
            info->rollerType = res.params[0];
            info->ratchetsPerTurn = res.params[1];
            info->scrollLines = res.params[2];
            info->hasRollerInfo = HIDPP_TRUE;
        } else if (is_transient(dev, rc)) {
            return rc;
        }
    }

    /* the multiplier does not change with the mode */
    if (sc->base.indexes[HIGHRES]) {
        make_packet(&req, dev, sc->base.indexes[HIGHRES], 0);

        if (!(rc = hidpp_device_request(dev, &req, &res)))
            info->multiplier = res.params[1];
        else if (is_transient(dev, rc))
            return rc;
    }

    unpack_flags(sc);
    info->initialized = HIDPP_TRUE;
    return HIDPP_OK;
}

static int ensure_init(struct hidpp_feat_scrolling **out, hidpp_device_t *dev) {
    void *feat;
    int rc = hidpp__feature_init(dev, HIDPP_FEAT_SCROLLING, &feat);
    *out = feat;
    return rc;
}

static size_t save_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_scrolling *sc = feat;
    uint8_t *start = b;
    (void)dev;

    SAVE_BYTE(b, sc->info.initialized);
    SAVE_BYTE(b, sc->info.hasRollerInfo);
    SAVE_BYTE(b, sc->info.rollerType);
    SAVE_BYTE(b, sc->info.ratchetsPerTurn);
    SAVE_BYTE(b, sc->info.scrollLines);
    SAVE_BYTE(b, sc->info.multiplier);

    return b - start;
}

static size_t load_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_scrolling *sc = feat;
    uint8_t *start = b;
    (void)dev;

    sc->info.initialized = LOAD_BYTE(b);
    sc->info.hasRollerInfo = LOAD_BYTE(b);
    sc->info.rollerType = LOAD_BYTE(b);
    sc->info.ratchetsPerTurn = LOAD_BYTE(b);
    sc->info.scrollLines = LOAD_BYTE(b);
    sc->info.multiplier = LOAD_BYTE(b);
    unpack_flags(sc);

    return b - start;
}

const struct hidpp_feat_vt hidpp_feat_scrolling_vt = {
    FEAT_VT_SIZE(struct hidpp_feat_scrolling),
    .ids = { SCROLLING_ID, HIGHRES_ID },
    .allIds = HIDPP_TRUE,
    .query = query_info,
    .saveCache = save_cache,
    .loadCache = load_cache,
};

static int ensure_highres(
    struct hidpp_feat_scrolling **sc, hidpp_device_t *dev
) {
    int rc;

    if ((rc = ensure_init(sc, dev)))
        return rc;

    return (*sc)->base.indexes[HIGHRES] ? HIDPP_OK : HIDPP_ENOSYS;
}

struct hidpp_scroll_info *hidpp_scroll_info(hidpp_device_t *dev) {
    struct hidpp_feat_scrolling *sc;

    if (!dev || ensure_init(&sc, dev))
        return NULL;

    return &sc->info;
}

int hidpp_scroll_highres(hidpp_device_t *dev) {
    struct hidpp_feat_scrolling *sc;
    int rc;

    if ((rc = ensure_highres(&sc, dev)))
        return rc;

    hidpp_packet_t req, res;
    make_packet(&req, dev, sc->base.indexes[HIGHRES], 0);

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    return res.params[0] & 1;
}

int hidpp_scroll_set_highres(hidpp_device_t *dev, int enabled) {
    struct hidpp_feat_scrolling *sc;
    int rc;

    if ((rc = ensure_highres(&sc, dev)))
        return rc;

    hidpp_packet_t req, res;
    make_packet(&req, dev, sc->base.indexes[HIGHRES], 1);
    req.params[0] = enabled ? 1 : 0;

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    return (res.params[0] & 1) == req.params[0] ? HIDPP_OK : HIDPP_EIO;
}
