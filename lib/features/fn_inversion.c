/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

/** Section: HID++ feature documentation

    Fn inversion (id: 0x40A2)

    -----------------------
    Fn 0 - get global Fn inversion
    -----------------------
    request:
    response:
        uint8_t state
        - bit 0 - Fn inversion on
        - bit 1 - Fn inversion on by default

    -----------------------
    Fn 1 - set global Fn inversion
    -----------------------
    request:
        uint8_t state (bit 0)
    response:
        uint8_t state

    Fn inversion for multi-host devices (id: 0x40A3)
    The request table of the setter lists the state first, while its
    signature, its response and devices in the wild put the host first.

    -----------------------
    Fn 0 - get global Fn inversion
    -----------------------
    request:
        uint8_t host (0xFF is the current host)
    response:
        uint8_t host
        uint8_t state
        uint8_t defaultState
        uint8_t capabilities
        - bit 0 - Fn lock (the user can toggle it, e.g. with Fn+Esc)

    -----------------------
    Fn 1 - set global Fn inversion
    -----------------------
    request:
        uint8_t host
        uint8_t state
    response:
        (same as get global Fn inversion)

    -----------------------
    Event 0 - F-lock change (current host, changed by the user)
    -----------------------
        (same as get global Fn inversion)
*/

#define FN_INVERSION_ID 0x40A2
#define MULTI_HOST_ID 0x40A3

#define STATE_FLAG 0x01
#define DEFAULT_STATE_FLAG 0x02
#define CAP_FN_LOCK_FLAG 0x01

struct hidpp_feat_fn_inversion {
    hidpp_bool_t initialized;
    hidpp_bool_t unsupported;

    uint8_t featIndex;
    hidpp_bool_t isMultiHost;
};

static int ensure_init(
    struct hidpp_feat_fn_inversion **out, hidpp_device_t *dev
) {
    struct hidpp_feat_fn_inversion *fn;

    if (!out || !dev)
        return HIDPP_EINVAL;

    fn = dev->features[HIDPP_FEAT_FN_INVERSION];
    *out = fn;

    if (fn->initialized)
        return fn->unsupported ? HIDPP_ENOSYS : HIDPP_OK;

    memset(fn, 0, sizeof(*fn));
    int featIndex = feature_lookup(dev, MULTI_HOST_ID);

    if (featIndex < 0)
        return featIndex;

    fn->isMultiHost = featIndex != 0;

    if (featIndex == 0
        && (featIndex = feature_lookup(dev, FN_INVERSION_ID)) < 0)
        return featIndex;

    fn->featIndex = (uint8_t)featIndex;
    fn->unsupported = featIndex == 0;
    fn->initialized = HIDPP_TRUE;

    return fn->unsupported ? HIDPP_ENOSYS : HIDPP_OK;
}

static void clear_cache(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_fn_inversion *fn = feat;
    (void)dev;
    fn->initialized = HIDPP_FALSE;
}

static void collect_cache(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_fn_inversion *fn;
    (void)feat;
    ensure_init(&fn, dev);
}

static size_t save_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_fn_inversion *fn = feat;
    uint8_t *start = b;
    (void)dev;

    SAVE_BYTE(b, fn->initialized);
    SAVE_BYTE(b, fn->unsupported);

    if (!fn->initialized || fn->unsupported)
        return b - start;

    SAVE_BYTE(b, fn->featIndex);
    SAVE_BYTE(b, fn->isMultiHost);

    return b - start;
}

static size_t load_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_fn_inversion *fn = feat;
    uint8_t *start = b;
    (void)dev;

    fn->initialized = LOAD_BYTE(b);
    fn->unsupported = LOAD_BYTE(b);

    if (!fn->initialized || fn->unsupported)
        return b - start;

    fn->featIndex = LOAD_BYTE(b);
    fn->isMultiHost = LOAD_BYTE(b);

    return b - start;
}

const struct hidpp_feat_vt hidpp_feat_fn_inversion_vt = {
    .size = sizeof(struct hidpp_feat_fn_inversion),
    .alignment = _Alignof(struct hidpp_feat_fn_inversion),
    .maxCacheSize = sizeof(struct hidpp_feat_fn_inversion),
    .minCacheSize = 2 * sizeof(uint8_t),
    .collectCache = collect_cache,
    .saveCache = save_cache,
    .loadCache = load_cache,
    .clearCache = clear_cache,
};

static void parse_state(
    const struct hidpp_feat_fn_inversion *fn,
    const hidpp_packet_t *res,
    struct hidpp_fn_inversion *out
) {
    memset(out, 0, sizeof(*out));

    if (fn->isMultiHost) {
        out->host = res->params[0];
        out->enabled = !!(res->params[1] & STATE_FLAG);
        out->defaultEnabled = !!(res->params[2] & STATE_FLAG);
        out->hasFnLock = !!(res->params[3] & CAP_FN_LOCK_FLAG);
    } else {
        out->host = HIDPP_HOST_CURRENT;
        out->enabled = !!(res->params[0] & STATE_FLAG);
        out->defaultEnabled = !!(res->params[0] & DEFAULT_STATE_FLAG);
    }
}

static int prepare(
    hidpp_device_t *dev,
    uint8_t host,
    uint8_t func,
    struct hidpp_feat_fn_inversion **fn,
    hidpp_packet_t *req
) {
    int rc;

    if ((rc = ensure_init(fn, dev)))
        return rc;

    /* single host devices only know the current one */
    if (!(*fn)->isMultiHost && host != HIDPP_HOST_CURRENT)
        return HIDPP_EINVAL;

    make_packet(req, dev, (*fn)->featIndex, func);
    return HIDPP_OK;
}

int hidpp_fn_inversion_get(
    hidpp_device_t *dev, uint8_t host, struct hidpp_fn_inversion *out
) {
    struct hidpp_feat_fn_inversion *fn;
    hidpp_packet_t req, res;
    int rc;

    if (!out)
        return HIDPP_EINVAL;

    if ((rc = prepare(dev, host, 0, &fn, &req)))
        return rc;

    if (fn->isMultiHost)
        req.params[0] = host;

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    if (fn->isMultiHost && host != HIDPP_HOST_CURRENT && res.params[0] != host)
        return HIDPP_EIO;

    parse_state(fn, &res, out);
    return HIDPP_OK;
}

int hidpp_fn_inversion_set(hidpp_device_t *dev, uint8_t host, int enabled) {
    struct hidpp_feat_fn_inversion *fn;
    hidpp_packet_t req, res;
    int rc;

    if ((rc = prepare(dev, host, 1, &fn, &req)))
        return rc;

    if (fn->isMultiHost) {
        req.params[0] = host;
        req.params[1] = enabled ? 1 : 0;
    } else {
        req.params[0] = enabled ? 1 : 0;
    }

    return hidpp_device_request(dev, &req, &res);
}
