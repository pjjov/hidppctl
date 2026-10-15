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

#define DISABLED_OSD_FLAG 0x1
#define ENABLED_MILEAGE_FLAG 0x2
#define IS_RECHARGABLE_FLAG 0x4

struct hidpp_feat_battery {
    struct hidpp_feat_base base;
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

static const char *get_level_name(int percent) {
    if (percent < 0 || percent > 100)
        return "Unknown";

    if (percent <= 10)
        return "Critical";
    if (percent <= 30)
        return "Low";
    if (percent <= 80)
        return "Good";
    return "Full";
}

static void unpack_flags(struct hidpp_battery_info *info) {
    info->disabledOSD = !!(info->flags & DISABLED_OSD_FLAG);
    info->enabledMileage = !!(info->flags & ENABLED_MILEAGE_FLAG);
    info->isRechargable = !!(info->flags & IS_RECHARGABLE_FLAG);
}

static int query_battery_info(
    hidpp_device_t *dev, uint8_t featIndex, struct hidpp_battery_info *out
) {
    hidpp_packet_t req, res;
    make_packet(&req, dev, featIndex, 1);

    int rc;

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    out->levelCount = res.params[0];
    out->flags = res.params[1];
    out->nominalLife = HIDPP_WORD(res.params[2], res.params[3]);
    out->criticalLevel = res.params[4];
    unpack_flags(out);
    out->initialized = HIDPP_TRUE;
    return HIDPP_OK;
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
    out->status = res.params[2];
    out->levelName = get_level_name(out->dischargeLevel);
    out->statusName = get_charge_name(out->status);

    return HIDPP_OK;
}

static int query_info(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_battery *bat = feat;
    int rc = query_battery_info(dev, bat->base.featIndex, &bat->info);

    /* information the device refuses for good stays uninitialized */
    return rc && is_transient(dev, rc) ? rc : HIDPP_OK;
}

static int ensure_init(struct hidpp_feat_battery **out, hidpp_device_t *dev) {
    void *feat;
    int rc = hidpp__feature_init(dev, HIDPP_FEAT_BATTERY, &feat);
    *out = feat;
    return rc;
}

static size_t save_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_battery *bat = feat;
    uint8_t *start = b;
    (void)dev;

    SAVE_BYTE(b, bat->info.initialized);
    SAVE_BYTE(b, bat->info.levelCount);
    SAVE_BYTE(b, bat->info.flags);
    SAVE_WORD(b, bat->info.nominalLife);
    SAVE_BYTE(b, bat->info.criticalLevel);

    return b - start;
}

static size_t load_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_battery *bat = feat;
    uint8_t *start = b;
    (void)dev;

    bat->info.initialized = LOAD_BYTE(b);
    bat->info.levelCount = LOAD_BYTE(b);
    bat->info.flags = LOAD_BYTE(b);
    bat->info.nominalLife = LOAD_WORD(b);
    bat->info.criticalLevel = LOAD_BYTE(b);
    unpack_flags(&bat->info);

    return b - start;
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

    if (id != 0x1000 || func != 0)
        return HIDPP_ENOENT;

    e->type = HIDPP_EVENT_BATTERY;
    e->as.battery.chargeState = pkt->params[0];
    e->as.battery.batteryLevel = pkt->params[1];
    e->as.battery.chargeStatus = pkt->params[2];
    e->as.battery.externalPower = pkt->params[3];
    return HIDPP_OK;
}

const struct hidpp_feat_vt hidpp_feat_battery_vt = {
    FEAT_VT_SIZE(struct hidpp_feat_battery),
    .ids = { 0x1000 },
    .query = query_info,
    .saveCache = save_cache,
    .loadCache = load_cache,
    .parseEvent = parse_event,
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

    if (query_battery_status(dev, bat->base.featIndex, status))
        return NULL;

    return status;
}