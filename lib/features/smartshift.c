/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

/** Section: HID++ feature documentation

    SmartShift 3G/EPM wheel (id: 0x2110)

    -----------------------
    Fn 0 - get ratchet control mode
    -----------------------
    request:
    response:
        uint8_t wheelMode (1 = freespin, 2 = ratchet)
        uint8_t autoDisengage (N/4 turns per second, 0xFF = always engaged)
        uint8_t autoDisengageDefault

    -----------------------
    Fn 1 - set ratchet control mode (persistent, 0 = do not change)
    -----------------------
    request:
        uint8_t wheelMode
        uint8_t autoDisengage
        uint8_t autoDisengageDefault
    response:
        (echoes the request)

    SmartShift 3G/EPM wheel with tunable torque (id: 0x2111)

    -----------------------
    Fn 0 - get capabilities
    -----------------------
    request:
    response:
        uint8_t capabilities
        - bit 0 - tunable torque
        uint8_t autoDisengageDefault
        uint8_t defaultTunableTorque (percent of maxForce)
        uint16_t maxForce (gram-force)

    -----------------------
    Fn 1 - get ratchet control mode
    -----------------------
    request:
    response:
        uint8_t wheelMode
        uint8_t autoDisengage
        uint8_t currentTunableTorque

    -----------------------
    Fn 2 - set ratchet control mode (persistent, 0 = do not change)
    -----------------------
    request:
        uint8_t wheelMode
        uint8_t autoDisengage
        uint8_t currentTunableTorque
    response:
*/

#define SMARTSHIFT_ID 0x2110
#define ENHANCED_ID 0x2111

#define CAP_TUNABLE_TORQUE_FLAG 0x01
#define MAX_TORQUE 100

struct hidpp_feat_smartshift {
    struct hidpp_feat_base base;
    uint8_t capabilities;
    struct hidpp_smartshift_info info;
};

static void unpack_flags(struct hidpp_feat_smartshift *ss) {
    ss->info.hasTunableTorque = ss->info.isEnhanced
        && !!(ss->capabilities & CAP_TUNABLE_TORQUE_FLAG);
}

static int query_info(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_smartshift *ss = feat;
    ss->info.isEnhanced = ss->base.which == 0; /* 0x2111 */
    struct hidpp_smartshift_info *info = &ss->info;

    if (info->isEnhanced) {
        hidpp_packet_t req, res;
        make_packet(&req, dev, ss->base.featIndex, 0);

        int rc;

        if ((rc = hidpp_device_request(dev, &req, &res)))
            return is_transient(dev, rc) ? rc : HIDPP_OK;

        ss->capabilities = res.params[0];
        info->defaultAutoDisengage = res.params[1];
        info->defaultTorque = res.params[2];
        info->maxForce = HIDPP_WORD(res.params[3], res.params[4]);
    }

    unpack_flags(ss);
    info->initialized = HIDPP_TRUE;
    return HIDPP_OK;
}

static int ensure_init(
    struct hidpp_feat_smartshift **out, hidpp_device_t *dev
) {
    void *feat;
    int rc = hidpp__feature_init(dev, HIDPP_FEAT_SMARTSHIFT, &feat);
    *out = feat;
    return rc;
}

static size_t save_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_smartshift *ss = feat;
    uint8_t *start = b;
    (void)dev;

    SAVE_BYTE(b, ss->capabilities);
    SAVE_BYTE(b, ss->info.initialized);
    SAVE_BYTE(b, ss->info.defaultAutoDisengage);
    SAVE_BYTE(b, ss->info.defaultTorque);
    SAVE_WORD(b, ss->info.maxForce);

    return b - start;
}

static size_t load_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_smartshift *ss = feat;
    uint8_t *start = b;
    (void)dev;

    ss->capabilities = LOAD_BYTE(b);
    ss->info.initialized = LOAD_BYTE(b);
    ss->info.isEnhanced = ss->base.which == 0; /* 0x2111 */
    ss->info.defaultAutoDisengage = LOAD_BYTE(b);
    ss->info.defaultTorque = LOAD_BYTE(b);
    ss->info.maxForce = LOAD_WORD(b);
    unpack_flags(ss);

    return b - start;
}

const struct hidpp_feat_vt hidpp_feat_smartshift_vt = {
    FEAT_VT_SIZE(struct hidpp_feat_smartshift),
    .ids = { ENHANCED_ID, SMARTSHIFT_ID },
    .query = query_info,
    .saveCache = save_cache,
    .loadCache = load_cache,
};

struct hidpp_smartshift_info *hidpp_smartshift_info(hidpp_device_t *dev) {
    struct hidpp_feat_smartshift *ss;

    if (!dev || ensure_init(&ss, dev))
        return NULL;

    return &ss->info;
}

int hidpp_smartshift_get(hidpp_device_t *dev, struct hidpp_smartshift *out) {
    struct hidpp_feat_smartshift *ss;
    int rc;

    if (!out)
        return HIDPP_EINVAL;

    if ((rc = ensure_init(&ss, dev)))
        return rc;

    hidpp_packet_t req, res;
    make_packet(&req, dev, ss->base.featIndex, ss->info.isEnhanced ? 1 : 0);

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    memset(out, 0, sizeof(*out));
    out->wheelMode = res.params[0];
    out->autoDisengage = res.params[1];

    if (ss->info.isEnhanced)
        out->torque = res.params[2];
    else
        out->autoDisengageDefault = res.params[2];

    return HIDPP_OK;
}

int hidpp_smartshift_set(
    hidpp_device_t *dev, const struct hidpp_smartshift *cfg
) {
    struct hidpp_feat_smartshift *ss;
    int rc;

    if (!cfg)
        return HIDPP_EINVAL;

    if ((rc = ensure_init(&ss, dev)))
        return rc;

    const struct hidpp_smartshift_info *info = &ss->info;

    if (cfg->wheelMode > HIDPP_WHEEL_MODE_RATCHET)
        return HIDPP_EINVAL;

    /* each feature version only has one of the two */
    if (info->isEnhanced && cfg->autoDisengageDefault)
        return HIDPP_EINVAL;

    if (!info->hasTunableTorque && cfg->torque)
        return HIDPP_EINVAL;

    if (cfg->torque > MAX_TORQUE)
        return HIDPP_EINVAL;

    hidpp_packet_t req, res;
    make_packet(&req, dev, ss->base.featIndex, info->isEnhanced ? 2 : 1);
    req.params[0] = cfg->wheelMode;
    req.params[1] = cfg->autoDisengage;
    req.params[2] = info->isEnhanced ? cfg->torque : cfg->autoDisengageDefault;

    return hidpp_device_request(dev, &req, &res);
}
