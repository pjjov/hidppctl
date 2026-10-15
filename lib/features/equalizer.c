/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

/** Section: HID++ feature documentation

    Equalizer settings for audio devices (id: 0x8310, versions 0 to 2)
    Big endian integers are exchanged. Gains are signed dB values.

    -----------------------
    Fn 0 - get EQ info
    -----------------------
    request:
    response:
        uint8_t bandCount
        uint8_t dbRange
        uint8_t capabilities (version 2, 1 = gains, 2 = coefficients)
        int8_t dbMin (version 2, 0 means -dbRange)
        int8_t dbMax (version 2, 0 means +dbRange)

    -----------------------
    Fn 1 - get frequencies
    -----------------------
    request:
        uint8_t bandIndex
    response:
        uint8_t bandIndex
        uint16_t frequencies[7] (Hz, from bandIndex on)

    -----------------------
    Fn 2 - get frequency gains
    -----------------------
    request:
        uint8_t location (version 1, 0 = EEPROM, 1 = RAM)
    response:
        int8_t gains[bandCount]

    -----------------------
    Fn 3 - set frequency gains
    -----------------------
    request:
        uint8_t persistence (0 = RAM, 1 = RAM and EEPROM, 2 = EEPROM)
        int8_t gains[bandCount]
    response:
        (echoes the request)

    -----------------------
    Fn 4 - get mic noise reduction (version 2)
    -----------------------
    request:
    response:
        uint8_t enabled

    -----------------------
    Fn 5 - set mic noise reduction (version 2)
    -----------------------
    request:
        uint8_t enabled
    response:
*/

#define EQUALIZER_ID 0x8310

#define FREQUENCIES_PER_CALL 7
#define LOCATION_EEPROM 0
#define LOCATION_RAM 1

struct hidpp_feat_equalizer {
    struct hidpp_feat_base base;
    int8_t rawMin; /* as reported, 0 means -dbRange */
    int8_t rawMax;
    struct hidpp_eq_info info;
};

static void unpack_flags(struct hidpp_feat_equalizer *eq) {
    struct hidpp_eq_info *info = &eq->info;

    info->hasLocation = info->version >= 1;
    info->hasNoiseReduction = info->version >= 2;
    info->dbMin = eq->rawMin ? eq->rawMin : (int8_t)-info->dbRange;
    info->dbMax = eq->rawMax ? eq->rawMax : (int8_t)info->dbRange;
}

static int query_frequencies(
    hidpp_device_t *dev, struct hidpp_feat_equalizer *eq
) {
    struct hidpp_eq_info *info = &eq->info;
    hidpp_packet_t req, res;
    int rc;

    make_packet(&req, dev, eq->base.featIndex, 1);

    for (uint8_t start = 0; start < info->bandCount;
         start += FREQUENCIES_PER_CALL) {
        req.params[0] = start;

        if ((rc = hidpp_device_request(dev, &req, &res)))
            return rc;

        for (uint8_t i = 0;
             i < FREQUENCIES_PER_CALL && start + i < info->bandCount;
             i++)
            info->frequencies[start + i] = HIDPP_WORD(
                res.params[1 + 2 * i], res.params[2 + 2 * i]
            );
    }

    return HIDPP_OK;
}

static int query_info(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_equalizer *eq = feat;
    eq->info.version = eq->base.version;
    struct hidpp_eq_info *info = &eq->info;
    hidpp_packet_t req, res;
    make_packet(&req, dev, eq->base.featIndex, 0);

    int rc;

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return is_transient(dev, rc) ? rc : HIDPP_OK;

    info->bandCount = res.params[0];
    info->dbRange = res.params[1];

    if (info->version >= 2) {
        info->capabilities = res.params[2];
        eq->rawMin = (int8_t)res.params[3];
        eq->rawMax = (int8_t)res.params[4];
    }

    /* more bands do not fit into a request */
    if (info->bandCount > HIDPP_EQ_MAX_BANDS)
        info->bandCount = HIDPP_EQ_MAX_BANDS;

    if ((rc = query_frequencies(dev, eq)) && is_transient(dev, rc))
        return rc;

    unpack_flags(eq);
    info->initialized = HIDPP_TRUE;
    return HIDPP_OK;
}

static int ensure_init(struct hidpp_feat_equalizer **out, hidpp_device_t *dev) {
    void *feat;
    int rc = hidpp__feature_init(dev, HIDPP_FEAT_EQUALIZER, &feat);
    *out = feat;
    return rc;
}

static size_t save_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_equalizer *eq = feat;
    uint8_t *start = b;
    (void)dev;

    SAVE_BYTE(b, eq->info.initialized);
    SAVE_BYTE(b, eq->info.bandCount);
    SAVE_BYTE(b, eq->info.dbRange);
    SAVE_BYTE(b, eq->info.capabilities);
    SAVE_BYTE(b, (uint8_t)eq->rawMin);
    SAVE_BYTE(b, (uint8_t)eq->rawMax);

    for (uint8_t i = 0; i < eq->info.bandCount; i++)
        SAVE_WORD(b, eq->info.frequencies[i]);

    return b - start;
}

static size_t load_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_equalizer *eq = feat;
    uint8_t *start = b;
    (void)dev;

    eq->info.initialized = LOAD_BYTE(b);
    eq->info.version = eq->base.version;
    eq->info.bandCount = LOAD_BYTE(b);
    eq->info.dbRange = LOAD_BYTE(b);
    eq->info.capabilities = LOAD_BYTE(b);
    eq->rawMin = (int8_t)LOAD_BYTE(b);
    eq->rawMax = (int8_t)LOAD_BYTE(b);

    if (eq->info.bandCount > HIDPP_EQ_MAX_BANDS)
        eq->info.bandCount = HIDPP_EQ_MAX_BANDS;

    for (uint8_t i = 0; i < eq->info.bandCount; i++)
        eq->info.frequencies[i] = LOAD_WORD(b);

    unpack_flags(eq);

    return b - start;
}

const struct hidpp_feat_vt hidpp_feat_equalizer_vt = {
    FEAT_VT_SIZE(struct hidpp_feat_equalizer),
    .ids = { EQUALIZER_ID },
    .query = query_info,
    .saveCache = save_cache,
    .loadCache = load_cache,
};

static int ensure_noise_reduction(
    struct hidpp_feat_equalizer **eq, hidpp_device_t *dev
) {
    int rc;

    if ((rc = ensure_init(eq, dev)))
        return rc;

    return (*eq)->info.hasNoiseReduction ? HIDPP_OK : HIDPP_ENOSYS;
}

struct hidpp_eq_info *hidpp_eq_info(hidpp_device_t *dev) {
    struct hidpp_feat_equalizer *eq;

    if (!dev || ensure_init(&eq, dev))
        return NULL;

    return &eq->info;
}

int hidpp_eq_get(hidpp_device_t *dev, int active, int8_t *out, size_t max) {
    struct hidpp_feat_equalizer *eq;
    int rc;

    if (!out)
        return HIDPP_EINVAL;

    if ((rc = ensure_init(&eq, dev)))
        return rc;

    const struct hidpp_eq_info *info = &eq->info;

    if (max < info->bandCount)
        return HIDPP_EINVAL;

    /* version 0 always answers with the stored gains */
    if (active && !info->hasLocation)
        return HIDPP_ENOSYS;

    hidpp_packet_t req, res;
    make_packet(&req, dev, eq->base.featIndex, 2);
    req.params[0] = active ? LOCATION_RAM : LOCATION_EEPROM;

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    for (uint8_t i = 0; i < info->bandCount; i++)
        out[i] = (int8_t)res.params[i];

    return info->bandCount;
}

int hidpp_eq_set(
    hidpp_device_t *dev, const int8_t *gains, size_t count, uint8_t persistence
) {
    struct hidpp_feat_equalizer *eq;
    int rc;

    if (!gains)
        return HIDPP_EINVAL;

    if ((rc = ensure_init(&eq, dev)))
        return rc;

    const struct hidpp_eq_info *info = &eq->info;

    if (count != info->bandCount || persistence > HIDPP_EQ_NON_VOLATILE)
        return HIDPP_EINVAL;

    for (size_t i = 0; i < count; i++)
        if (gains[i] < info->dbMin || gains[i] > info->dbMax)
            return HIDPP_EINVAL;

    hidpp_packet_t req, res;
    make_packet(&req, dev, eq->base.featIndex, 3);
    req.kind = HIDPP_KIND_LONG;
    req.params[0] = persistence;

    for (size_t i = 0; i < count; i++)
        req.params[1 + i] = (uint8_t)gains[i];

    return hidpp_device_request(dev, &req, &res);
}

int hidpp_eq_noise_reduction(hidpp_device_t *dev) {
    struct hidpp_feat_equalizer *eq;
    int rc;

    if ((rc = ensure_noise_reduction(&eq, dev)))
        return rc;

    hidpp_packet_t req, res;
    make_packet(&req, dev, eq->base.featIndex, 4);

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    return res.params[0] & 1;
}

int hidpp_eq_set_noise_reduction(hidpp_device_t *dev, int enabled) {
    struct hidpp_feat_equalizer *eq;
    int rc;

    if ((rc = ensure_noise_reduction(&eq, dev)))
        return rc;

    hidpp_packet_t req, res;
    make_packet(&req, dev, eq->base.featIndex, 5);
    req.params[0] = enabled ? 1 : 0;

    return hidpp_device_request(dev, &req, &res);
}
