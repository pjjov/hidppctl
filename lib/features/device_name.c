/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

/** Section: HID++ feature documentation

    Device name and type (id: 0x0005)
    Big endian integers are exchanged.

    -----------------------
    Fn 0 - get name length
    -----------------------
    request:
    response:
        uint8_t nameLength

    -----------------------
    Fn 1 - get name slice
    -----------------------
    request:
        uint8_t index
    response:
        uint8_t[16|3] chars

    -----------------------
    Fn 2 - get device type
    -----------------------
    request:
    response:
        uint8_t type
*/

#define MAX_NAME_LEN 256

struct hidpp_feat_device_name {
    struct hidpp_feat_base base;
    uint8_t deviceType;
    uint8_t nameLength;
    char name[MAX_NAME_LEN + 1];
};

static int query_device_type(
    hidpp_device_t *dev, uint8_t featIndex, uint8_t *out
) {
    hidpp_packet_t req, res;
    make_packet(&req, dev, featIndex, 2);

    int rc;

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    *out = res.params[0];
    return HIDPP_OK;
}

static int query_name_length(
    hidpp_device_t *dev, uint8_t featIndex, uint8_t *out
) {
    hidpp_packet_t req, res;
    make_packet(&req, dev, featIndex, 0);

    int rc;

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    *out = res.params[0];
    return HIDPP_OK;
}

static int query_device_name(
    hidpp_device_t *dev, uint8_t featIndex, size_t len, char *buf
) {
    int rc = HIDPP_OK;
    hidpp_packet_t req, res;
    make_packet(&req, dev, featIndex, 1);

    size_t read = 0;

    while (read < len && read < MAX_NAME_LEN) {
        req.params[0] = read;

        if ((rc = hidpp_device_request(dev, &req, &res)))
            break;

        size_t psize = packet_length(res.kind) - 4;
        size_t size = len - read < psize ? len - read : psize;

        if (size == 0)
            break;

        memcpy(&buf[read], res.params, size);
        read += size;
    }

    buf[read] = '\0';
    return rc;
}

/* Answers the device refuses for good are left empty. */
static int query_info(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_device_name *dn = feat;
    uint8_t index = dn->base.featIndex;
    int rc;

    rc = query_device_type(dev, index, &dn->deviceType);

    if (rc && is_transient(dev, rc))
        return rc;

    rc = query_name_length(dev, index, &dn->nameLength);

    if (rc && is_transient(dev, rc))
        return rc;

    rc = query_device_name(dev, index, dn->nameLength, dn->name);

    return rc && is_transient(dev, rc) ? rc : HIDPP_OK;
}

static int ensure_init(
    struct hidpp_feat_device_name **out, hidpp_device_t *dev
) {
    void *feat;
    int rc = hidpp__feature_init(dev, HIDPP_FEAT_DEVICE_NAME, &feat);
    *out = feat;
    return rc;
}

static size_t save_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_device_name *dn = feat;
    uint8_t *start = b;
    (void)dev;

    SAVE_BYTE(b, dn->deviceType);
    SAVE_BYTE(b, dn->nameLength);
    SAVE_BYTES(b, dn->name, MAX_NAME_LEN);

    return b - start;
}

static size_t load_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_device_name *dn = feat;
    uint8_t *start = b;
    (void)dev;

    dn->deviceType = LOAD_BYTE(b);
    dn->nameLength = LOAD_BYTE(b);
    LOAD_BYTES(b, dn->name, MAX_NAME_LEN);
    dn->name[MAX_NAME_LEN] = '\0';

    return b - start;
}

const struct hidpp_feat_vt hidpp_feat_device_name_vt = {
    FEAT_VT_SIZE(struct hidpp_feat_device_name),
    .ids = { 0x0005 },
    .query = query_info,
    .saveCache = save_cache,
    .loadCache = load_cache,
};

int hidpp_device_type(hidpp_device_t *dev) {
    if (!dev)
        return HIDPP_EINVAL;

    struct hidpp_feat_device_name *dn;
    int rc;

    if ((rc = ensure_init(&dn, dev)))
        return rc;

    return dn->deviceType;
}

const char *hidpp_device_name(hidpp_device_t *dev) {
    struct hidpp_feat_device_name *dn;

    if (!dev || ensure_init(&dn, dev))
        return NULL;

    return dn->name;
}
