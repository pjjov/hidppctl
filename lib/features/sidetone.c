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
    struct hidpp_feat_base base;
};

static int ensure_init(struct hidpp_feat_sidetone **out, hidpp_device_t *dev) {
    void *feat;
    int rc = hidpp__feature_init(dev, HIDPP_FEAT_SIDETONE, &feat);
    *out = feat;
    return rc;
}

static int parse_event(
    hidpp_device_t *dev,
    void *feat,
    uint16_t id,
    uint8_t func,
    const hidpp_packet_t *pkt,
    struct hidpp_event *e
) {
    (void)dev, (void)feat;

    if (id != SIDETONE_ID || func != 0)
        return HIDPP_ENOENT;

    e->type = HIDPP_EVENT_SIDETONE;
    e->as.sidetone.channel = pkt->params[0];
    e->as.sidetone.level = pkt->params[1];
    e->as.sidetone.mute = pkt->params[2];
    return HIDPP_OK;
}

const struct hidpp_feat_vt hidpp_feat_sidetone_vt = {
    FEAT_VT_SIZE(struct hidpp_feat_sidetone),
    .ids = { SIDETONE_ID },
    .parseEvent = parse_event,
};

static int ensure_mute(struct hidpp_feat_sidetone **st, hidpp_device_t *dev) {
    int rc;

    if ((rc = ensure_init(st, dev)))
        return rc;

    return (*st)->base.version >= 1 ? HIDPP_OK : HIDPP_ENOSYS;
}

int hidpp_sidetone_get(hidpp_device_t *dev) {
    struct hidpp_feat_sidetone *st;
    int rc;

    if ((rc = ensure_init(&st, dev)))
        return rc;

    hidpp_packet_t req, res;
    make_packet(&req, dev, st->base.featIndex, 0);

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
    make_packet(&req, dev, st->base.featIndex, 1);
    req.params[0] = level;

    return hidpp_device_request(dev, &req, &res);
}

int hidpp_sidetone_mute(hidpp_device_t *dev) {
    struct hidpp_feat_sidetone *st;
    int rc;

    if ((rc = ensure_mute(&st, dev)))
        return rc;

    hidpp_packet_t req, res;
    make_packet(&req, dev, st->base.featIndex, 2);

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
    make_packet(&req, dev, st->base.featIndex, 3);
    req.params[0] = mask;
    req.params[1] = mute & mask;

    return hidpp_device_request(dev, &req, &res);
}
