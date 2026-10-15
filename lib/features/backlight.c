/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

/** Section: HID++ feature documentation

    Backlight (id: 0x1982, versions 0 to 3)
    Durations are the exception and are little endian.

    -----------------------
    Fn 0 - get configuration
    -----------------------
    request:
    response:
        uint8_t configuration
        - bit 0 - bcklEn
        uint8_t options
        - bit 0 - wow
        - bit 1 - crown
        - bit 2 - pwrSave
        - bits 3-4 - bcklMode (version 3)
        uint8_t supported
        - bit 0 - wow supported
        - bit 1 - crown supported
        - bit 2 - pwrSave supported
        - bit 3 - automatic mode supported (version 3)
        - bit 4 - temporary manual mode supported (version 3)
        - bit 5 - permanent manual mode supported (version 3)
        uint16_t effectList (version 2, little endian)
        uint8_t level (version 3)
        uint16_t durationHandsOut (version 3, little endian, 5 s units)
        uint16_t durationHandsIn (version 3)
        uint16_t durationPowered (version 3)

    -----------------------
    Fn 1 - set configuration (persistent)
    -----------------------
    request:
        uint8_t configuration
        uint8_t options
        uint8_t effect (version 2)
        uint8_t level (version 3)
        uint16_t durationHandsOut (version 3)
        uint16_t durationHandsIn (version 3)
        uint16_t durationPowered (version 3)
    response:

    -----------------------
    Fn 2 - get info
    -----------------------
    request:
    response:
        uint8_t levelCount
        uint8_t level
        uint8_t status
        uint8_t effect (version 2)
        uint16_t defaultDurationHandsOut (version 3)
        uint16_t defaultDurationHandsIn (version 3)
        uint16_t defaultDurationPowered (version 3)

    -----------------------
    Fn 3 - set effect (volatile, version 2)
    -----------------------
    request:
        uint8_t effect
    response:

    -----------------------
    Event 0 - backlight info
    -----------------------
        uint8_t levelCount
        uint8_t level
        uint8_t status
        uint8_t effect
*/

#define BACKLIGHT_ID 0x1982

#define DURATION_UNIT 5     /* seconds */
#define DURATION_MAX 0x05A0 /* 2 hours */
#define MAX_EFFECT 15

/* clang-format off */

#define CONFIG_ENABLED_FLAG    0x01

#define OPT_WOW_FLAG           0x01
#define OPT_CROWN_FLAG         0x02
#define OPT_PWRSAVE_FLAG       0x04
#define OPT_MODE_SHIFT         3
#define OPT_MODE_MASK          0x03

#define SUP_WOW_FLAG           0x01
#define SUP_CROWN_FLAG         0x02
#define SUP_PWRSAVE_FLAG       0x04
#define SUP_AUTO_FLAG          0x08
#define SUP_TEMP_MANUAL_FLAG   0x10
#define SUP_PERM_MANUAL_FLAG   0x20

/* clang-format on */

struct hidpp_feat_backlight {
    struct hidpp_feat_base base;
    uint8_t supported; /* the "supported" byte of getBacklightConfig */
    struct hidpp_backlight_info info;
};

static const char *get_status_name(int status) {
    switch (status) {
        /* clang-format off */
    case HIDPP_BACKLIGHT_DISABLED_BY_SW:      return "Disabled by software";
    case HIDPP_BACKLIGHT_DISABLED_BY_BATTERY: return "Disabled by critical battery";
    case HIDPP_BACKLIGHT_ALS:                 return "Automatic";
    case HIDPP_BACKLIGHT_ALS_SATURATED:       return "Automatic, saturated";
    case HIDPP_BACKLIGHT_MANUAL_TEMPORARY:    return "Manual, temporary";
    case HIDPP_BACKLIGHT_MANUAL_PERMANENT:    return "Manual, permanent";
    default:                                  return "Unknown";
        /* clang-format on */
    }
}

const char *hidpp_backlight_effect_name(uint8_t effect) {
    switch (effect) {
        /* clang-format off */
    case HIDPP_BACKLIGHT_EFFECT_STATIC:    return "Static";
    case HIDPP_BACKLIGHT_EFFECT_NONE:      return "None";
    case HIDPP_BACKLIGHT_EFFECT_BREATHING: return "Breathing light";
    case HIDPP_BACKLIGHT_EFFECT_CONTRAST:  return "Contrast";
    case HIDPP_BACKLIGHT_EFFECT_REACTION:  return "Reaction";
    case HIDPP_BACKLIGHT_EFFECT_RANDOM:    return "Random";
    case HIDPP_BACKLIGHT_EFFECT_WAVES:     return "Waves";
    default:                               return "Unknown";
        /* clang-format on */
    }
}

static uint16_t load_duration(const uint8_t *b) {
    /* durations are little endian, in units of 5 seconds */
    return (uint16_t)(HIDPP_WORD(b[1], b[0]) * DURATION_UNIT);
}

static void save_duration(uint8_t *b, uint16_t seconds) {
    uint16_t raw = (uint16_t)(seconds / DURATION_UNIT);
    b[0] = HIDPP_LSB(raw);
    b[1] = HIDPP_MSB(raw);
}

static void unpack_flags(struct hidpp_feat_backlight *bl) {
    struct hidpp_backlight_info *info = &bl->info;

    info->hasEffects = info->version >= 2;
    info->hasDurations = info->version >= 3;
    info->supportsWow = !!(bl->supported & SUP_WOW_FLAG);
    info->supportsCrown = !!(bl->supported & SUP_CROWN_FLAG);
    info->supportsPowerSave = !!(bl->supported & SUP_PWRSAVE_FLAG);
    info->supportsAutoMode = !!(bl->supported & SUP_AUTO_FLAG);
    info->supportsTempManualMode = !!(bl->supported & SUP_TEMP_MANUAL_FLAG);
    info->supportsPermManualMode = !!(bl->supported & SUP_PERM_MANUAL_FLAG);
}

static int query_info(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_backlight *bl = feat;
    bl->info.version = bl->base.version;
    struct hidpp_backlight_info *info = &bl->info;
    hidpp_packet_t req, res;

    int rc;

    make_packet(&req, dev, bl->base.featIndex, 0);

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return is_transient(dev, rc) ? rc : HIDPP_OK;

    bl->supported = res.params[2];

    if (info->version >= 2)
        info->effectList = HIDPP_WORD(res.params[4], res.params[3]);

    make_packet(&req, dev, bl->base.featIndex, 2);

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return is_transient(dev, rc) ? rc : HIDPP_OK;

    info->levelCount = res.params[0];

    if (info->version >= 3) {
        info->defaultDurationHandsOut = load_duration(&res.params[4]);
        info->defaultDurationHandsIn = load_duration(&res.params[6]);
        info->defaultDurationPowered = load_duration(&res.params[8]);
    }

    unpack_flags(bl);
    info->initialized = HIDPP_TRUE;
    return HIDPP_OK;
}

static int ensure_init(struct hidpp_feat_backlight **out, hidpp_device_t *dev) {
    void *feat;
    int rc = hidpp__feature_init(dev, HIDPP_FEAT_BACKLIGHT, &feat);
    *out = feat;
    return rc;
}

static size_t save_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_backlight *bl = feat;
    uint8_t *start = b;
    (void)dev;

    SAVE_BYTE(b, bl->supported);
    SAVE_BYTE(b, bl->info.initialized);
    SAVE_BYTE(b, bl->info.levelCount);
    SAVE_WORD(b, bl->info.effectList);
    SAVE_WORD(b, bl->info.defaultDurationHandsOut);
    SAVE_WORD(b, bl->info.defaultDurationHandsIn);
    SAVE_WORD(b, bl->info.defaultDurationPowered);

    return b - start;
}

static size_t load_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_backlight *bl = feat;
    uint8_t *start = b;
    (void)dev;

    bl->supported = LOAD_BYTE(b);
    bl->info.initialized = LOAD_BYTE(b);
    bl->info.version = bl->base.version;
    bl->info.levelCount = LOAD_BYTE(b);
    bl->info.effectList = LOAD_WORD(b);
    bl->info.defaultDurationHandsOut = LOAD_WORD(b);
    bl->info.defaultDurationHandsIn = LOAD_WORD(b);
    bl->info.defaultDurationPowered = LOAD_WORD(b);
    unpack_flags(bl);

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
    const uint8_t *p = pkt->params;
    (void)dev, (void)feat;

    if (id != BACKLIGHT_ID || func != 0)
        return HIDPP_ENOENT;

    e->type = HIDPP_EVENT_BACKLIGHT;
    e->as.backlight.levelCount = p[0];
    e->as.backlight.level = p[1];
    e->as.backlight.status = p[2];
    e->as.backlight.effect = p[3];
    return HIDPP_OK;
}

const struct hidpp_feat_vt hidpp_feat_backlight_vt = {
    FEAT_VT_SIZE(struct hidpp_feat_backlight),
    .ids = { BACKLIGHT_ID },
    .query = query_info,
    .saveCache = save_cache,
    .loadCache = load_cache,
    .parseEvent = parse_event,
};

/* Effects are numbered like the bits of the effect list. */
static int check_effect(
    const struct hidpp_backlight_info *info, uint8_t effect
) {
    if (info->effectList == 0)
        return HIDPP_OK; /* the device has not told which ones exist */

    if (effect > MAX_EFFECT || !(info->effectList & (1u << effect)))
        return HIDPP_EINVAL;

    return HIDPP_OK;
}

static int check_mode(const struct hidpp_backlight_info *info, uint8_t mode) {
    int any = info->supportsAutoMode || info->supportsTempManualMode
        || info->supportsPermManualMode;

    switch (mode) {
    case HIDPP_BACKLIGHT_MODE_NONE:
        return any ? HIDPP_EINVAL : HIDPP_OK;
    case HIDPP_BACKLIGHT_MODE_AUTO:
        return info->supportsAutoMode ? HIDPP_OK : HIDPP_EINVAL;
    case HIDPP_BACKLIGHT_MODE_TEMP_MANUAL:
        return info->supportsTempManualMode ? HIDPP_OK : HIDPP_EINVAL;
    case HIDPP_BACKLIGHT_MODE_PERM_MANUAL:
        return info->supportsPermManualMode ? HIDPP_OK : HIDPP_EINVAL;
    default:
        return HIDPP_EINVAL;
    }
}

static int check_duration(uint16_t seconds) {
    if (seconds < DURATION_UNIT || seconds > DURATION_MAX * DURATION_UNIT
        || seconds % DURATION_UNIT)
        return HIDPP_EINVAL;

    return HIDPP_OK;
}

static int check_config(
    const struct hidpp_backlight_info *info,
    const struct hidpp_backlight_config *cfg
) {
    int rc;

    if ((cfg->wow && !info->supportsWow) || (cfg->crown && !info->supportsCrown)
        || (cfg->powerSave && !info->supportsPowerSave))
        return HIDPP_EINVAL;

    if (info->hasEffects && (rc = check_effect(info, cfg->effect)))
        return rc;

    if (!info->hasDurations)
        return cfg->mode == HIDPP_BACKLIGHT_MODE_NONE ? HIDPP_OK : HIDPP_EINVAL;

    if ((rc = check_mode(info, cfg->mode)))
        return rc;

    if (info->levelCount && cfg->level >= info->levelCount)
        return HIDPP_EINVAL;

    if ((rc = check_duration(cfg->durationHandsOut))
        || (rc = check_duration(cfg->durationHandsIn))
        || (rc = check_duration(cfg->durationPowered)))
        return rc;

    return HIDPP_OK;
}

struct hidpp_backlight_info *hidpp_backlight_info(hidpp_device_t *dev) {
    struct hidpp_feat_backlight *bl;

    if (!dev || ensure_init(&bl, dev))
        return NULL;

    return &bl->info;
}

int hidpp_backlight_get(
    hidpp_device_t *dev, struct hidpp_backlight_config *out
) {
    struct hidpp_feat_backlight *bl;
    int rc;

    if (!out)
        return HIDPP_EINVAL;

    if ((rc = ensure_init(&bl, dev)))
        return rc;

    const struct hidpp_backlight_info *info = &bl->info;
    hidpp_packet_t req, res;
    make_packet(&req, dev, bl->base.featIndex, 0);

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    memset(out, 0, sizeof(*out));
    out->enabled = !!(res.params[0] & CONFIG_ENABLED_FLAG);
    out->wow = !!(res.params[1] & OPT_WOW_FLAG);
    out->crown = !!(res.params[1] & OPT_CROWN_FLAG);
    out->powerSave = !!(res.params[1] & OPT_PWRSAVE_FLAG);

    if (info->hasDurations) {
        out->mode = (res.params[1] >> OPT_MODE_SHIFT) & OPT_MODE_MASK;
        out->level = res.params[5];
        out->durationHandsOut = load_duration(&res.params[6]);
        out->durationHandsIn = load_duration(&res.params[8]);
        out->durationPowered = load_duration(&res.params[10]);
    }

    if (!info->hasEffects)
        return HIDPP_OK;

    /* the active effect is not part of the configuration response */
    struct hidpp_backlight_state state;

    if ((rc = hidpp_backlight_state(dev, &state)))
        return rc;

    out->effect = state.effect;
    return HIDPP_OK;
}

int hidpp_backlight_set(
    hidpp_device_t *dev, const struct hidpp_backlight_config *cfg
) {
    struct hidpp_feat_backlight *bl;
    int rc;

    if (!cfg)
        return HIDPP_EINVAL;

    if ((rc = ensure_init(&bl, dev)))
        return rc;

    const struct hidpp_backlight_info *info = &bl->info;

    if ((rc = check_config(info, cfg)))
        return rc;

    hidpp_packet_t req, res;
    make_packet(&req, dev, bl->base.featIndex, 1);

    uint8_t options = 0;
    options |= cfg->wow ? OPT_WOW_FLAG : 0;
    options |= cfg->crown ? OPT_CROWN_FLAG : 0;
    options |= cfg->powerSave ? OPT_PWRSAVE_FLAG : 0;
    options |= (uint8_t)((cfg->mode & OPT_MODE_MASK) << OPT_MODE_SHIFT);

    req.params[0] = cfg->enabled ? CONFIG_ENABLED_FLAG : 0;
    req.params[1] = options;
    req.params[2] = info->hasEffects ? cfg->effect : 0;

    if (info->hasDurations) {
        req.kind = HIDPP_KIND_LONG;
        req.params[3] = cfg->level;
        save_duration(&req.params[4], cfg->durationHandsOut);
        save_duration(&req.params[6], cfg->durationHandsIn);
        save_duration(&req.params[8], cfg->durationPowered);
    }

    return hidpp_device_request(dev, &req, &res);
}

int hidpp_backlight_state(
    hidpp_device_t *dev, struct hidpp_backlight_state *out
) {
    struct hidpp_feat_backlight *bl;
    int rc;

    if (!out)
        return HIDPP_EINVAL;

    if ((rc = ensure_init(&bl, dev)))
        return rc;

    hidpp_packet_t req, res;
    make_packet(&req, dev, bl->base.featIndex, 2);

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    out->levelCount = res.params[0];
    out->level = res.params[1];
    out->status = res.params[2];
    out->effect = bl->info.hasEffects ? res.params[3] : 0;
    out->statusName = get_status_name(out->status);
    out->effectName = hidpp_backlight_effect_name(out->effect);

    return HIDPP_OK;
}

int hidpp_backlight_set_effect(hidpp_device_t *dev, uint8_t effect) {
    struct hidpp_feat_backlight *bl;
    int rc;

    if ((rc = ensure_init(&bl, dev)))
        return rc;

    if (!bl->info.hasEffects)
        return HIDPP_ENOSYS;

    if ((rc = check_effect(&bl->info, effect)))
        return rc;

    hidpp_packet_t req, res;
    make_packet(&req, dev, bl->base.featIndex, 3);
    req.params[0] = effect;

    return hidpp_device_request(dev, &req, &res);
}
