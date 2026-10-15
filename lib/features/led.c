/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

/** Section: HID++ feature documentation

    Color LED effects (id: 0x8070, versions 0 to 7)
    Big endian integers are exchanged.

    -----------------------
    Fn 0 - get info
    -----------------------
    request:
    response:
        uint8_t zoneCount
        uint16_t nvCapabilities
        uint16_t extCapabilities
        - bit 0 - getZoneEffect is supported
        - bit 1 - getEffectSettings is NOT supported
        - bit 2 - setLedBinInfo is supported
        - bit 3 - monochrome LEDs only
        - bit 4 - synchronization is NOT supported

    -----------------------
    Fn 1 - get zone info
    -----------------------
    request:
        uint8_t zone
    response:
        uint8_t zone
        uint16_t location
        uint8_t effectCount
        uint8_t persistencyCapabilities

    -----------------------
    Fn 2 - get zone effect info
    -----------------------
    request:
        uint8_t zone
        uint8_t effect
    response:
        uint8_t zone
        uint8_t effect
        uint16_t effectId
        uint16_t effectCapabilities
        uint16_t effectPeriod (ms)

    -----------------------
    Fn 3 - set zone effect
    -----------------------
    request:
        uint8_t zone
        uint8_t effect
        uint8_t params[10]
        uint8_t persistence (0 = RAM, 1 = RAM and EEPROM, 2 = EEPROM)
    response:

    -----------------------
    Fn 4 - get NV config
    -----------------------
    request:
        uint16_t nvCapability
    response:
        uint16_t nvCapability
        uint8_t state
        uint8_t params[2]

    -----------------------
    Fn 5 - set NV config
    -----------------------
    request:
        uint16_t nvCapability
        uint8_t state
        uint8_t params[2]
    response:

    -----------------------
    Fn 6 - get LED bin info
    -----------------------
    request:
        uint8_t zone
        uint8_t binIndex
    response:
        uint8_t zone
        uint8_t binIndex
        uint16_t red, green, blue, white

    -----------------------
    Fn 7 - get SW control
    -----------------------
    request:
    response:
        uint8_t swControl
        uint8_t syncEvents

    -----------------------
    Fn 8 - set SW control
    -----------------------
    request:
        uint8_t swControl
        uint8_t syncEvents
    response:

    -----------------------
    Fn 9 - get effect settings
    -----------------------
    request:
        uint8_t zone
        uint8_t persistence (0 = RAM, 1 = EEPROM)
    response:
        uint8_t zone
        uint8_t red, green, blue
        uint16_t period
        uint8_t brightness
        uint8_t param

    -----------------------
    Fn 10 - clear effect settings
    -----------------------
    request:
        uint8_t zone
    response:

    -----------------------
    Fn 11 - set cycling direction
    -----------------------
    request:
        uint8_t direction (0 = clockwise, 1 = anticlockwise)
    response:

    -----------------------
    Fn 12 - get current color
    -----------------------
    request:
        uint8_t zone
    response:
        uint8_t zone
        uint8_t red, green, blue

    -----------------------
    Fn 13 - synchronize effect
    -----------------------
    request:
        uint8_t zone (0xFF for all)
        uint8_t reserved
        int16_t drift
    response:

    -----------------------
    Fn 14 - get zone effect (version 2)
    -----------------------
    request:
        uint8_t zone
        uint8_t persistence (0 = RAM, 1 = EEPROM)
    response:
        uint8_t zone
        uint8_t effect
        uint8_t params[10]

    -----------------------
    Fn 15 - set LED bin info
    -----------------------
    request:
        (same as the response of get LED bin info)
    response:
        (echo)

    -----------------------
    Event 0 - sync effect
    -----------------------
        uint8_t zone
        uint16_t effectCounter (ms)

    RGB effects (id: 0x8071, versions 0 to 4)
    Big endian integers are exchanged. Zones are called RGB clusters. Most
    functions read or write depending on their first parameter, getOrSet
    (0 = get, 1 = set, 2 = get backup), and echo it in the response.

    -----------------------
    Fn 0 - get info
    -----------------------
    request:
        uint8_t cluster
        uint8_t effect
        uint8_t typeOfInfo
        uint8_t params[13]
    response, about the device (0xFF, 0xFF, 0):
        uint8_t cluster
        uint8_t effect
        uint8_t clusterCount
        uint16_t nvCapabilities
        uint16_t extCapabilities
        - bits 0-4 - as for 0x8070 (bit 1 is ignored)
        - bit 5 - shutdown is supported
        - bit 6 - cluster changed events are sent
        uint8_t multiClusterEffectCount
    response, about a cluster (cluster, 0xFF, 0):
        uint8_t cluster
        uint8_t effect
        uint16_t location
        uint8_t effectCount
        uint8_t displayPersistencyCapabilities
        uint8_t effectPersistencyCapabilities (bit 0 - EEPROM)
        uint8_t multiLedPatternCapabilities (bit 0)
    response, about an effect (cluster or 0xFF, effect, 0):
        (same as the response of get zone effect info of 0x8070)
    response, about an onboard stored effect (0xFF, effect, 1, slot, type):
        uint8_t cluster
        uint8_t effect
        uint8_t typeOfInfo
        uint8_t params[13]

    -----------------------
    Fn 1 - set RGB cluster effect
    -----------------------
    request:
        uint8_t cluster (0xFF for all)
        uint8_t effect
        uint8_t params[10]
        uint8_t flags
        - bits 0-1 - persistence (bit 0 - RAM, bit 1 - EEPROM)
        - bits 2-3 - power mode (0 = full, 1 = power save)
    response:

    -----------------------
    Fn 2 - set multi-LED RGB cluster pattern
    -----------------------
    request:
        uint8_t cluster
        uint8_t pattern
    response:

    -----------------------
    Fn 3 - manage NV config
    -----------------------
    request and response:
        uint8_t getOrSet
        uint16_t nvCapability
        uint8_t state
        uint8_t params[6]

    -----------------------
    Fn 4 - manage RGB LED bin info
    -----------------------
    request and response:
        uint8_t getOrSet
        uint8_t cluster
        uint8_t binIndex
        uint8_t params[8]

    -----------------------
    Fn 5 - manage SW control
    -----------------------
    request and response:
        uint8_t getOrSet
        uint8_t swControlFlags
        - bit 0 - SW controls all clusters
        - bit 1 - SW controls power modes
        uint8_t eventsNotificationFlags
        - bit 0 - effect sync
        - bit 1 - user activity
        - bit 2 - no user activity timeout

    -----------------------
    Fn 6 - set effect sync correction
    -----------------------
    request:
        uint8_t cluster (0xFF for all)
        uint8_t reserved
        int16_t drift
    response:

    -----------------------
    Fn 7 - manage RGB power mode config
    -----------------------
    request and response:
        uint8_t getOrSet
        uint16_t flags
        uint16_t noActivityTimeoutToPowerSave (s, 0xFFFF = never)
        uint16_t noActivityTimeoutToOff (s, 0xFFFF = never)

    -----------------------
    Fn 8 - manage RGB power mode
    -----------------------
    request and response:
        uint8_t getOrSet
        uint8_t powerMode (1 = full, 2 = power save, 3 = off)

    -----------------------
    Fn 9 - shutdown (version 4)
    -----------------------
    request:
    response:

    -----------------------
    Events
    -----------------------
    0 - effect sync:        uint8_t cluster, uint16_t effectCounter
    1 - user activity:      uint8_t type (0 = timeout, 1 = activity)
    2 - cluster changed:    (same as the request of set RGB cluster effect)
*/

#define COLOR_LED_ID 0x8070
#define RGB_EFFECTS_ID 0x8071

#define MAX_ZONES 16
#define MAX_EFFECTS 16
#define PARAM_COUNT 10
#define MAX_BIN_INDEX 5
#define ONBOARD_INFO_SIZE 13
#define MAX_ONBOARD_TYPE 6

#define GET 0
#define SET 1
#define GET_BACKUP 2

#define INFO_GENERAL 0
#define INFO_ONBOARD 1

/* clang-format off */

#define EXT_ZONE_EFFECT_FLAG    0x0001
#define EXT_NO_SETTINGS_FLAG    0x0002
#define EXT_SET_BIN_FLAG        0x0004
#define EXT_MONOCHROME_FLAG     0x0008
#define EXT_NO_SYNC_FLAG        0x0010
#define EXT_SHUTDOWN_FLAG       0x0020
#define EXT_CHANGED_EVENTS_FLAG 0x0040

#define EFFECT_PERSISTENCY_FLAG 0x01
#define PATTERNS_FLAG           0x01

#define PERSISTENCE_MASK        0x03
#define POWER_MODE_SHIFT        2

#define SW_CONTROL_MASK_8070    0x01
#define SW_CONTROL_MASK_8071    0x03
#define NOTIFY_MASK_8070        0x01
#define NOTIFY_MASK_8071        0x07

/* clang-format on */

struct led_zone {
    struct hidpp_led_zone info;
    struct hidpp_led_effect_info effects[MAX_EFFECTS];
};

struct hidpp_feat_led {
    hidpp_bool_t initialized;
    hidpp_bool_t unsupported;

    uint8_t featIndex;
    struct hidpp_led_info info;
    struct led_zone zones[MAX_ZONES];
    struct hidpp_led_effect_info allZones[MAX_EFFECTS]; /* 0x8071 */
};

const char *hidpp_led_effect_name(uint16_t id) {
    switch (id) {
        /* clang-format off */
    case HIDPP_LED_DISABLED:       return "Disabled";
    case HIDPP_LED_FIXED:          return "Fixed";
    case HIDPP_LED_PULSING:        return "Pulsing";
    case HIDPP_LED_CYCLING:        return "Color cycling";
    case HIDPP_LED_WAVE:           return "Color wave";
    case HIDPP_LED_STARLIGHT:      return "Starlight";
    case HIDPP_LED_LIGHT_ON_PRESS: return "Light on press";
    case HIDPP_LED_AUDIO:          return "Audio visualizer";
    case HIDPP_LED_BOOT_UP:        return "Boot up";
    case HIDPP_LED_DEMO:           return "Demo mode";
    case HIDPP_LED_BREATHING:      return "Breathing";
    case HIDPP_LED_RIPPLE:         return "Ripple";
    case HIDPP_LED_ONBOARD:        return "Custom onboard stored";
    default:                       return "Unknown";
        /* clang-format on */
    }
}

static const char *location_name(uint16_t location) {
    switch (location) {
        /* clang-format off */
    case 0x01: return "Primary";
    case 0x02: return "Logo";
    case 0x03: return "Left side";
    case 0x04: return "Right side";
    case 0x05: return "Combined";
    case 0x06: return "Primary 1";
    case 0x07: return "Primary 2";
    case 0x08: return "Primary 3";
    case 0x09: return "Primary 4";
    case 0x0A: return "Primary 5";
    case 0x0B: return "Primary 6";
    case 0x81: return "Idle state";
    case 0x82: return "In-use state";
    case 0x83: return "Muted state";
    case 0x84: return "Soft muted state";
    case 0xFF: return "All";
    default:   return "Unknown";
        /* clang-format on */
    }
}

static void unpack_flags(struct hidpp_led_info *info) {
    uint16_t ext = info->extCapabilities;
    hidpp_bool_t rgb = info->isRgbEffects;

    info->hasZoneEffect = !rgb && info->version >= 2
        && !!(ext & EXT_ZONE_EFFECT_FLAG);
    info->hasSettings = !rgb && !(ext & EXT_NO_SETTINGS_FLAG);
    info->canSetBin = !!(ext & EXT_SET_BIN_FLAG);
    info->isMonochrome = !!(ext & EXT_MONOCHROME_FLAG);
    info->hasSync = !(ext & EXT_NO_SYNC_FLAG);
    info->hasShutdown = rgb && !!(ext & EXT_SHUTDOWN_FLAG);
    info->hasChangedEvents = rgb && !!(ext & EXT_CHANGED_EVENTS_FLAG);
}

static void unpack_zone(struct hidpp_led_zone *zone) {
    zone->locationName = location_name(zone->location);
}

static void unpack_effect(struct hidpp_led_effect_info *effect) {
    effect->name = hidpp_led_effect_name(effect->id);
}

/* Prepares a 0x8071 getInfo request. */
static void make_info_request(
    hidpp_packet_t *req,
    hidpp_device_t *dev,
    struct hidpp_feat_led *led,
    uint8_t zone,
    uint8_t effect,
    uint8_t type
) {
    make_packet(req, dev, led->featIndex, 0);
    req->kind = HIDPP_KIND_LONG;
    req->params[0] = zone;
    req->params[1] = effect;
    req->params[2] = type;
}

static int query_effect(
    hidpp_device_t *dev,
    struct hidpp_feat_led *led,
    uint8_t zone,
    uint8_t index,
    struct hidpp_led_effect_info *out
) {
    hidpp_packet_t req, res;
    int rc;

    if (led->info.isRgbEffects) {
        make_info_request(&req, dev, led, zone, index, INFO_GENERAL);
    } else {
        make_packet(&req, dev, led->featIndex, 2);
        req.params[0] = zone;
        req.params[1] = index;
    }

    /* an effect the device refuses for good stays uninitialized */
    if ((rc = hidpp_device_request(dev, &req, &res)))
        return is_transient(dev, rc) ? rc : HIDPP_OK;

    out->zone = zone;
    out->index = index;
    out->id = HIDPP_WORD(res.params[2], res.params[3]);
    out->capabilities = HIDPP_WORD(res.params[4], res.params[5]);
    out->period = HIDPP_WORD(res.params[6], res.params[7]);
    unpack_effect(out);
    out->initialized = HIDPP_TRUE;
    return HIDPP_OK;
}

static int query_zone(
    hidpp_device_t *dev, struct hidpp_feat_led *led, uint8_t index
) {
    struct led_zone *z = &led->zones[index];
    struct hidpp_led_zone *info = &z->info;
    hidpp_packet_t req, res;
    int rc;

    if (led->info.isRgbEffects) {
        make_info_request(&req, dev, led, index, 0xFF, INFO_GENERAL);
    } else {
        make_packet(&req, dev, led->featIndex, 1);
        req.params[0] = index;
    }

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return is_transient(dev, rc) ? rc : HIDPP_OK;

    /* the effect index is echoed in front of 0x8071's cluster info */
    const uint8_t *p = &res.params[led->info.isRgbEffects ? 2 : 1];

    info->index = index;
    info->location = HIDPP_WORD(p[0], p[1]);
    info->effectCount = p[2];
    info->displayPersistency = p[3];

    if (led->info.isRgbEffects) {
        info->effectPersistency = p[4];
        info->hasPatterns = !!(p[5] & PATTERNS_FLAG);
    }

    if (info->effectCount > MAX_EFFECTS)
        info->effectCount = MAX_EFFECTS;

    for (uint8_t i = 0; i < info->effectCount; i++)
        if ((rc = query_effect(dev, led, index, i, &z->effects[i])))
            return rc;

    unpack_zone(info);
    info->initialized = HIDPP_TRUE;
    return HIDPP_OK;
}

static int query_info(hidpp_device_t *dev, struct hidpp_feat_led *led) {
    struct hidpp_led_info *info = &led->info;
    hidpp_packet_t req, res;
    int rc;

    if (info->isRgbEffects)
        make_info_request(&req, dev, led, 0xFF, 0xFF, INFO_GENERAL);
    else
        make_packet(&req, dev, led->featIndex, 0);

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return is_transient(dev, rc) ? rc : HIDPP_OK;

    const uint8_t *p = &res.params[info->isRgbEffects ? 2 : 0];

    info->zoneCount = p[0];
    info->nvCapabilities = HIDPP_WORD(p[1], p[2]);
    info->extCapabilities = HIDPP_WORD(p[3], p[4]);

    if (info->isRgbEffects)
        info->multiZoneEffects = p[5];

    if (info->zoneCount > MAX_ZONES)
        info->zoneCount = MAX_ZONES;

    if (info->multiZoneEffects > MAX_EFFECTS)
        info->multiZoneEffects = MAX_EFFECTS;

    for (uint8_t i = 0; i < info->zoneCount; i++)
        if ((rc = query_zone(dev, led, i)))
            return rc;

    for (uint8_t i = 0; i < info->multiZoneEffects; i++) {
        rc = query_effect(dev, led, HIDPP_LED_ALL_ZONES, i, &led->allZones[i]);

        if (rc)
            return rc;
    }

    unpack_flags(info);
    info->initialized = HIDPP_TRUE;
    return HIDPP_OK;
}

static int ensure_init(struct hidpp_feat_led **out, hidpp_device_t *dev) {
    struct hidpp_feat_led *led;

    if (!out || !dev)
        return HIDPP_EINVAL;

    led = dev->features[HIDPP_FEAT_LED];
    *out = led;

    if (led->initialized)
        return led->unsupported ? HIDPP_ENOSYS : HIDPP_OK;

    memset(led, 0, sizeof(*led));
    int featIndex = feature_lookup(dev, RGB_EFFECTS_ID);
    uint16_t id = RGB_EFFECTS_ID;
    int rc;

    if (featIndex < 0)
        return featIndex;

    led->info.isRgbEffects = featIndex != 0;

    if (featIndex == 0) {
        id = COLOR_LED_ID;

        if ((featIndex = feature_lookup(dev, id)) < 0)
            return featIndex;
    }

    led->featIndex = (uint8_t)featIndex;
    led->unsupported = featIndex == 0;

    if (!led->unsupported) {
        led->info.version = feature_version(dev, id);

        if ((rc = query_info(dev, led)))
            return rc;
    }

    led->initialized = HIDPP_TRUE;

    return led->unsupported ? HIDPP_ENOSYS : HIDPP_OK;
}

static void clear_cache(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_led *led = feat;
    (void)dev;
    led->initialized = HIDPP_FALSE;
}

static void collect_cache(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_led *led;
    (void)feat;
    ensure_init(&led, dev);
}

static uint8_t *save_effects(
    uint8_t *b, const struct hidpp_led_effect_info *effects, uint8_t count
) {
    for (uint8_t i = 0; i < count; i++) {
        SAVE_BYTE(b, effects[i].initialized);
        SAVE_WORD(b, effects[i].id);
        SAVE_WORD(b, effects[i].capabilities);
        SAVE_WORD(b, effects[i].period);
    }

    return b;
}

static uint8_t *load_effects(
    uint8_t *b,
    struct hidpp_led_effect_info *effects,
    uint8_t zone,
    uint8_t count
) {
    for (uint8_t i = 0; i < count; i++) {
        struct hidpp_led_effect_info *e = &effects[i];
        e->zone = zone;
        e->index = i;
        e->initialized = LOAD_BYTE(b);
        e->id = LOAD_WORD(b);
        e->capabilities = LOAD_WORD(b);
        e->period = LOAD_WORD(b);
        unpack_effect(e);
    }

    return b;
}

static size_t save_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_led *led = feat;
    struct hidpp_led_info *info = &led->info;
    uint8_t *start = b;
    (void)dev;

    SAVE_BYTE(b, led->initialized);
    SAVE_BYTE(b, led->unsupported);

    if (!led->initialized || led->unsupported)
        return b - start;

    SAVE_BYTE(b, led->featIndex);
    SAVE_BYTE(b, info->initialized);
    SAVE_BYTE(b, info->isRgbEffects);
    SAVE_BYTE(b, info->version);
    SAVE_BYTE(b, info->zoneCount);
    SAVE_WORD(b, info->nvCapabilities);
    SAVE_WORD(b, info->extCapabilities);
    SAVE_BYTE(b, info->multiZoneEffects);

    for (uint8_t i = 0; i < info->zoneCount; i++) {
        struct hidpp_led_zone *zone = &led->zones[i].info;
        SAVE_BYTE(b, zone->initialized);
        SAVE_WORD(b, zone->location);
        SAVE_BYTE(b, zone->effectCount);
        SAVE_BYTE(b, zone->displayPersistency);
        SAVE_BYTE(b, zone->effectPersistency);
        SAVE_BYTE(b, zone->hasPatterns);
        b = save_effects(b, led->zones[i].effects, zone->effectCount);
    }

    b = save_effects(b, led->allZones, info->multiZoneEffects);
    return b - start;
}

static size_t load_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_led *led = feat;
    struct hidpp_led_info *info = &led->info;
    uint8_t *start = b;
    (void)dev;

    led->initialized = LOAD_BYTE(b);
    led->unsupported = LOAD_BYTE(b);

    if (!led->initialized || led->unsupported)
        return b - start;

    led->featIndex = LOAD_BYTE(b);
    info->initialized = LOAD_BYTE(b);
    info->isRgbEffects = LOAD_BYTE(b);
    info->version = LOAD_BYTE(b);
    info->zoneCount = LOAD_BYTE(b);
    info->nvCapabilities = LOAD_WORD(b);
    info->extCapabilities = LOAD_WORD(b);
    info->multiZoneEffects = LOAD_BYTE(b);

    if (info->zoneCount > MAX_ZONES)
        info->zoneCount = MAX_ZONES;

    if (info->multiZoneEffects > MAX_EFFECTS)
        info->multiZoneEffects = MAX_EFFECTS;

    for (uint8_t i = 0; i < info->zoneCount; i++) {
        struct hidpp_led_zone *zone = &led->zones[i].info;
        zone->index = i;
        zone->initialized = LOAD_BYTE(b);
        zone->location = LOAD_WORD(b);
        zone->effectCount = LOAD_BYTE(b);
        zone->displayPersistency = LOAD_BYTE(b);
        zone->effectPersistency = LOAD_BYTE(b);
        zone->hasPatterns = LOAD_BYTE(b);

        if (zone->effectCount > MAX_EFFECTS)
            zone->effectCount = MAX_EFFECTS;

        b = load_effects(b, led->zones[i].effects, i, zone->effectCount);
        unpack_zone(zone);
    }

    b = load_effects(
        b, led->allZones, HIDPP_LED_ALL_ZONES, info->multiZoneEffects
    );
    unpack_flags(info);
    return b - start;
}

const struct hidpp_feat_vt hidpp_feat_led_vt = {
    .size = sizeof(struct hidpp_feat_led),
    .alignment = _Alignof(struct hidpp_feat_led),
    .maxCacheSize = sizeof(struct hidpp_feat_led),
    .minCacheSize = 2 * sizeof(uint8_t),
    .collectCache = collect_cache,
    .saveCache = save_cache,
    .loadCache = load_cache,
    .clearCache = clear_cache,
};

/* Restricts the call to one of the two features. */
static int ensure_kind(
    struct hidpp_feat_led **led, hidpp_device_t *dev, hidpp_bool_t rgb
) {
    int rc;

    if ((rc = ensure_init(led, dev)))
        return rc;

    return (*led)->info.isRgbEffects == rgb ? HIDPP_OK : HIDPP_ENOSYS;
}

static struct hidpp_led_zone *find_zone(
    struct hidpp_feat_led *led, uint8_t zone
) {
    if (zone >= led->info.zoneCount || !led->zones[zone].info.initialized)
        return NULL;

    return &led->zones[zone].info;
}

static struct hidpp_led_effect_info *find_effect(
    struct hidpp_feat_led *led, uint8_t zone, uint8_t index
) {
    struct hidpp_led_effect_info *effect;

    if (zone == HIDPP_LED_ALL_ZONES) {
        if (!led->info.isRgbEffects || index >= led->info.multiZoneEffects)
            return NULL;

        effect = &led->allZones[index];
    } else {
        struct hidpp_led_zone *info = find_zone(led, zone);

        if (!info || index >= info->effectCount)
            return NULL;

        effect = &led->zones[zone].effects[index];
    }

    return effect->initialized ? effect : NULL;
}

struct hidpp_led_info *hidpp_led_info(hidpp_device_t *dev) {
    struct hidpp_feat_led *led;

    if (!dev || ensure_init(&led, dev))
        return NULL;

    return &led->info;
}

struct hidpp_led_zone *hidpp_led_zone(hidpp_device_t *dev, uint8_t zone) {
    struct hidpp_feat_led *led;

    if (!dev || ensure_init(&led, dev))
        return NULL;

    return find_zone(led, zone);
}

struct hidpp_led_effect_info *hidpp_led_effect_info(
    hidpp_device_t *dev, uint8_t zone, uint8_t index
) {
    struct hidpp_feat_led *led;

    if (!dev || ensure_init(&led, dev))
        return NULL;

    return find_effect(led, zone, index);
}

int hidpp_led_find_effect(hidpp_device_t *dev, uint8_t zone, uint16_t id) {
    struct hidpp_feat_led *led;
    int rc;

    if ((rc = ensure_init(&led, dev)))
        return rc;

    uint8_t count;

    if (zone == HIDPP_LED_ALL_ZONES) {
        count = led->info.multiZoneEffects;
    } else {
        struct hidpp_led_zone *info = find_zone(led, zone);

        if (!info)
            return HIDPP_EINVAL;

        count = info->effectCount;
    }

    for (uint8_t i = 0; i < count; i++) {
        struct hidpp_led_effect_info *effect = find_effect(led, zone, i);

        if (effect && effect->id == id)
            return i;
    }

    return HIDPP_ENOENT;
}

static void pack_color(uint8_t *out, uint8_t r, uint8_t g, uint8_t b) {
    out[0] = r;
    out[1] = g;
    out[2] = b;
}

/*  A 16-bit period has its MSB in `msb` and LSB in `lsb`, where the LSB
    byte holds the 8-bit speed instead if the MSB is zero. */
static int pack_period(
    uint8_t *msb, uint8_t *lsb, const struct hidpp_led_params *p
) {
    if (p->period) {
        if (p->period <= 0xFF)
            return HIDPP_EINVAL; /* would be read as a speed */

        *msb = HIDPP_MSB(p->period);
        *lsb = HIDPP_LSB(p->period);
    } else {
        *lsb = p->speed;
    }

    return HIDPP_OK;
}

static void unpack_period(
    uint8_t msb, uint8_t lsb, struct hidpp_led_params *out
) {
    if (msb)
        out->period = HIDPP_WORD(msb, lsb);
    else
        out->speed = lsb;
}

int hidpp_led_pack(
    uint16_t id, const struct hidpp_led_params *p, uint8_t out[10]
) {
    if (!p || !out)
        return HIDPP_EINVAL;

    memset(out, 0, PARAM_COUNT);

    switch (id) {
    case HIDPP_LED_DISABLED:
    case HIDPP_LED_BOOT_UP:
    case HIDPP_LED_DEMO:
        return HIDPP_OK;
    case HIDPP_LED_FIXED:
        pack_color(out, p->red, p->green, p->blue);
        out[3] = p->ramp;
        return HIDPP_OK;
    case HIDPP_LED_PULSING:
        pack_color(out, p->red, p->green, p->blue);
        out[3] = p->speed;
        return HIDPP_OK;
    case HIDPP_LED_CYCLING:
        out[7] = p->intensity;
        return pack_period(&out[5], &out[6], p);
    case HIDPP_LED_WAVE:
        pack_color(out, p->red, p->green, p->blue);
        pack_color(&out[3], p->red2, p->green2, p->blue2);
        out[7] = p->direction;
        out[8] = p->intensity;
        return pack_period(&out[9], &out[6], p);
    case HIDPP_LED_STARLIGHT:
        pack_color(out, p->red, p->green, p->blue);
        pack_color(&out[3], p->red2, p->green2, p->blue2);
        return HIDPP_OK;
    case HIDPP_LED_LIGHT_ON_PRESS:
        pack_color(out, p->red, p->green, p->blue);
        pack_color(&out[3], p->red2, p->green2, p->blue2);
        out[6] = HIDPP_MSB(p->delay);
        out[7] = HIDPP_LSB(p->delay);
        return HIDPP_OK;
    case HIDPP_LED_BREATHING:
        pack_color(out, p->red, p->green, p->blue);
        out[3] = HIDPP_MSB(p->period);
        out[4] = HIDPP_LSB(p->period);
        out[5] = p->waveform;
        out[6] = p->intensity;
        return HIDPP_OK;
    case HIDPP_LED_RIPPLE:
        pack_color(out, p->red, p->green, p->blue);
        out[4] = HIDPP_MSB(p->period);
        out[5] = HIDPP_LSB(p->period);
        return HIDPP_OK;
    default:
        return HIDPP_EINVAL;
    }
}

int hidpp_led_unpack(
    uint16_t id, const uint8_t params[10], struct hidpp_led_params *out
) {
    if (!params || !out)
        return HIDPP_EINVAL;

    memset(out, 0, sizeof(*out));
    const uint8_t *p = params;

    switch (id) {
    case HIDPP_LED_DISABLED:
    case HIDPP_LED_BOOT_UP:
    case HIDPP_LED_DEMO:
        return HIDPP_OK;
    case HIDPP_LED_FIXED:
        out->red = p[0], out->green = p[1], out->blue = p[2];
        out->ramp = p[3];
        return HIDPP_OK;
    case HIDPP_LED_PULSING:
        out->red = p[0], out->green = p[1], out->blue = p[2];
        out->speed = p[3];
        return HIDPP_OK;
    case HIDPP_LED_CYCLING:
        unpack_period(p[5], p[6], out);
        out->intensity = p[7];
        return HIDPP_OK;
    case HIDPP_LED_WAVE:
        out->red = p[0], out->green = p[1], out->blue = p[2];
        out->red2 = p[3], out->green2 = p[4], out->blue2 = p[5];
        unpack_period(p[9], p[6], out);
        out->direction = p[7];
        out->intensity = p[8];
        return HIDPP_OK;
    case HIDPP_LED_STARLIGHT:
        out->red = p[0], out->green = p[1], out->blue = p[2];
        out->red2 = p[3], out->green2 = p[4], out->blue2 = p[5];
        return HIDPP_OK;
    case HIDPP_LED_LIGHT_ON_PRESS:
        out->red = p[0], out->green = p[1], out->blue = p[2];
        out->red2 = p[3], out->green2 = p[4], out->blue2 = p[5];
        out->delay = HIDPP_WORD(p[6], p[7]);
        return HIDPP_OK;
    case HIDPP_LED_BREATHING:
        out->red = p[0], out->green = p[1], out->blue = p[2];
        out->period = HIDPP_WORD(p[3], p[4]);
        out->waveform = p[5];
        out->intensity = p[6];
        return HIDPP_OK;
    case HIDPP_LED_RIPPLE:
        out->red = p[0], out->green = p[1], out->blue = p[2];
        out->period = HIDPP_WORD(p[4], p[5]);
        return HIDPP_OK;
    default:
        return HIDPP_EINVAL;
    }
}

/* 0x8070 numbers its storages, 0x8071 uses a bit for each. */
static uint8_t color_led_persistence(uint8_t persistence) {
    switch (persistence & PERSISTENCE_MASK) {
    case HIDPP_LED_VOLATILE:
        return 0;
    case HIDPP_LED_NON_VOLATILE:
        return 2;
    default:
        return 1;
    }
}

int hidpp_led_set_effect(
    hidpp_device_t *dev, const struct hidpp_led_effect *e
) {
    struct hidpp_feat_led *led;
    int rc;

    if (!e)
        return HIDPP_EINVAL;

    if ((rc = ensure_init(&led, dev)))
        return rc;

    if (!find_effect(led, e->zone, e->index))
        return HIDPP_EINVAL;

    if (e->persistence == 0 || (e->persistence & ~PERSISTENCE_MASK))
        return HIDPP_EINVAL;

    hidpp_packet_t req, res;

    if (!led->info.isRgbEffects) {
        if (e->powerMode)
            return HIDPP_EINVAL;

        make_packet(&req, dev, led->featIndex, 3);
        req.kind = HIDPP_KIND_LONG;
        req.params[0] = e->zone;
        req.params[1] = e->index;
        memcpy(&req.params[2], e->params, PARAM_COUNT);
        req.params[12] = color_led_persistence(e->persistence);
        return hidpp_device_request(dev, &req, &res);
    }

    if (e->powerMode > 1)
        return HIDPP_EINVAL;

    /* storing the effect has to be supported by the cluster */
    struct hidpp_led_zone *zone = find_zone(led, e->zone);

    if (zone && (e->persistence & HIDPP_LED_NON_VOLATILE)
        && !(zone->effectPersistency & EFFECT_PERSISTENCY_FLAG))
        return HIDPP_EINVAL;

    make_packet(&req, dev, led->featIndex, 1);
    req.kind = HIDPP_KIND_LONG;
    req.params[0] = e->zone;
    req.params[1] = e->index;
    memcpy(&req.params[2], e->params, PARAM_COUNT);
    req.params[12] = (uint8_t)(e->persistence
                               | (e->powerMode << POWER_MODE_SHIFT));
    return hidpp_device_request(dev, &req, &res);
}

int hidpp_led_get_effect(
    hidpp_device_t *dev,
    uint8_t zone,
    uint8_t persistence,
    struct hidpp_led_effect *out
) {
    struct hidpp_feat_led *led;
    int rc;

    if (!out)
        return HIDPP_EINVAL;

    if ((rc = ensure_kind(&led, dev, HIDPP_FALSE)))
        return rc;

    if (!led->info.hasZoneEffect)
        return HIDPP_ENOSYS;

    if (!find_zone(led, zone))
        return HIDPP_EINVAL;

    if (persistence != HIDPP_LED_VOLATILE
        && persistence != HIDPP_LED_NON_VOLATILE)
        return HIDPP_EINVAL;

    hidpp_packet_t req, res;
    make_packet(&req, dev, led->featIndex, 14);
    req.params[0] = zone;
    req.params[1] = persistence == HIDPP_LED_NON_VOLATILE;

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    memset(out, 0, sizeof(*out));
    out->zone = res.params[0];
    out->index = res.params[1];
    out->persistence = persistence;
    memcpy(out->params, &res.params[2], PARAM_COUNT);
    return HIDPP_OK;
}

/* A single capability the device announced. */
static int check_capability(
    const struct hidpp_feat_led *led, uint16_t capability
) {
    if (capability == 0 || (capability & (capability - 1)))
        return HIDPP_EINVAL;

    if (led->info.nvCapabilities && !(led->info.nvCapabilities & capability))
        return HIDPP_EINVAL;

    return HIDPP_OK;
}

int hidpp_led_nv_get(
    hidpp_device_t *dev, uint16_t capability, struct hidpp_led_nv *out
) {
    struct hidpp_feat_led *led;
    hidpp_packet_t req, res;
    int rc;

    if (!out)
        return HIDPP_EINVAL;

    if ((rc = ensure_init(&led, dev)))
        return rc;

    if ((rc = check_capability(led, capability)))
        return rc;

    memset(out, 0, sizeof(*out));
    out->capability = capability;

    if (!led->info.isRgbEffects) {
        make_packet(&req, dev, led->featIndex, 4);
        req.params[0] = HIDPP_MSB(capability);
        req.params[1] = HIDPP_LSB(capability);

        if ((rc = hidpp_device_request(dev, &req, &res)))
            return rc;

        out->state = res.params[2];
        out->params[0] = res.params[3];
        out->params[1] = res.params[4];
        return HIDPP_OK;
    }

    make_packet(&req, dev, led->featIndex, 3);
    req.kind = HIDPP_KIND_LONG;
    req.params[0] = GET;
    req.params[1] = HIDPP_MSB(capability);
    req.params[2] = HIDPP_LSB(capability);

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    out->state = res.params[3];
    memcpy(out->params, &res.params[4], sizeof(out->params));
    return HIDPP_OK;
}

int hidpp_led_nv_set(hidpp_device_t *dev, const struct hidpp_led_nv *cfg) {
    struct hidpp_feat_led *led;
    hidpp_packet_t req, res;
    int rc;

    if (!cfg)
        return HIDPP_EINVAL;

    if ((rc = ensure_init(&led, dev)))
        return rc;

    if ((rc = check_capability(led, cfg->capability)))
        return rc;

    if (!led->info.isRgbEffects) {
        /* there is only room for two parameters */
        for (size_t i = 2; i < sizeof(cfg->params); i++)
            if (cfg->params[i])
                return HIDPP_EINVAL;

        make_packet(&req, dev, led->featIndex, 5);
        req.params[0] = HIDPP_MSB(cfg->capability);
        req.params[1] = HIDPP_LSB(cfg->capability);
        req.params[2] = cfg->state;
        req.params[3] = cfg->params[0];
        req.params[4] = cfg->params[1];
        req.kind = HIDPP_KIND_LONG;
        return hidpp_device_request(dev, &req, &res);
    }

    make_packet(&req, dev, led->featIndex, 3);
    req.kind = HIDPP_KIND_LONG;
    req.params[0] = SET;
    req.params[1] = HIDPP_MSB(cfg->capability);
    req.params[2] = HIDPP_LSB(cfg->capability);
    req.params[3] = cfg->state;
    memcpy(&req.params[4], cfg->params, sizeof(cfg->params));
    return hidpp_device_request(dev, &req, &res);
}

int hidpp_led_sw_control_get(
    hidpp_device_t *dev, uint8_t *control, uint8_t *notifications
) {
    struct hidpp_feat_led *led;
    hidpp_packet_t req, res;
    int rc;

    if (!control || !notifications)
        return HIDPP_EINVAL;

    if ((rc = ensure_init(&led, dev)))
        return rc;

    if (!led->info.isRgbEffects) {
        make_packet(&req, dev, led->featIndex, 7);

        if ((rc = hidpp_device_request(dev, &req, &res)))
            return rc;

        *control = res.params[0] ? HIDPP_LED_SW_EFFECTS : 0;
        *notifications = res.params[1] ? HIDPP_LED_NOTIFY_SYNC : 0;
        return HIDPP_OK;
    }

    make_packet(&req, dev, led->featIndex, 5);
    req.params[0] = GET;

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    *control = res.params[1] & SW_CONTROL_MASK_8071;
    *notifications = res.params[2] & NOTIFY_MASK_8071;
    return HIDPP_OK;
}

int hidpp_led_sw_control_set(
    hidpp_device_t *dev, uint8_t control, uint8_t notifications
) {
    struct hidpp_feat_led *led;
    hidpp_packet_t req, res;
    int rc;

    if ((rc = ensure_init(&led, dev)))
        return rc;

    hidpp_bool_t rgb = led->info.isRgbEffects;
    uint8_t controlMask = rgb ? SW_CONTROL_MASK_8071 : SW_CONTROL_MASK_8070;
    uint8_t notifyMask = rgb ? NOTIFY_MASK_8071 : NOTIFY_MASK_8070;

    if ((control & ~controlMask) || (notifications & ~notifyMask))
        return HIDPP_EINVAL;

    if ((notifications & HIDPP_LED_NOTIFY_SYNC) && !led->info.hasSync)
        return HIDPP_EINVAL;

    if (!rgb) {
        make_packet(&req, dev, led->featIndex, 8);
        req.params[0] = control;
        req.params[1] = notifications;
        return hidpp_device_request(dev, &req, &res);
    }

    make_packet(&req, dev, led->featIndex, 5);
    req.params[0] = SET;
    req.params[1] = control;
    req.params[2] = notifications;
    return hidpp_device_request(dev, &req, &res);
}

int hidpp_led_bin_get(
    hidpp_device_t *dev,
    uint8_t zone,
    uint8_t binIndex,
    int backup,
    struct hidpp_led_bin *out
) {
    struct hidpp_feat_led *led;
    hidpp_packet_t req, res;
    int rc;

    if (!out)
        return HIDPP_EINVAL;

    if ((rc = ensure_init(&led, dev)))
        return rc;

    if (!find_zone(led, zone) || binIndex > MAX_BIN_INDEX)
        return HIDPP_EINVAL;

    out->zone = zone;
    out->binIndex = binIndex;

    if (!led->info.isRgbEffects) {
        if (backup)
            return HIDPP_ENOSYS;

        make_packet(&req, dev, led->featIndex, 6);
        req.params[0] = zone;
        req.params[1] = binIndex;

        if ((rc = hidpp_device_request(dev, &req, &res)))
            return rc;

        memcpy(out->data, &res.params[2], sizeof(out->data));
        return HIDPP_OK;
    }

    make_packet(&req, dev, led->featIndex, 4);
    req.params[0] = backup ? GET_BACKUP : GET;
    req.params[1] = zone;
    req.params[2] = binIndex;

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    memcpy(out->data, &res.params[3], sizeof(out->data));
    return HIDPP_OK;
}

int hidpp_led_bin_set(hidpp_device_t *dev, const struct hidpp_led_bin *bin) {
    struct hidpp_feat_led *led;
    hidpp_packet_t req, res;
    int rc;

    if (!bin)
        return HIDPP_EINVAL;

    if ((rc = ensure_init(&led, dev)))
        return rc;

    if (!find_zone(led, bin->zone) || bin->binIndex > MAX_BIN_INDEX)
        return HIDPP_EINVAL;

    if (!led->info.isRgbEffects) {
        if (!led->info.canSetBin)
            return HIDPP_ENOSYS;

        make_packet(&req, dev, led->featIndex, 15);
        req.kind = HIDPP_KIND_LONG;
        req.params[0] = bin->zone;
        req.params[1] = bin->binIndex;
        memcpy(&req.params[2], bin->data, sizeof(bin->data));
        return hidpp_device_request(dev, &req, &res);
    }

    make_packet(&req, dev, led->featIndex, 4);
    req.kind = HIDPP_KIND_LONG;
    req.params[0] = SET;
    req.params[1] = bin->zone;
    req.params[2] = bin->binIndex;
    memcpy(&req.params[3], bin->data, sizeof(bin->data));
    return hidpp_device_request(dev, &req, &res);
}

int hidpp_led_sync(hidpp_device_t *dev, uint8_t zone, int16_t drift) {
    struct hidpp_feat_led *led;
    int rc;

    if ((rc = ensure_init(&led, dev)))
        return rc;

    if (!led->info.hasSync)
        return HIDPP_ENOSYS;

    if (zone != HIDPP_LED_ALL_ZONES && !find_zone(led, zone))
        return HIDPP_EINVAL;

    uint16_t raw = (uint16_t)drift;
    hidpp_packet_t req, res;
    make_packet(&req, dev, led->featIndex, led->info.isRgbEffects ? 6 : 13);
    req.params[0] = zone;
    req.params[2] = HIDPP_MSB(raw);
    req.params[3] = HIDPP_LSB(raw);
    req.kind = HIDPP_KIND_LONG;
    return hidpp_device_request(dev, &req, &res);
}

int hidpp_led_settings(
    hidpp_device_t *dev,
    uint8_t zone,
    uint8_t persistence,
    struct hidpp_led_settings *out
) {
    struct hidpp_feat_led *led;
    int rc;

    if (!out)
        return HIDPP_EINVAL;

    if ((rc = ensure_kind(&led, dev, HIDPP_FALSE)))
        return rc;

    if (!led->info.hasSettings)
        return HIDPP_ENOSYS;

    if (!find_zone(led, zone))
        return HIDPP_EINVAL;

    if (persistence != HIDPP_LED_VOLATILE
        && persistence != HIDPP_LED_NON_VOLATILE)
        return HIDPP_EINVAL;

    hidpp_packet_t req, res;
    make_packet(&req, dev, led->featIndex, 9);
    req.params[0] = zone;
    req.params[1] = persistence == HIDPP_LED_NON_VOLATILE;

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    out->red = res.params[1];
    out->green = res.params[2];
    out->blue = res.params[3];
    out->period = HIDPP_WORD(res.params[4], res.params[5]);
    out->brightness = res.params[6];
    out->param = res.params[7];
    return HIDPP_OK;
}

int hidpp_led_clear_settings(hidpp_device_t *dev, uint8_t zone) {
    struct hidpp_feat_led *led;
    int rc;

    if ((rc = ensure_kind(&led, dev, HIDPP_FALSE)))
        return rc;

    if (!find_zone(led, zone))
        return HIDPP_EINVAL;

    hidpp_packet_t req, res;
    make_packet(&req, dev, led->featIndex, 10);
    req.params[0] = zone;
    return hidpp_device_request(dev, &req, &res);
}

int hidpp_led_cycling_direction(hidpp_device_t *dev, int direction) {
    struct hidpp_feat_led *led;
    int rc;

    if ((rc = ensure_kind(&led, dev, HIDPP_FALSE)))
        return rc;

    hidpp_packet_t req, res;
    make_packet(&req, dev, led->featIndex, 11);
    req.params[0] = direction ? 1 : 0;
    return hidpp_device_request(dev, &req, &res);
}

int hidpp_led_current_color(hidpp_device_t *dev, uint8_t zone, uint8_t rgb[3]) {
    struct hidpp_feat_led *led;
    int rc;

    if (!rgb)
        return HIDPP_EINVAL;

    if ((rc = ensure_kind(&led, dev, HIDPP_FALSE)))
        return rc;

    if (!find_zone(led, zone))
        return HIDPP_EINVAL;

    hidpp_packet_t req, res;
    make_packet(&req, dev, led->featIndex, 12);
    req.params[0] = zone;

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    memcpy(rgb, &res.params[1], 3);
    return HIDPP_OK;
}

int hidpp_led_set_pattern(hidpp_device_t *dev, uint8_t zone, uint8_t pattern) {
    struct hidpp_feat_led *led;
    int rc;

    if ((rc = ensure_kind(&led, dev, HIDPP_TRUE)))
        return rc;

    struct hidpp_led_zone *info = find_zone(led, zone);

    if (!info)
        return HIDPP_EINVAL;

    if (!info->hasPatterns)
        return HIDPP_ENOSYS;

    hidpp_packet_t req, res;
    make_packet(&req, dev, led->featIndex, 2);
    req.params[0] = zone;
    req.params[1] = pattern;
    return hidpp_device_request(dev, &req, &res);
}

int hidpp_led_power_config_get(
    hidpp_device_t *dev, struct hidpp_led_power_config *out
) {
    struct hidpp_feat_led *led;
    int rc;

    if (!out)
        return HIDPP_EINVAL;

    if ((rc = ensure_kind(&led, dev, HIDPP_TRUE)))
        return rc;

    hidpp_packet_t req, res;
    make_packet(&req, dev, led->featIndex, 7);
    req.kind = HIDPP_KIND_LONG;
    req.params[0] = GET;

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    out->flags = HIDPP_WORD(res.params[1], res.params[2]);
    out->toPowerSave = HIDPP_WORD(res.params[3], res.params[4]);
    out->toOff = HIDPP_WORD(res.params[5], res.params[6]);
    return HIDPP_OK;
}

int hidpp_led_power_config_set(
    hidpp_device_t *dev, const struct hidpp_led_power_config *cfg
) {
    struct hidpp_feat_led *led;
    int rc;

    if (!cfg)
        return HIDPP_EINVAL;

    if ((rc = ensure_kind(&led, dev, HIDPP_TRUE)))
        return rc;

    hidpp_packet_t req, res;
    make_packet(&req, dev, led->featIndex, 7);
    req.kind = HIDPP_KIND_LONG;
    req.params[0] = SET;
    req.params[1] = HIDPP_MSB(cfg->flags);
    req.params[2] = HIDPP_LSB(cfg->flags);
    req.params[3] = HIDPP_MSB(cfg->toPowerSave);
    req.params[4] = HIDPP_LSB(cfg->toPowerSave);
    req.params[5] = HIDPP_MSB(cfg->toOff);
    req.params[6] = HIDPP_LSB(cfg->toOff);
    return hidpp_device_request(dev, &req, &res);
}

int hidpp_led_power_mode(hidpp_device_t *dev) {
    struct hidpp_feat_led *led;
    int rc;

    if ((rc = ensure_kind(&led, dev, HIDPP_TRUE)))
        return rc;

    hidpp_packet_t req, res;
    make_packet(&req, dev, led->featIndex, 8);
    req.params[0] = GET;

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    return res.params[1];
}

int hidpp_led_set_power_mode(hidpp_device_t *dev, uint8_t mode) {
    struct hidpp_feat_led *led;
    int rc;

    if ((rc = ensure_kind(&led, dev, HIDPP_TRUE)))
        return rc;

    if (mode < HIDPP_LED_POWER_FULL || mode > HIDPP_LED_POWER_OFF)
        return HIDPP_EINVAL;

    hidpp_packet_t req, res;
    make_packet(&req, dev, led->featIndex, 8);
    req.params[0] = SET;
    req.params[1] = mode;
    return hidpp_device_request(dev, &req, &res);
}

int hidpp_led_shutdown(hidpp_device_t *dev) {
    struct hidpp_feat_led *led;
    int rc;

    if ((rc = ensure_kind(&led, dev, HIDPP_TRUE)))
        return rc;

    if (!led->info.hasShutdown)
        return HIDPP_ENOSYS;

    hidpp_packet_t req, res;
    make_packet(&req, dev, led->featIndex, 9);
    return hidpp_device_request(dev, &req, &res);
}

int hidpp_led_onboard_info(
    hidpp_device_t *dev,
    uint8_t effect,
    uint8_t slot,
    uint8_t type,
    uint8_t out[13]
) {
    struct hidpp_feat_led *led;
    int rc;

    if (!out)
        return HIDPP_EINVAL;

    if ((rc = ensure_kind(&led, dev, HIDPP_TRUE)))
        return rc;

    if (type > MAX_ONBOARD_TYPE)
        return HIDPP_EINVAL;

    hidpp_packet_t req, res;
    make_info_request(&req, dev, led, 0xFF, effect, INFO_ONBOARD);
    req.params[3] = slot;
    req.params[4] = type;

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    memcpy(out, &res.params[3], ONBOARD_INFO_SIZE);
    return HIDPP_OK;
}
