/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"
#include "hidpp.h"

/** Section: HID++ feature documentation

    Firmware info (id: 0x0003)
    Big endian integers are exchanged.

    -----------------------
    Fn 0 - get entity count
    -----------------------
    request:
    response:
        uint8_t entityCount

    -----------------------
    Fn 1 - get entity info
    -----------------------
    request:
        uint8_t entityId
    response:
        uint8_t type (lower nibble)
        uint24_t prefix
        uint16_t version
        uint16_t buildNumber
        uint8_t unknown
        uint8_t[7] specificInfo
*/

#define MAX_ENTITY_COUNT 256

struct hidpp_feat_firmware_info {
    hidpp_bool_t initialized;
    hidpp_bool_t unsupported;
    uint8_t featIndex;
    uint8_t entityCount;

    struct hidpp_firmware_entity entities[MAX_ENTITY_COUNT];
};

static inline uint64_t build_uint64_t(uint8_t *a, size_t count) {
    uint64_t out = 0;
    for (size_t i = 0; i < count; i++)
        out = (out << 8) | (uint64_t)a[i];
    return out;
}

static const char *get_type_name(int type) {
    switch (type) {
    case HIDPP_FIRMWARE_MAIN_APP:
        return "Main application";
    case HIDPP_FIRMWARE_BOOT_LOADER:
        return "Boot loader";
    case HIDPP_FIRMWARE_HARDWARE:
        return "Hardware";
    default:
        return "Other";
    }
}

static int query_entity_count(hidpp_device_t *dev, uint8_t featIndex) {
    hidpp_packet_t req, res;
    make_packet(&req, dev, featIndex, 0);

    if (hidpp_device_request(dev, &req, &res))
        return 0;

    return res.params[0];
}

static int query_entity_info(
    struct hidpp_firmware_entity *out,
    hidpp_device_t *dev,
    uint8_t featIndex,
    uint8_t entityId
) {
    hidpp_packet_t req, res;
    make_packet(&req, dev, featIndex, 1);
    req.params[0] = entityId;

    int rc;

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    out->id = entityId;
    out->type = res.params[0];
    out->prefix = HIDPP_DWORD(0, res.params[1], res.params[2], res.params[3]);
    out->version = HIDPP_WORD(res.params[4], res.params[5]);
    out->buildNumber = HIDPP_WORD(res.params[6], res.params[7]);
    out->reserved = res.params[8];
    memcpy(out->specificInfo, &res.params[9], 7);
    out->typeName = get_type_name(out->type);
    out->initialized = HIDPP_TRUE;

    return HIDPP_OK;
}

static int ensure_init(
    struct hidpp_feat_firmware_info **out, hidpp_device_t *dev
) {
    struct hidpp_feat_firmware_info *fw;

    if (!out || !dev)
        return HIDPP_EINVAL;

    fw = dev->features[HIDPP_FEAT_FIRMWARE_INFO];
    *out = fw;

    if (fw->initialized)
        return fw->unsupported ? HIDPP_ENOSYS : HIDPP_OK;

    memset(fw->entities, 0, sizeof(fw->entities));
    int featIndex = hidpp_feature_index(dev, 0x0003);
    fw->featIndex = featIndex;
    fw->unsupported = featIndex <= 0;

    if (!fw->unsupported)
        fw->entityCount = query_entity_count(dev, fw->featIndex);

    fw->initialized = HIDPP_TRUE;

    return HIDPP_OK;
}

static void clear_cache(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_firmware_info *fw = feat;
    fw->initialized = HIDPP_FALSE;
}

const struct hidpp_feat_vt hidpp_feat_firmware_info_vt = {
    .size = sizeof(struct hidpp_feat_firmware_info),
    .alignment = _Alignof(struct hidpp_feat_firmware_info),
    .clearCache = clear_cache,
};

struct hidpp_firmware_entity *hidpp_firmware_entity(
    hidpp_device_t *dev, uint8_t id, uint8_t *count
) {
    struct hidpp_feat_firmware_info *fw;
    struct hidpp_firmware_entity *info;

    if (!dev || ensure_init(&fw, dev))
        return NULL;

    info = &fw->entities[id];

    if (!info->initialized && query_entity_info(info, dev, fw->featIndex, id))
        return NULL;

    if (count)
        *count = fw->entityCount;

    return info;
}

uint64_t hidpp_cache_id(hidpp_device_t *dev) {
    if (!dev)
        return 0;

    hidpp_packet_t req, res;

    /* Cached feature index is avoided */
    make_packet(&req, dev, 0, 0);
    req.params[0] = HIDPP_MSB(0x0003);
    req.params[1] = HIDPP_LSB(0x0003);

    if (hidpp_device_request(dev, &req, &res))
        return 0;

    uint8_t featIndex = res.params[0];
    make_packet(&req, dev, featIndex, 1);
    req.params[0] = 0;

    if (hidpp_device_request(dev, &req, &res))
        return 0;

    /* 0x00 + Prefix + Version + Build number */
    uint64_t cacheId = build_uint64_t(&res.params[1], 7);
    return cacheId;
}