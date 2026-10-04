/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"
#include "hidpp.h"

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
    hidpp_bool_t initialized;
    hidpp_bool_t unsupported;

    uint8_t featIndex;
    uint8_t deviceType;
    uint8_t nameLength;
    char name[MAX_NAME_LEN + 1];
};

static uint8_t query_device_type(hidpp_device_t *dev, uint8_t featIndex) {
    hidpp_packet_t req, res;
    make_packet(&req, dev, featIndex, 2);

    if (hidpp_device_request(dev, &req, &res))
        return 0;

    return res.params[0];
}

static uint8_t query_name_length(hidpp_device_t *dev, uint8_t featIndex) {
    hidpp_packet_t req, res;
    make_packet(&req, dev, featIndex, 0);

    if (hidpp_device_request(dev, &req, &res))
        return 0;

    return res.params[0];
}

static void query_device_name(
    hidpp_device_t *dev, uint8_t featIndex, size_t len, char *buf
) {
    hidpp_packet_t req, res;
    make_packet(&req, dev, featIndex, 1);

    size_t read = 0;

    while (read < len && read < UINT8_MAX) {
        req.params[0] = read;

        if (hidpp_device_request(dev, &req, &res))
            break;

        size_t psize = packet_length(res.kind);
        size_t size = len - read < psize ? len - read : psize;

        memcpy(&buf[read], res.params, size);
        read += size;
    }

    buf[read] = '\0';
}

static int ensure_init(
    struct hidpp_feat_device_name **out, hidpp_device_t *dev
) {
    struct hidpp_feat_device_name *dn;

    if (!out || !dev)
        return HIDPP_EINVAL;

    dn = dev->features[HIDPP_FEAT_FIRMWARE_INFO];
    *out = dn;

    if (dn->initialized)
        return dn->unsupported ? HIDPP_ENOSYS : HIDPP_OK;

    memset(dn->name, 0, sizeof(dn->name));
    int featIndex = hidpp_feature_index(dev, 0x0005);
    dn->featIndex = featIndex;
    dn->unsupported = featIndex <= 0;

    if (!dn->unsupported) {
        dn->deviceType = query_device_type(dev, dn->featIndex);
        dn->nameLength = query_name_length(dev, dn->featIndex);
        query_device_name(dev, dn->featIndex, dn->nameLength, dn->name);
    }

    dn->initialized = HIDPP_TRUE;

    return HIDPP_OK;
}

static void clear_cache(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_device_name *dn = feat;
    dn->initialized = HIDPP_FALSE;
}

static void collect_cache(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_device_name *dn;
    (void)feat;
    ensure_init(&dn, dev);
}

static void save_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_device_name *dn = feat;

    SAVE_BYTE(b, dn->initialized);
    SAVE_BYTE(b, dn->unsupported);
    SAVE_BYTE(b, dn->featIndex);
    SAVE_BYTE(b, dn->deviceType);
    SAVE_BYTE(b, dn->nameLength);
    SAVE_BYTES(b, dn->name, MAX_NAME_LEN);
}

static void load_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_device_name *dn = feat;

    dn->initialized = LOAD_BYTE(b);
    dn->unsupported = LOAD_BYTE(b);
    dn->featIndex = LOAD_BYTE(b);
    dn->deviceType = LOAD_BYTE(b);
    dn->nameLength = LOAD_BYTE(b);
    LOAD_BYTES(b, dn->name, MAX_NAME_LEN);
    dn->name[MAX_NAME_LEN] = '\0';
}

const struct hidpp_feat_vt hidpp_feat_device_name_vt = {
    .size = sizeof(struct hidpp_feat_device_name),
    .cacheSize = sizeof(struct hidpp_feat_device_name),
    .alignment = _Alignof(struct hidpp_feat_device_name),
    .collectCache = collect_cache,
    .saveCache = save_cache,
    .loadCache = load_cache,
    .clearCache = clear_cache,
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

char *hidpp_device_name(hidpp_device_t *dev) {
    struct hidpp_feat_device_name *dn;

    if (!dev || ensure_init(&dn, dev))
        return NULL;

    return dn->name;
}
