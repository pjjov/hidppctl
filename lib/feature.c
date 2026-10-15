/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

/*  Life cycle of the feature objects: lookup of their features, lazy
    initialization, caching and event decoding. Objects whose vtable has
    no ids (the root) are only forwarded to their own hooks. */

#include "common.h"

static int is_managed(const struct hidpp_feat_vt *vt) { return vt->ids[0]; }

/* Resolves the ids of the vtable, see struct hidpp_feat_vt. */
static int lookup(
    hidpp_device_t *dev,
    const struct hidpp_feat_vt *vt,
    struct hidpp_feat_base *base
) {
    hidpp_bool_t found = HIDPP_FALSE;

    for (uint8_t i = 0; i < FEAT_MAX_IDS && vt->ids[i]; i++) {
        int index = feature_lookup(dev, vt->ids[i]);

        if (index < 0)
            return index; /* not known yet, ask again next time */

        base->indexes[i] = (uint8_t)index;

        if (index && !found) {
            found = HIDPP_TRUE;
            base->featIndex = (uint8_t)index;
            base->which = i;
        }

        if (found && !vt->allIds)
            break;
    }

    base->unsupported = !found;

    if (found)
        base->version = feature_version(dev, vt->ids[base->which]);

    return HIDPP_OK;
}

int hidpp__feature_init(hidpp_device_t *dev, int kind, void **out) {
    if (!out)
        return HIDPP_EINVAL;

    *out = NULL;

    if (!dev || kind < 0 || kind >= hidpp_feat_max)
        return HIDPP_EINVAL;

    const struct hidpp_feat_vt *vt = hidpp_feat_vtables[kind];
    struct hidpp_feat_base *base = dev->features[kind];
    int rc;

    *out = base;

    if (base->initialized)
        return base->unsupported ? HIDPP_ENOSYS : HIDPP_OK;

    memset(base, 0, vt->size);

    if (vt->init && (rc = vt->init(dev, base)))
        return rc;

    if ((rc = lookup(dev, vt, base)))
        return rc;

    /* the query can still find the feature unusable */
    if (!base->unsupported && vt->query && (rc = vt->query(dev, base)))
        return rc;

    base->initialized = HIDPP_TRUE;

    return base->unsupported ? HIDPP_ENOSYS : HIDPP_OK;
}

void hidpp__feature_collect(hidpp_device_t *dev, int kind) {
    const struct hidpp_feat_vt *vt = hidpp_feat_vtables[kind];
    void *feat = dev->features[kind];

    if (is_managed(vt) && hidpp__feature_init(dev, kind, &feat))
        return;

    if (vt->collectCache)
        vt->collectCache(dev, feat);
}

void hidpp__feature_clear(hidpp_device_t *dev, int kind) {
    const struct hidpp_feat_vt *vt = hidpp_feat_vtables[kind];
    struct hidpp_feat_base *base = dev->features[kind];

    if (!is_managed(vt)) {
        if (vt->clearCache)
            vt->clearCache(dev, base);
        return;
    }

    base->initialized = HIDPP_FALSE;
}

size_t hidpp__feature_save(hidpp_device_t *dev, int kind, uint8_t *b) {
    const struct hidpp_feat_vt *vt = hidpp_feat_vtables[kind];
    struct hidpp_feat_base *base = dev->features[kind];
    uint8_t *start = b;

    if (!is_managed(vt))
        return vt->saveCache ? vt->saveCache(dev, base, b) : 0;

    SAVE_BYTE(b, base->initialized);
    SAVE_BYTE(b, base->unsupported);

    if (!base->initialized || base->unsupported)
        return b - start;

    SAVE_BYTE(b, base->featIndex);
    SAVE_BYTE(b, base->which);
    SAVE_BYTE(b, base->version);

    for (int i = 0; i < FEAT_MAX_IDS; i++)
        SAVE_BYTE(b, base->indexes[i]);

    if (vt->saveCache)
        b += vt->saveCache(dev, base, b);

    return b - start;
}

size_t hidpp__feature_load(hidpp_device_t *dev, int kind, uint8_t *b) {
    const struct hidpp_feat_vt *vt = hidpp_feat_vtables[kind];
    struct hidpp_feat_base *base = dev->features[kind];
    uint8_t *start = b;

    if (!is_managed(vt))
        return vt->loadCache ? vt->loadCache(dev, base, b) : 0;

    /* whatever the loader does not set has to be the initial state */
    memset(base, 0, vt->size);

    if (vt->init)
        vt->init(dev, base);

    base->initialized = LOAD_BYTE(b);
    base->unsupported = LOAD_BYTE(b);

    if (!base->initialized || base->unsupported)
        return b - start;

    base->featIndex = LOAD_BYTE(b);
    base->which = LOAD_BYTE(b);
    base->version = LOAD_BYTE(b);

    for (int i = 0; i < FEAT_MAX_IDS; i++)
        base->indexes[i] = LOAD_BYTE(b);

    if (base->which >= FEAT_MAX_IDS)
        base->which = 0;

    if (vt->loadCache)
        b += vt->loadCache(dev, base, b);

    return b - start;
}

int hidpp__feature_parse_event(
    hidpp_device_t *dev,
    uint16_t id,
    const hidpp_packet_t *pkt,
    struct hidpp_event *out
) {
    /* function id is the high nibble, the low one is the software id */
    uint8_t func = HIDPP_MSN(pkt->func);

    for (int kind = 0; kind < hidpp_feat_max; kind++) {
        const struct hidpp_feat_vt *vt = hidpp_feat_vtables[kind];

        if (vt->parseEvent
            && vt->parseEvent(dev, dev->features[kind], id, func, pkt, out)
                == HIDPP_OK)
            return HIDPP_OK;
    }

    return HIDPP_ENOENT;
}
