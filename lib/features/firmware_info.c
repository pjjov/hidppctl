/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

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
#define SERIAL_NUM_LEN 12

#define HAS_SERIAL_NUMBER_FLAG 0x1
#define SUPPORTS_USB_FLAG 0x8
#define SUPPORTS_EQUAD_FLAG 0x4
#define SUPPORTS_BTLE_FLAG 0x2
#define SUPPORTS_BT_FLAG 0x1

struct hidpp_feat_firmware_info {
    hidpp_bool_t initialized;
    hidpp_bool_t unsupported;
    hidpp_bool_t hasSerialNumber;
    uint8_t featIndex;
    uint8_t entityCount;

    char serialNumber[SERIAL_NUM_LEN + 1];
    struct hidpp_firmware_info info;
    struct hidpp_firmware_entity entities[MAX_ENTITY_COUNT];
};

static const char *get_type_name(int type) {
    switch (type) {
        /* clang-format off */
    case HIDPP_FIRMWARE_MAIN_APP:            return "Main application";
    case HIDPP_FIRMWARE_BOOT_LOADER:         return "Boot loader";
    case HIDPP_FIRMWARE_HARDWARE:            return "Hardware";
    case HIDPP_FIRMWARE_TOUCHPAD:            return "Touchpad";
    case HIDPP_FIRMWARE_OPTICAL_SENSOR:      return "Optical sensor";
    case HIDPP_FIRMWARE_SOFTDEVICE:          return "Softdevice";
    case HIDPP_FIRMWARE_RF_COMPANION_MCU:    return "RF Companion MCU";
    case HIDPP_FIRMWARE_FACTORY_APPLICATION: return "Factory application";
    case HIDPP_FIRMWARE_RGB_CUSTOM_EFFECT:   return "RGB Custom Effect";
    case HIDPP_FIRMWARE_MOTOR_DRIVE:         return "Motor drive";
    default:                                 return "Other";
        /* clang-format on */
    }
}

static void unpack_flags(struct hidpp_firmware_info *info) {
    info->hasSerialNumber = !!(info->capabilities & HAS_SERIAL_NUMBER_FLAG);
    info->supportsUSB = !!(info->transportFlags & SUPPORTS_USB_FLAG);
    info->supportsEQuad = !!(info->transportFlags & SUPPORTS_EQUAD_FLAG);
    info->supportsBTLE = !!(info->transportFlags & SUPPORTS_BTLE_FLAG);
    info->supportsBT = !!(info->transportFlags & SUPPORTS_BT_FLAG);
}

static int query_firmware_info(
    hidpp_device_t *dev, uint8_t featIndex, struct hidpp_firmware_info *out
) {
    hidpp_packet_t req, res;
    make_packet(&req, dev, featIndex, 0);

    int rc;

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    out->entityCount = res.params[0];
    out->transportFlags = HIDPP_WORD(res.params[5], res.params[6]);
    out->extendedModelId = res.params[13];
    out->capabilities = res.params[14];
    out->reserved = res.params[15];

    out->unitId = HIDPP_DWORD(
        res.params[1], res.params[2], res.params[3], res.params[4]
    );

    out->modelId = HIDPP_QWORD(
        0,
        0,
        res.params[7],
        res.params[8],
        res.params[9],
        res.params[10],
        res.params[11],
        res.params[12]
    );

    unpack_flags(out);
    out->initialized = HIDPP_TRUE;

    return HIDPP_OK;
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
    out->firmwareNumber = HIDPP_DWORD(
        res.params[1], res.params[2], res.params[3], res.params[4]
    );
    out->revision = res.params[5];
    out->buildNumber = HIDPP_WORD(res.params[6], res.params[7]);
    out->reserved = res.params[8];
    memcpy(out->specificInfo, &res.params[9], 7);
    out->typeName = get_type_name(out->type);
    out->initialized = HIDPP_TRUE;

    return HIDPP_OK;
}

static int query_serial_number(
    hidpp_device_t *dev, uint8_t featIndex, char *out
) {
    hidpp_packet_t req, res;
    make_packet(&req, dev, featIndex, 2);

    int rc;

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    memcpy(out, res.params, SERIAL_NUM_LEN);
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

    memset(fw->serialNumber, 0, sizeof(fw->serialNumber));
    memset(&fw->info, 0, sizeof(fw->info));
    memset(fw->entities, 0, sizeof(fw->entities));
    int featIndex = hidpp_feature_index(dev, 0x0003);
    fw->featIndex = featIndex;
    fw->hasSerialNumber = HIDPP_FALSE;
    fw->unsupported = featIndex <= 0;

    if (!fw->unsupported) {
        int rc = query_firmware_info(dev, fw->featIndex, &fw->info);
        fw->entityCount = rc == 0 ? fw->info.entityCount : 0;
        fw->hasSerialNumber = fw->info.hasSerialNumber;
    }

    if (fw->hasSerialNumber)
        query_serial_number(dev, fw->featIndex, fw->serialNumber);

    fw->initialized = HIDPP_TRUE;

    return HIDPP_OK;
}

static void clear_cache(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_firmware_info *fw = feat;
    fw->initialized = HIDPP_FALSE;
}

static void collect_cache(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_firmware_info *fw;
    (void)feat;
    ensure_init(&fw, dev);
}

static size_t save_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_firmware_info *fw = feat;
    uint8_t *start = b;

    SAVE_BYTE(b, fw->initialized);
    SAVE_BYTE(b, fw->unsupported);

    if (!fw->initialized || fw->unsupported)
        return b - start;

    SAVE_BYTE(b, fw->featIndex);
    SAVE_BYTE(b, fw->entityCount);
    SAVE_BYTE(b, fw->hasSerialNumber);

    SAVE_BYTES(b, fw->serialNumber, SERIAL_NUM_LEN);

    SAVE_BYTE(b, fw->info.entityCount);
    SAVE_DWORD(b, fw->info.unitId);
    SAVE_WORD(b, fw->info.transportFlags);
    SAVE_QWORD(b, fw->info.modelId);
    SAVE_BYTE(b, fw->info.extendedModelId);
    SAVE_BYTE(b, fw->info.capabilities);
    SAVE_BYTE(b, fw->info.reserved);

    for (size_t i = 0; i < fw->entityCount; i++) {
        struct hidpp_firmware_entity *info = &fw->entities[i];
        SAVE_BYTE(b, info->initialized);
        SAVE_BYTE(b, info->id);
        SAVE_BYTE(b, info->type);
        SAVE_DWORD(b, info->firmwareNumber);
        SAVE_WORD(b, info->revision);
        SAVE_WORD(b, info->buildNumber);
        SAVE_BYTE(b, info->reserved);
        SAVE_BYTES(b, (char *)info->specificInfo, sizeof(info->specificInfo));
    }

    return b - start;
}

static size_t load_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_firmware_info *fw = feat;
    uint8_t *start = b;

    fw->initialized = LOAD_BYTE(b);
    fw->unsupported = LOAD_BYTE(b);

    if (!fw->initialized || fw->unsupported)
        return b - start;

    fw->featIndex = LOAD_BYTE(b);
    fw->entityCount = LOAD_BYTE(b);
    fw->hasSerialNumber = LOAD_BYTE(b);

    LOAD_BYTES(b, fw->serialNumber, SERIAL_NUM_LEN);
    fw->serialNumber[SERIAL_NUM_LEN] = '\0';

    fw->info.entityCount = LOAD_BYTE(b);
    fw->info.unitId = LOAD_DWORD(b);
    fw->info.transportFlags = LOAD_WORD(b);
    fw->info.modelId = LOAD_QWORD(b);
    fw->info.extendedModelId = LOAD_BYTE(b);
    fw->info.capabilities = LOAD_BYTE(b);
    fw->info.reserved = LOAD_BYTE(b);
    unpack_flags(&fw->info);

    for (size_t i = 0; i < fw->entityCount; i++) {
        struct hidpp_firmware_entity *info = &fw->entities[i];
        info->initialized = LOAD_BYTE(b);
        info->id = LOAD_BYTE(b);
        info->type = LOAD_BYTE(b);
        info->firmwareNumber = LOAD_DWORD(b);
        info->revision = LOAD_WORD(b);
        info->buildNumber = LOAD_WORD(b);
        info->reserved = LOAD_BYTE(b);
        LOAD_BYTES(b, (char *)info->specificInfo, sizeof(info->specificInfo));

        if (fw->initialized && !fw->unsupported && info->initialized) {
            info->typeName = get_type_name(info->type);
        } else {
            info->typeName = NULL;
        }
    }

    return b - start;
}

const struct hidpp_feat_vt hidpp_feat_firmware_info_vt = {
    .size = sizeof(struct hidpp_feat_firmware_info),
    .alignment = _Alignof(struct hidpp_feat_firmware_info),
    .maxCacheSize = sizeof(struct hidpp_feat_firmware_info),
    .minCacheSize = 2 * sizeof(uint8_t),
    .collectCache = collect_cache,
    .saveCache = save_cache,
    .loadCache = load_cache,
    .clearCache = clear_cache,
};

struct hidpp_firmware_info *hidpp_firmware_info(hidpp_device_t *dev) {
    struct hidpp_feat_firmware_info *fw;

    if (!dev || ensure_init(&fw, dev))
        return NULL;

    return &fw->info;
}

struct hidpp_firmware_entity *hidpp_firmware_entity(
    hidpp_device_t *dev, uint8_t id
) {
    struct hidpp_feat_firmware_info *fw;
    struct hidpp_firmware_entity *info;

    if (!dev || ensure_init(&fw, dev))
        return NULL;

    info = &fw->entities[id];

    if (!info->initialized && query_entity_info(info, dev, fw->featIndex, id))
        return NULL;

    return info;
}

const char *hidpp_serial_number(hidpp_device_t *dev) {
    struct hidpp_feat_firmware_info *fw;

    if (!dev || ensure_init(&fw, dev))
        return NULL;

    return fw->hasSerialNumber ? fw->serialNumber : NULL;
}