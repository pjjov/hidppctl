/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

/** Section: HID++ feature documentation

    Illumination light (id: 0x1990, versions 0 and 1)
    Big endian integers are exchanged.

    -----------------------
    Fn 0 - get illumination
    -----------------------
    request:
    response:
        uint8_t state (bit 0)

    -----------------------
    Fn 1 - set illumination
    -----------------------
    request:
        uint8_t state (bit 0)
    response:

    Brightness (lumen) and color temperature (kelvin) share the same layouts.
    Functions 2-6 are used for brightness and 7-11 for color temperature:

    -----------------------
    Fn 2 / 7 - get control info
    -----------------------
    request:
    response:
        uint8_t capabilities
        - bit 0 - hasEvents
        - bit 1 - hasLinearLevels
        - bit 2 - hasNonLinearLevels
        - bit 3 - hasDynamicMaximum (version 1, brightness only)
        uint16_t min
        uint16_t max
        uint16_t res
        uint8_t maxLevels (low nibble)

    -----------------------
    Fn 3 / 8 - get value
    -----------------------
    request:
    response:
        uint16_t value

    -----------------------
    Fn 4 / 9 - set value
    -----------------------
    request:
        uint16_t value
    response:

    -----------------------
    Fn 5 / 10 - get levels
    -----------------------
    request:
        uint8_t startIndex (high nibble)
    response:
        uint8_t flags
        - bit 0 - linear
        - bits 5-7 - validCount
        uint8_t targetLevels
        - bits 4-7 - startIndex
        - bits 0-3 - levelCount
        uint16_t values[7] (non-linear)
        or uint16_t min, max, step (linear)

    -----------------------
    Fn 6 / 11 - set levels
    -----------------------
    request:
        uint8_t flags
        - bit 0 - linear
        - bit 1 - reset
        - bits 5-7 - validCount
        uint8_t targetLevels
        uint16_t values[7] (non-linear)
        or uint16_t min, max, step (linear)
    response:

    -----------------------
    Fn 12 - get effective maximum brightness (version 1)
    -----------------------
    request:
    response:
        uint16_t effectiveMax

    -----------------------
    Events
    -----------------------
    0 - illumination changed:       uint8_t state
    1 - brightness changed:         uint16_t value
    2 - color temperature changed:  uint16_t value
    3 - effective maximum changed:  uint16_t value
    4 - brightness clamped:         (layout is not documented)
*/

#define ILLUMINATION_ID 0x1990

#define CONTROL_COUNT 2
#define LEVELS_PER_CALL 7
#define EFFECTIVE_MAX_FUNC 12

/* clang-format off */

#define CAP_EVENTS_FLAG          0x01
#define CAP_LINEAR_FLAG          0x02
#define CAP_NON_LINEAR_FLAG      0x04
#define CAP_DYNAMIC_MAX_FLAG     0x08

#define LEVELS_LINEAR_FLAG       0x01
#define LEVELS_RESET_FLAG        0x02
#define LEVELS_COUNT_SHIFT       5

#define MAX_LEVELS_MASK          0x0F

/* clang-format on */

/* first function of each control: info, get, set, getLevels, setLevels */
static const uint8_t control_functions[CONTROL_COUNT] = { 2, 7 };

enum {
    FUNC_INFO,
    FUNC_GET,
    FUNC_SET,
    FUNC_GET_LEVELS,
    FUNC_SET_LEVELS,
};

struct hidpp_feat_illumination {
    hidpp_bool_t initialized;
    hidpp_bool_t unsupported;

    uint8_t featIndex;
    uint8_t version;
    struct hidpp_illumination_info controls[CONTROL_COUNT];
};

static void unpack_flags(
    struct hidpp_feat_illumination *ill,
    struct hidpp_illumination_info *info,
    int control
) {
    info->hasEvents = !!(info->flags & CAP_EVENTS_FLAG);
    info->hasLinearLevels = !!(info->flags & CAP_LINEAR_FLAG);
    info->hasNonLinearLevels = !!(info->flags & CAP_NON_LINEAR_FLAG);
    info->hasDynamicMaximum = ill->version >= 1
        && control == HIDPP_ILLUMINATION_BRIGHTNESS
        && !!(info->flags & CAP_DYNAMIC_MAX_FLAG);
    info->isSettable = info->min != info->max;
}

static int query_control_info(
    hidpp_device_t *dev, struct hidpp_feat_illumination *ill, int control
) {
    struct hidpp_illumination_info *info = &ill->controls[control];
    hidpp_packet_t req, res;

    make_packet(
        &req, dev, ill->featIndex, control_functions[control] + FUNC_INFO
    );

    int rc;

    /*  An error report means that the device has no such control. */
    if ((rc = hidpp_device_request(dev, &req, &res)))
        return is_transient(dev, rc) ? rc : HIDPP_OK;

    info->flags = res.params[0];
    info->min = HIDPP_WORD(res.params[1], res.params[2]);
    info->max = HIDPP_WORD(res.params[3], res.params[4]);
    info->res = HIDPP_WORD(res.params[5], res.params[6]);
    info->maxLevels = res.params[7] & MAX_LEVELS_MASK;
    unpack_flags(ill, info, control);
    info->initialized = HIDPP_TRUE;
    return HIDPP_OK;
}

static int ensure_init(
    struct hidpp_feat_illumination **out, hidpp_device_t *dev
) {
    struct hidpp_feat_illumination *ill;

    if (!out || !dev)
        return HIDPP_EINVAL;

    ill = dev->features[HIDPP_FEAT_ILLUMINATION];
    *out = ill;

    if (ill->initialized)
        return ill->unsupported ? HIDPP_ENOSYS : HIDPP_OK;

    memset(ill, 0, sizeof(*ill));
    int featIndex = feature_lookup(dev, ILLUMINATION_ID);

    if (featIndex < 0)
        return featIndex;

    ill->featIndex = (uint8_t)featIndex;
    ill->unsupported = featIndex == 0;

    if (!ill->unsupported) {
        struct hidpp_feature_info *feat = hidpp_feature_info(
            dev, ILLUMINATION_ID
        );
        ill->version = feat ? feat->version : 0;

        for (int i = 0; i < CONTROL_COUNT; i++) {
            int rc = query_control_info(dev, ill, i);

            if (rc)
                return rc;
        }
    }

    ill->initialized = HIDPP_TRUE;

    return ill->unsupported ? HIDPP_ENOSYS : HIDPP_OK;
}

static void clear_cache(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_illumination *ill = feat;
    (void)dev;
    ill->initialized = HIDPP_FALSE;
}

static void collect_cache(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_illumination *ill;
    (void)feat;
    ensure_init(&ill, dev);
}

static size_t save_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_illumination *ill = feat;
    uint8_t *start = b;
    (void)dev;

    SAVE_BYTE(b, ill->initialized);
    SAVE_BYTE(b, ill->unsupported);

    if (!ill->initialized || ill->unsupported)
        return b - start;

    SAVE_BYTE(b, ill->featIndex);
    SAVE_BYTE(b, ill->version);

    for (int i = 0; i < CONTROL_COUNT; i++) {
        struct hidpp_illumination_info *info = &ill->controls[i];
        SAVE_BYTE(b, info->initialized);
        SAVE_BYTE(b, info->flags);
        SAVE_WORD(b, info->min);
        SAVE_WORD(b, info->max);
        SAVE_WORD(b, info->res);
        SAVE_BYTE(b, info->maxLevels);
    }

    return b - start;
}

static size_t load_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_illumination *ill = feat;
    uint8_t *start = b;
    (void)dev;

    ill->initialized = LOAD_BYTE(b);
    ill->unsupported = LOAD_BYTE(b);

    if (!ill->initialized || ill->unsupported)
        return b - start;

    ill->featIndex = LOAD_BYTE(b);
    ill->version = LOAD_BYTE(b);

    for (int i = 0; i < CONTROL_COUNT; i++) {
        struct hidpp_illumination_info *info = &ill->controls[i];
        info->initialized = LOAD_BYTE(b);
        info->flags = LOAD_BYTE(b);
        info->min = LOAD_WORD(b);
        info->max = LOAD_WORD(b);
        info->res = LOAD_WORD(b);
        info->maxLevels = LOAD_BYTE(b) & MAX_LEVELS_MASK;
        unpack_flags(ill, info, i);
    }

    return b - start;
}

const struct hidpp_feat_vt hidpp_feat_illumination_vt = {
    .size = sizeof(struct hidpp_feat_illumination),
    .alignment = _Alignof(struct hidpp_feat_illumination),
    .maxCacheSize = sizeof(struct hidpp_feat_illumination),
    .minCacheSize = 2 * sizeof(uint8_t),
    .collectCache = collect_cache,
    .saveCache = save_cache,
    .loadCache = load_cache,
    .clearCache = clear_cache,
};

/* Looks the feature up and checks that `control` exists on the device. */
static int get_control(
    hidpp_device_t *dev,
    int control,
    struct hidpp_feat_illumination **ill,
    struct hidpp_illumination_info **info
) {
    int rc;

    if (control < 0 || control >= CONTROL_COUNT)
        return HIDPP_EINVAL;

    if ((rc = ensure_init(ill, dev)))
        return rc;

    *info = &(*ill)->controls[control];
    return (*info)->initialized ? HIDPP_OK : HIDPP_ENOSYS;
}

static int check_value(
    const struct hidpp_illumination_info *info, uint16_t value
) {
    if (value < info->min || value > info->max)
        return HIDPP_EINVAL;

    if (info->res && (value - info->min) % info->res)
        return HIDPP_EINVAL;

    return HIDPP_OK;
}

static int has_levels(const struct hidpp_illumination_info *info) {
    return info->hasLinearLevels || info->hasNonLinearLevels;
}

struct hidpp_illumination_info *hidpp_illumination_info(
    hidpp_device_t *dev, int control
) {
    struct hidpp_feat_illumination *ill;
    struct hidpp_illumination_info *info;

    if (!dev || get_control(dev, control, &ill, &info))
        return NULL;

    return info;
}

int hidpp_illumination_get(hidpp_device_t *dev) {
    struct hidpp_feat_illumination *ill;
    int rc;

    if ((rc = ensure_init(&ill, dev)))
        return rc;

    hidpp_packet_t req, res;
    make_packet(&req, dev, ill->featIndex, 0);

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    return res.params[0] & 1;
}

int hidpp_illumination_set(hidpp_device_t *dev, int state) {
    struct hidpp_feat_illumination *ill;
    int rc;

    if ((rc = ensure_init(&ill, dev)))
        return rc;

    hidpp_packet_t req, res;
    make_packet(&req, dev, ill->featIndex, 1);
    req.params[0] = state ? 1 : 0;

    return hidpp_device_request(dev, &req, &res);
}

int hidpp_illumination_value(hidpp_device_t *dev, int control) {
    struct hidpp_feat_illumination *ill;
    struct hidpp_illumination_info *info;
    int rc;

    if ((rc = get_control(dev, control, &ill, &info)))
        return rc;

    hidpp_packet_t req, res;
    make_packet(
        &req, dev, ill->featIndex, control_functions[control] + FUNC_GET
    );

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    return HIDPP_WORD(res.params[0], res.params[1]);
}

int hidpp_illumination_set_value(
    hidpp_device_t *dev, int control, uint16_t value
) {
    struct hidpp_feat_illumination *ill;
    struct hidpp_illumination_info *info;
    int rc;

    if ((rc = get_control(dev, control, &ill, &info)))
        return rc;

    /* devices with a single value do not implement the setter */
    if (!info->isSettable)
        return HIDPP_ENOSYS;

    if ((rc = check_value(info, value)))
        return rc;

    hidpp_packet_t req, res;
    make_packet(
        &req, dev, ill->featIndex, control_functions[control] + FUNC_SET
    );
    req.params[0] = HIDPP_MSB(value);
    req.params[1] = HIDPP_LSB(value);

    return hidpp_device_request(dev, &req, &res);
}

static int query_levels_chunk(
    hidpp_device_t *dev,
    struct hidpp_feat_illumination *ill,
    int control,
    uint8_t startIndex,
    hidpp_packet_t *res
) {
    hidpp_packet_t req;
    make_packet(
        &req, dev, ill->featIndex, control_functions[control] + FUNC_GET_LEVELS
    );
    req.params[0] = (uint8_t)(startIndex << 4);

    return hidpp_device_request(dev, &req, res);
}

int hidpp_illumination_levels(
    hidpp_device_t *dev, int control, struct hidpp_illumination_levels *out
) {
    struct hidpp_feat_illumination *ill;
    struct hidpp_illumination_info *info;
    hidpp_packet_t res;
    int rc;

    if (!out)
        return HIDPP_EINVAL;

    if ((rc = get_control(dev, control, &ill, &info)))
        return rc;

    if (!has_levels(info))
        return HIDPP_ENOSYS;

    if ((rc = query_levels_chunk(dev, ill, control, 0, &res)))
        return rc;

    memset(out, 0, sizeof(*out));
    out->linear = !!(res.params[0] & LEVELS_LINEAR_FLAG);

    if (out->linear) {
        out->min = HIDPP_WORD(res.params[2], res.params[3]);
        out->max = HIDPP_WORD(res.params[4], res.params[5]);
        out->step = HIDPP_WORD(res.params[6], res.params[7]);

        if (out->step && out->max >= out->min) {
            unsigned count = (unsigned)(out->max - out->min) / out->step + 1;
            out->count = count > UINT8_MAX ? UINT8_MAX : (uint8_t)count;
        }

        return HIDPP_OK;
    }

    out->count = res.params[1] & 0x0F;

    if (out->count > HIDPP_ILLUMINATION_MAX_LEVELS)
        return HIDPP_EIO;

    uint8_t index = 0;

    while (index < out->count) {
        if (index > 0
            && (rc = query_levels_chunk(dev, ill, control, index, &res)))
            return rc;

        uint8_t valid = res.params[0] >> LEVELS_COUNT_SHIFT;

        if (valid == 0 || valid > LEVELS_PER_CALL
            || (res.params[1] >> 4) != index)
            return HIDPP_EIO;

        for (uint8_t i = 0; i < valid && index < out->count; i++, index++)
            out->values[index] = HIDPP_WORD(
                res.params[2 + 2 * i], res.params[3 + 2 * i]
            );
    }

    return HIDPP_OK;
}

static int check_levels(
    const struct hidpp_illumination_info *info,
    const struct hidpp_illumination_levels *levels
) {
    if (levels->linear) {
        if (!info->hasLinearLevels || levels->step == 0
            || levels->min > levels->max
            || (levels->max - levels->min) % levels->step)
            return HIDPP_EINVAL;

        if (check_value(info, levels->min) || check_value(info, levels->max))
            return HIDPP_EINVAL;

        return HIDPP_OK;
    }

    if (!info->hasNonLinearLevels || info->maxLevels == 0)
        return HIDPP_EINVAL;

    if (levels->count == 0 || levels->count > info->maxLevels
        || levels->count > HIDPP_ILLUMINATION_MAX_LEVELS)
        return HIDPP_EINVAL;

    for (uint8_t i = 0; i < levels->count; i++) {
        if (check_value(info, levels->values[i]))
            return HIDPP_EINVAL;

        if (i > 0 && levels->values[i] < levels->values[i - 1])
            return HIDPP_EINVAL; /* levels never decrease */
    }

    return HIDPP_OK;
}

int hidpp_illumination_set_levels(
    hidpp_device_t *dev,
    int control,
    const struct hidpp_illumination_levels *levels
) {
    struct hidpp_feat_illumination *ill;
    struct hidpp_illumination_info *info;
    int rc;

    if (!levels)
        return HIDPP_EINVAL;

    if ((rc = get_control(dev, control, &ill, &info)))
        return rc;

    if (!has_levels(info))
        return HIDPP_ENOSYS;

    if ((rc = check_levels(info, levels)))
        return rc;

    hidpp_packet_t req, res;
    uint8_t func = control_functions[control] + FUNC_SET_LEVELS;

    if (levels->linear) {
        make_packet(&req, dev, ill->featIndex, func);
        req.kind = HIDPP_KIND_LONG;
        req.params[0] = LEVELS_LINEAR_FLAG;
        req.params[2] = HIDPP_MSB(levels->min);
        req.params[3] = HIDPP_LSB(levels->min);
        req.params[4] = HIDPP_MSB(levels->max);
        req.params[5] = HIDPP_LSB(levels->max);
        req.params[6] = HIDPP_MSB(levels->step);
        req.params[7] = HIDPP_LSB(levels->step);

        return hidpp_device_request(dev, &req, &res);
    }

    /*  Seven values fit into a request. Growing the level count together
        with the values keeps the device from seeing unset levels. */
    for (uint8_t start = 0; start < levels->count; start += LEVELS_PER_CALL) {
        uint8_t valid = levels->count - start;

        if (valid > LEVELS_PER_CALL)
            valid = LEVELS_PER_CALL;

        make_packet(&req, dev, ill->featIndex, func);
        req.kind = HIDPP_KIND_LONG;
        req.params[0] = (uint8_t)(valid << LEVELS_COUNT_SHIFT);
        req.params[1] = (uint8_t)((start << 4) | (start + valid));

        for (uint8_t i = 0; i < valid; i++) {
            req.params[2 + 2 * i] = HIDPP_MSB(levels->values[start + i]);
            req.params[3 + 2 * i] = HIDPP_LSB(levels->values[start + i]);
        }

        if ((rc = hidpp_device_request(dev, &req, &res)))
            return rc;
    }

    return HIDPP_OK;
}

int hidpp_illumination_reset_levels(hidpp_device_t *dev, int control) {
    struct hidpp_feat_illumination *ill;
    struct hidpp_illumination_info *info;
    int rc;

    if ((rc = get_control(dev, control, &ill, &info)))
        return rc;

    if (!has_levels(info))
        return HIDPP_ENOSYS;

    hidpp_packet_t req, res;
    make_packet(
        &req, dev, ill->featIndex, control_functions[control] + FUNC_SET_LEVELS
    );
    req.kind = HIDPP_KIND_LONG;
    req.params[0] = LEVELS_RESET_FLAG;

    return hidpp_device_request(dev, &req, &res);
}

int hidpp_illumination_effective_max(hidpp_device_t *dev) {
    struct hidpp_feat_illumination *ill;
    struct hidpp_illumination_info *info;
    int rc;

    if ((rc = get_control(dev, HIDPP_ILLUMINATION_BRIGHTNESS, &ill, &info)))
        return rc;

    if (!info->hasDynamicMaximum)
        return HIDPP_ENOSYS;

    hidpp_packet_t req, res;
    make_packet(&req, dev, ill->featIndex, EFFECTIVE_MAX_FUNC);

    if ((rc = hidpp_device_request(dev, &req, &res))) {
        /* the device is still busy testing itself */
        if (rc == HIDPP_EIO
            && hidpp_error_code(dev->receiver) == HIDPP_ERR_BUSY)
            return HIDPP_EAGAIN;

        return rc;
    }

    return HIDPP_WORD(res.params[0], res.params[1]);
}
