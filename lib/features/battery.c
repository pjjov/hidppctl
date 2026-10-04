/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"
#include "hidpp.h"

/** Section: HID++ feature documentation

    Battery status (id: 0x1000)
    Big endian integers are exchanged.

    -----------------------
    Fn 0 - get battery status
    -----------------------
    request:
    response:
        uint8_t dischargeLevel
        uint8_t dischargeNextLevel
        uint8_t status

    -----------------------
    Fn 1 - get battery capability
    -----------------------
    request:
    response:
        uint8_t levelCount
        uint8_t flags
        uint16_t nominalBatteryLife
        uint8_t criticalLevel
*/

#define MAX_NAME_LEN 256

#define DISABLED_OSD_FLAG 0x1
#define ENABLED_MILEAGE_FLAG 0x2
#define IS_RECHARGABLE_FLAG 0x4

struct hidpp_feat_battery {
    hidpp_bool_t initialized;
    hidpp_bool_t unsupported;

    uint8_t featIndex;
    struct hidpp_battery_info info;
    struct hidpp_battery_status status;
};

static const char *get_charge_name(int charge) {
    switch (charge) {
        /* clang-format off */
    case HIDPP_BATTERY_DISCHARGING: return "Discharging";
    case HIDPP_BATTERY_RECHARGING: return "Recharging";
    case HIDPP_BATTERY_CHARGE_FINAL: return "Charge in final state";
    case HIDPP_BATTERY_CHARGE_COMPLETE: return "Charge complete";
    case HIDPP_BATTERY_SUBOPTIMAL_RECHARGING: return "Recharging below optimal speed";
    case HIDPP_BATTERY_INVALID_TYPE: return "Invalid battery type";
    case HIDPP_BATTERY_THREMAL_ERROR: return "Thermal error";
    case HIDPP_BATTERY_CHARGE_ERROR: return "Other charging error";
    default: return "Unknown";
        /* clang-format on */
    }
}

static const char *get_level_name(float level) {
    if (level < 0 || level > 1)
        return "Unknown";
    if (level <= 0.1)
        return "Critical";
    if (level <= 0.3)
        return "Low";
    if (level <= 0.8)
        return "Good";
    return "Full";
}

static void unpack_flags(struct hidpp_battery_info *info) {
    info->disabledOSD = !!(info->flags & DISABLED_OSD_FLAG);
    info->enabledMileage = !!(info->flags & ENABLED_MILEAGE_FLAG);
    info->isRechargable = !!(info->flags & IS_RECHARGABLE_FLAG);
}

static void query_battery_info(
    hidpp_device_t *dev, uint8_t featIndex, struct hidpp_battery_info *out
) {
    hidpp_packet_t req, res;
    make_packet(&req, dev, featIndex, 1);

    if (hidpp_device_request(dev, &req, &res))
        return;

    out->levelCount = res.params[0];
    out->flags = res.params[1];
    out->nominalLife = HIDPP_WORD(res.params[2], res.params[3]);
    out->criticalLevel = res.params[3];
    unpack_flags(out);
    out->initialized = HIDPP_TRUE;
}

static int query_battery_status(
    hidpp_device_t *dev, uint8_t featIndex, struct hidpp_battery_status *out
) {
    hidpp_packet_t req, res;
    make_packet(&req, dev, featIndex, 0);

    int rc;

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    out->dischargeLevel = res.params[0];
    out->dischargeNextLevel = res.params[1];
    out->status = res.params[3];
    out->levelName = get_level_name((float)out->dischargeLevel / UINT8_MAX);
    out->statusName = get_charge_name(out->status);

    return HIDPP_OK;
}

static int ensure_init(struct hidpp_feat_battery **out, hidpp_device_t *dev) {
    struct hidpp_feat_battery *bat;

    if (!out || !dev)
        return HIDPP_EINVAL;

    bat = dev->features[HIDPP_FEAT_BATTERY];
    *out = bat;

    if (bat->initialized)
        return bat->unsupported ? HIDPP_ENOSYS : HIDPP_OK;

    bat->info.initialized = HIDPP_FALSE;
    int featIndex = hidpp_feature_index(dev, 0x1000);
    bat->featIndex = featIndex;
    bat->unsupported = featIndex <= 0;

    if (!bat->unsupported)
        query_battery_info(dev, bat->featIndex, &bat->info);

    bat->initialized = HIDPP_TRUE;

    return HIDPP_OK;
}

static void clear_cache(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_battery *bat = feat;
    bat->initialized = HIDPP_FALSE;
}

static void collect_cache(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_battery *bat;
    (void)feat;
    ensure_init(&bat, dev);
}

static size_t save_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_battery *bat = feat;
    uint8_t *start = b;

    SAVE_BYTE(b, bat->initialized);
    SAVE_BYTE(b, bat->unsupported);

    if (!bat->initialized || bat->unsupported)
        return b - start;

    SAVE_BYTE(b, bat->featIndex);
    SAVE_BYTE(b, bat->info.levelCount);
    SAVE_BYTE(b, bat->info.flags);
    SAVE_BYTE(b, bat->info.nominalLife);
    SAVE_BYTE(b, bat->info.criticalLevel);

    return b - start;
}

static size_t load_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_battery *bat = feat;
    uint8_t *start = b;

    bat->initialized = LOAD_BYTE(b);
    bat->unsupported = LOAD_BYTE(b);

    if (!bat->initialized || bat->unsupported)
        return b - start;

    bat->featIndex = LOAD_BYTE(b);
    bat->info.levelCount = LOAD_BYTE(b);
    bat->info.flags = LOAD_BYTE(b);
    bat->info.nominalLife = LOAD_BYTE(b);
    bat->info.criticalLevel = LOAD_BYTE(b);
    unpack_flags(&bat->info);

    return b - start;
}

const struct hidpp_feat_vt hidpp_feat_battery_vt = {
    .size = sizeof(struct hidpp_feat_battery),
    .alignment = _Alignof(struct hidpp_feat_battery),
    .maxCacheSize = sizeof(struct hidpp_feat_battery),
    .minCacheSize = 2 * sizeof(uint8_t),
    .collectCache = collect_cache,
    .saveCache = save_cache,
    .loadCache = load_cache,
    .clearCache = clear_cache,
};

struct hidpp_battery_info *hidpp_battery_info(hidpp_device_t *dev) {
    struct hidpp_feat_battery *bat;

    if (!dev || ensure_init(&bat, dev))
        return NULL;

    return &bat->info;
}

struct hidpp_battery_status *hidpp_battery_status(hidpp_device_t *dev) {
    struct hidpp_feat_battery *bat;
    struct hidpp_battery_status *status;

    if (!dev || ensure_init(&bat, dev))
        return NULL;

    status = &bat->status;

    if (query_battery_status(dev, bat->featIndex, status))
        return NULL;

    return status;
}