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
    struct hidpp_feat_base base;
};

/* 0x40A3 is preferred over 0x40A2. */
static int is_multi_host(const struct hidpp_feat_fn_inversion *fn) {
    return fn->base.which == 0;
}

static int ensure_init(
    struct hidpp_feat_fn_inversion **out, hidpp_device_t *dev
) {
    void *feat;
    int rc = hidpp__feature_init(dev, HIDPP_FEAT_FN_INVERSION, &feat);
    *out = feat;
    return rc;
}

/* F-lock changes are only reported by 0x40A3. */
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

    if (id != MULTI_HOST_ID || func != 0)
        return HIDPP_ENOENT;

    e->type = HIDPP_EVENT_FN_INVERSION;
    e->as.fnInversion.host = p[0];
    e->as.fnInversion.enabled = !!(p[1] & STATE_FLAG);
    e->as.fnInversion.defaultEnabled = !!(p[2] & STATE_FLAG);
    e->as.fnInversion.hasFnLock = !!(p[3] & CAP_FN_LOCK_FLAG);
    return HIDPP_OK;
}

const struct hidpp_feat_vt hidpp_feat_fn_inversion_vt = {
    FEAT_VT_SIZE(struct hidpp_feat_fn_inversion),
    .ids = { MULTI_HOST_ID, FN_INVERSION_ID },
    .parseEvent = parse_event,
};

static void parse_state(
    const struct hidpp_feat_fn_inversion *fn,
    const hidpp_packet_t *res,
    struct hidpp_fn_inversion *out
) {
    memset(out, 0, sizeof(*out));

    if (is_multi_host(fn)) {
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
    if (!is_multi_host(*fn) && host != HIDPP_HOST_CURRENT)
        return HIDPP_EINVAL;

    make_packet(req, dev, (*fn)->base.featIndex, func);
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

    if (is_multi_host(fn))
        req.params[0] = host;

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    if (is_multi_host(fn) && host != HIDPP_HOST_CURRENT
        && res.params[0] != host)
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

    if (is_multi_host(fn)) {
        req.params[0] = host;
        req.params[1] = enabled ? 1 : 0;
    } else {
        req.params[0] = enabled ? 1 : 0;
    }

    return hidpp_device_request(dev, &req, &res);
}
