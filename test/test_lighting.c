/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

/*  Lighting: brightness control (0x8040), color LED effects (0x8070),
    RGB effects (0x8071) and per-key lighting (0x8081). */

#include "test.h"

/* ---------------------------- brightness ----------------------------- */

static int brightness_index;

static fake_t *make_brightness_version(uint8_t version) {
    fake_t *f = t_new();
    fake_add_firmware_info(f);
    brightness_index = t_add_canned(f, 0x8040, version);
    /* max 1000, 260 steps, every capability, min 10 */
    const uint8_t info[] = { 0x03, 0xE8, 0x04, 0x1F, 0x00, 0x0A, 0x01 };
    t_answer(brightness_index, 0, info, sizeof(info));
    t_answer(brightness_index, 1, (const uint8_t[]) { 0x01, 0xF4 }, 2);
    t_answer(brightness_index, 3, (const uint8_t[]) { 1 }, 1);
    return f;
}

static fake_t *make_brightness(void) { return make_brightness_version(1); }

static int test_brightness_info(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_brightness();
    hidpp_device_t *dev = t_open(f);
    struct hidpp_brightness_info *info = hidpp_brightness_info(dev);
    T_NOT_NULL(info);
    T_EQ(info->max, 1000);
    T_EQ(info->min, 10);
    T_EQ(info->steps, 0x0104);
    T_TRUE(info->hasHwBrightness && info->hasEvents && info->hasState);
    T_TRUE(info->hasHwOnOff && info->isTransient);
    fake_free(f);
    return 0;
}

static int test_brightness_info_v0(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_brightness_version(0);
    hidpp_device_t *dev = t_open(f);
    struct hidpp_brightness_info *info = hidpp_brightness_info(dev);
    T_NOT_NULL(info);
    T_EQ(info->min, 0);
    T_EQ(info->steps, 4);
    T_TRUE(info->hasHwBrightness && info->hasEvents);
    T_FALSE(info->hasState || info->hasHwOnOff || info->isTransient);
    T_ERR(hidpp_brightness_state(dev), HIDPP_ENOSYS);
    T_ERR(hidpp_brightness_set_state(dev, 1), HIDPP_ENOSYS);
    T_OK(hidpp_brightness_set(dev, 0));
    fake_free(f);
    return 0;
}

static int test_brightness_get_set(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_brightness();
    hidpp_device_t *dev = t_open(f);
    T_EQ(hidpp_brightness_get(dev), 500);
    T_OK(hidpp_brightness_set(dev, 1000));
    T_EQ(T_FUNC(f), 2);
    T_EQ(HIDPP_WORD(T_PARAM(f, 0), T_PARAM(f, 1)), 1000);
    T_ERR(hidpp_brightness_set(dev, 9), HIDPP_EINVAL);
    T_ERR(hidpp_brightness_set(dev, 1001), HIDPP_EINVAL);

    T_EQ(hidpp_brightness_state(dev), 1);
    T_OK(hidpp_brightness_set_state(dev, 0));
    T_EQ(T_FUNC(f), 4);
    T_EQ(T_PARAM(f, 0), 0);
    fake_free(f);
    return 0;
}

static int test_brightness_events(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_brightness();
    hidpp_device_t *dev = t_open(f);
    const uint8_t value[] = { 0x00, 0x64 };
    struct hidpp_event e = t_event(f, dev, brightness_index, 0, value, 2);
    T_EQ(e.type, HIDPP_EVENT_BRIGHTNESS);
    T_EQ(e.as.brightness.kind, HIDPP_BRIGHTNESS_EVENT_VALUE);
    T_EQ(e.as.brightness.value, 100);

    e = t_event(f, dev, brightness_index, 1, (const uint8_t[]) { 1 }, 1);
    T_EQ(e.as.brightness.kind, HIDPP_BRIGHTNESS_EVENT_STATE);
    T_EQ(e.as.brightness.value, 1);
    fake_free(f);
    return 0;
}

static int test_brightness_unsupported_and_cache(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = t_new();
    hidpp_device_t *dev = t_open(f);
    T_NULL(hidpp_brightness_info(dev));
    T_ERR(hidpp_brightness_get(dev), HIDPP_ENOSYS);
    T_ERR(hidpp_brightness_get(NULL), HIDPP_EINVAL);
    fake_free(f);

    dev = t_from_cache(make_brightness, &f);
    T_EQ(hidpp_brightness_info(dev)->steps, 0x0104);
    T_TRUE(hidpp_brightness_info(dev)->hasState);
    T_EQ(f->requests, 0);
    fake_free(f);
    return 0;
}

static const pf_test_t brightness_tests[] = {
    { test_brightness_info, "info" },
    { test_brightness_info_v0, "info_v0" },
    { test_brightness_get_set, "get_set" },
    { test_brightness_events, "events" },
    { test_brightness_unsupported_and_cache, "unsupported_and_cache" },
    { 0 },
};

const pf_suite_t suite_brightness = { "brightness", 1, brightness_tests };

/* ------------------------------- led --------------------------------- */

/* zone 0: primary with disabled, fixed and cycling; zone 1: logo with
   fixed and breathing */
static const uint16_t color_led_effects[2][3] = { { 0, 1, 3 }, { 1, 10 } };
static const uint8_t color_led_counts[2] = { 3, 2 };

static int color_led_handler(
    fake_t *f, uint8_t func, const hidpp_packet_t *req, hidpp_packet_t *res
) {
    (void)f;
    const uint8_t *q = req->params;
    uint8_t *r = res->params;

    if (func != 0 && func != 4 && func != 5 && func != 7 && func != 8
        && func != 11 && func != 13 && q[0] >= 2)
        return HIDPP_ERR_INVALID_ARGUMENT;

    switch (func) {
    case 0: /* 2 zones, getZoneEffect and setLedBinInfo */
        memcpy(r, "\x02\x00\x07\x00\x05", 5);
        return 0;
    case 1:
        r[0] = q[0];
        r[2] = q[0] ? 0x02 : 0x01;
        r[3] = color_led_counts[q[0]];
        r[4] = 0x01;
        return 0;
    case 2:
        if (q[1] >= color_led_counts[q[0]])
            return HIDPP_ERR_INVALID_ARGUMENT;
        r[0] = q[0];
        r[1] = q[1];
        r[3] = (uint8_t)color_led_effects[q[0]][q[1]];
        r[5] = 0x01;
        r[6] = 0x03, r[7] = 0xE8;
        return 0;
    case 4:
        memcpy(r, q, 2);
        memcpy(&r[2], "\x01\xAA\xBB", 3);
        return 0;
    case 6:
        memcpy(r, q, 2);
        memcpy(&r[2], "\x01\x02\x03\x04\x05\x06\x07\x08", 8);
        return 0;
    case 7:
        r[0] = 1;
        return 0;
    case 9:
        r[0] = q[0];
        memcpy(&r[1], "\x10\x20\x30\x07\xD0\x50\x02", 7);
        return 0;
    case 12:
        r[0] = q[0];
        memcpy(&r[1], "\x0A\x14\x1E", 3);
        return 0;
    case 14:
        r[0] = q[0];
        r[1] = 1;
        for (int k = 0; k < 10; k++)
            r[2 + k] = (uint8_t)(k + 1);
        return 0;
    default:
        return func <= 15 ? 0 : HIDPP_ERR_INVALID_FUNCTION_ID;
    }
}

/* cluster 0: logo with fixed and wave; all clusters: cycling */
static int rgb_handler(
    fake_t *f, uint8_t func, const hidpp_packet_t *req, hidpp_packet_t *res
) {
    (void)f;
    const uint8_t *q = req->params;
    uint8_t *r = res->params;

    switch (func) {
    case 0:
        r[0] = q[0];
        r[1] = q[1];

        if (q[2] == 1) { /* onboard effect information */
            r[2] = 1;
            for (int k = 0; k < 13; k++)
                r[3 + k] = (uint8_t)(0x10 + k);
            return 0;
        }

        if (q[0] == 0xFF && q[1] == 0xFF) {
            /* 1 cluster, shutdown and changed events, 1 multi effect */
            memcpy(&r[2], "\x01\x00\x41\x00\x62\x01", 6);
            return 0;
        }

        if (q[0] != 0 && q[0] != 0xFF)
            return HIDPP_ERR_INVALID_ARGUMENT;

        if (q[1] == 0xFF) {
            memcpy(&r[2], "\x00\x02\x02\x01\x01\x01", 6);
            return 0;
        }

        if (q[1] >= (q[0] == 0xFF ? 1 : 2))
            return HIDPP_ERR_INVALID_ARGUMENT;

        r[3] = q[0] == 0xFF ? 3 : (q[1] ? 4 : 1);
        return 0;
    case 3:
        memcpy(r, q, 3);
        memcpy(&r[3], "\x02\x01\x02\x03\x04\x05\x06", 7);
        return 0;
    case 4:
        memcpy(r, q, 3);
        memcpy(&r[3], "\x11\x22\x33\x44\x55\x66\x77\x88", 8);
        return 0;
    case 5:
        memcpy(r, "\x00\x03\x05", 3);
        return 0;
    case 7:
        memcpy(r, "\x00\x00\x00\x00\x3C\xFF\xFF", 7);
        return 0;
    case 8:
        r[1] = HIDPP_LED_POWER_SAVE;
        return 0;
    default:
        return func <= 9 ? 0 : HIDPP_ERR_INVALID_FUNCTION_ID;
    }
}

static int led_index;

static fake_t *make_color_led(void) {
    fake_t *f = t_new();
    fake_add_firmware_info(f);
    led_index = fake_add_feature(f, 0x8070, 3, color_led_handler);
    return f;
}

static fake_t *make_rgb(void) {
    fake_t *f = t_new();
    fake_add_firmware_info(f);
    led_index = fake_add_feature(f, 0x8071, 4, rgb_handler);
    return f;
}

static int test_led_info_8070(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_color_led();
    hidpp_device_t *dev = t_open(f);
    struct hidpp_led_info *info = hidpp_led_info(dev);
    T_NOT_NULL(info);
    T_FALSE(info->isRgbEffects);
    T_EQ(info->version, 3);
    T_EQ(info->zoneCount, 2);
    T_EQ(info->nvCapabilities, 0x0007);
    T_TRUE(info->hasZoneEffect && info->hasSettings && info->canSetBin);
    T_TRUE(info->hasSync);
    T_FALSE(info->isMonochrome || info->hasShutdown);

    struct hidpp_led_zone *zone = hidpp_led_zone(dev, 1);
    T_NOT_NULL(zone);
    T_EQ(zone->location, 2);
    T_STR(zone->locationName, "Logo");
    T_EQ(zone->effectCount, 2);
    T_NULL(hidpp_led_zone(dev, 2));
    T_NULL(hidpp_led_zone(dev, HIDPP_LED_ALL_ZONES));

    struct hidpp_led_effect_info *effect = hidpp_led_effect_info(dev, 0, 2);
    T_NOT_NULL(effect);
    T_EQ(effect->id, HIDPP_LED_CYCLING);
    T_EQ(effect->capabilities, 1);
    T_EQ(effect->period, 1000);
    T_STR(effect->name, "Color cycling");
    T_NULL(hidpp_led_effect_info(dev, 0, 3));
    T_NULL(hidpp_led_effect_info(dev, HIDPP_LED_ALL_ZONES, 0));

    T_EQ(hidpp_led_find_effect(dev, 1, HIDPP_LED_BREATHING), 1);
    T_ERR(hidpp_led_find_effect(dev, 1, HIDPP_LED_WAVE), HIDPP_ENOENT);
    T_ERR(hidpp_led_find_effect(dev, 5, 1), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

static int test_led_info_8071(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_rgb();
    hidpp_device_t *dev = t_open(f);
    struct hidpp_led_info *info = hidpp_led_info(dev);
    T_NOT_NULL(info);
    T_TRUE(info->isRgbEffects);
    T_EQ(info->zoneCount, 1);
    T_EQ(info->nvCapabilities, 0x0041);
    T_EQ(info->multiZoneEffects, 1);
    T_TRUE(info->hasShutdown && info->hasChangedEvents && info->hasSync);
    T_FALSE(info->hasZoneEffect || info->hasSettings || info->canSetBin);

    struct hidpp_led_zone *zone = hidpp_led_zone(dev, 0);
    T_NOT_NULL(zone);
    T_STR(zone->locationName, "Logo");
    T_EQ(zone->effectCount, 2);
    T_EQ(zone->displayPersistency, 1);
    T_EQ(zone->effectPersistency, 1);
    T_TRUE(zone->hasPatterns);

    T_EQ(hidpp_led_effect_info(dev, 0, 1)->id, HIDPP_LED_WAVE);
    T_EQ(
        hidpp_led_effect_info(dev, HIDPP_LED_ALL_ZONES, 0)->id,
        HIDPP_LED_CYCLING
    );
    T_EQ(hidpp_led_find_effect(dev, HIDPP_LED_ALL_ZONES, HIDPP_LED_CYCLING), 0);
    fake_free(f);
    return 0;
}

static int test_led_pack(int seed, int i) {
    (void)seed, (void)i;
    uint8_t raw[10];
    struct hidpp_led_params p = { .red = 1,
                                  .green = 2,
                                  .blue = 3,
                                  .red2 = 4,
                                  .green2 = 5,
                                  .blue2 = 6,
                                  .period = 0x1234,
                                  .speed = 7,
                                  .intensity = 80,
                                  .waveform = 1,
                                  .direction = 3,
                                  .ramp = 2,
                                  .delay = 0x0102 };

    T_OK(hidpp_led_pack(HIDPP_LED_FIXED, &p, raw));
    T_MEM(raw, "\x01\x02\x03\x02\0\0\0\0\0\0", 10);
    T_OK(hidpp_led_pack(HIDPP_LED_CYCLING, &p, raw));
    T_MEM(raw, "\0\0\0\0\0\x12\x34\x50\0\0", 10);
    T_OK(hidpp_led_pack(HIDPP_LED_WAVE, &p, raw));
    T_MEM(raw, "\x01\x02\x03\x04\x05\x06\x34\x03\x50\x12", 10);
    T_OK(hidpp_led_pack(HIDPP_LED_LIGHT_ON_PRESS, &p, raw));
    T_MEM(raw, "\x01\x02\x03\x04\x05\x06\x01\x02\0\0", 10);
    T_OK(hidpp_led_pack(HIDPP_LED_BREATHING, &p, raw));
    T_MEM(raw, "\x01\x02\x03\x12\x34\x01\x50\0\0\0", 10);
    T_OK(hidpp_led_pack(HIDPP_LED_RIPPLE, &p, raw));
    T_MEM(raw, "\x01\x02\x03\0\x12\x34\0\0\0\0", 10);

    /* without a 16-bit period, the speed takes its LSB */
    p.period = 0;
    T_OK(hidpp_led_pack(HIDPP_LED_CYCLING, &p, raw));
    T_MEM(raw, "\0\0\0\0\0\0\x07\x50\0\0", 10);

    p.period = 255; /* indistinguishable from a speed */
    T_ERR(hidpp_led_pack(HIDPP_LED_WAVE, &p, raw), HIDPP_EINVAL);
    T_ERR(hidpp_led_pack(0x99, &p, raw), HIDPP_EINVAL);
    T_ERR(hidpp_led_pack(HIDPP_LED_FIXED, NULL, raw), HIDPP_EINVAL);
    return 0;
}

static int test_led_unpack_roundtrip(int seed, int i) {
    (void)i;
    uint32_t rng = (uint32_t)seed | 1;
    const uint16_t ids[] = { HIDPP_LED_FIXED,     HIDPP_LED_PULSING,
                             HIDPP_LED_CYCLING,   HIDPP_LED_WAVE,
                             HIDPP_LED_STARLIGHT, HIDPP_LED_LIGHT_ON_PRESS,
                             HIDPP_LED_BREATHING, HIDPP_LED_RIPPLE };

    for (int n = 0; n < 200; n++) {
        uint16_t id = ids[t_rand(&rng) % 8];
        uint8_t raw[10], back[10];
        for (int k = 0; k < 10; k++)
            raw[k] = (uint8_t)t_rand(&rng);

        struct hidpp_led_params p;
        T_OK(hidpp_led_unpack(id, raw, &p));
        T_OK(hidpp_led_pack(id, &p, back));
        T_OK(hidpp_led_unpack(id, back, &p));

        /* what an effect uses survives, everything else becomes zero */
        uint8_t again[10];
        T_OK(hidpp_led_pack(id, &p, again));
        T_MEM(again, back, 10);
    }
    return 0;
}

static int test_led_set_effect_8070(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_color_led();
    hidpp_device_t *dev = t_open(f);
    struct hidpp_led_effect e = { .zone = 1,
                                  .index = 1,
                                  .persistence = HIDPP_LED_VOLATILE
                                      | HIDPP_LED_NON_VOLATILE };
    for (int k = 0; k < 10; k++)
        e.params[k] = (uint8_t)(0xA0 + k);

    T_OK(hidpp_led_set_effect(dev, &e));
    T_EQ(T_FUNC(f), 3);
    T_EQ(T_PARAM(f, 0), 1);
    T_EQ(T_PARAM(f, 1), 1);
    T_MEM(&fake_last(f)->raw[6], e.params, 10);
    T_EQ(T_PARAM(f, 12), 1); /* RAM and EEPROM */

    e.persistence = HIDPP_LED_VOLATILE;
    T_OK(hidpp_led_set_effect(dev, &e));
    T_EQ(T_PARAM(f, 12), 0);
    e.persistence = HIDPP_LED_NON_VOLATILE;
    T_OK(hidpp_led_set_effect(dev, &e));
    T_EQ(T_PARAM(f, 12), 2);

    unsigned before = f->requests;
    e.index = 2;
    T_ERR(hidpp_led_set_effect(dev, &e), HIDPP_EINVAL);
    e.index = 0, e.zone = 2;
    T_ERR(hidpp_led_set_effect(dev, &e), HIDPP_EINVAL);
    e.zone = 0, e.persistence = 0;
    T_ERR(hidpp_led_set_effect(dev, &e), HIDPP_EINVAL);
    e.persistence = HIDPP_LED_VOLATILE, e.powerMode = 1;
    T_ERR(hidpp_led_set_effect(dev, &e), HIDPP_EINVAL);
    T_EQ(f->requests, before);
    fake_free(f);
    return 0;
}

static int test_led_set_effect_8071(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_rgb();
    hidpp_device_t *dev = t_open(f);
    struct hidpp_led_effect e = { .zone = 0,
                                  .index = 1,
                                  .persistence = HIDPP_LED_NON_VOLATILE,
                                  .powerMode = 1 };
    e.params[0] = 0xFF;

    T_OK(hidpp_led_set_effect(dev, &e));
    T_EQ(T_FUNC(f), 1);
    T_EQ(T_PARAM(f, 2), 0xFF);
    T_EQ(T_PARAM(f, 12), 0x02 | (1 << 2));

    e.zone = HIDPP_LED_ALL_ZONES, e.index = 0;
    e.persistence = HIDPP_LED_VOLATILE, e.powerMode = 0;
    T_OK(hidpp_led_set_effect(dev, &e));
    T_EQ(T_PARAM(f, 0), 0xFF);
    T_EQ(T_PARAM(f, 12), 0x01);

    e.powerMode = 2;
    T_ERR(hidpp_led_set_effect(dev, &e), HIDPP_EINVAL);
    e.powerMode = 0, e.index = 1;
    T_ERR(hidpp_led_set_effect(dev, &e), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

static int test_led_get_effect(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_color_led();
    hidpp_device_t *dev = t_open(f);
    struct hidpp_led_effect e;
    T_OK(hidpp_led_get_effect(dev, 1, HIDPP_LED_NON_VOLATILE, &e));
    T_EQ(T_FUNC(f), 14);
    T_EQ(T_PARAM(f, 1), 1);
    T_EQ(e.zone, 1);
    T_EQ(e.index, 1);
    T_EQ(e.persistence, HIDPP_LED_NON_VOLATILE);
    T_MEM(e.params, "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A", 10);
    T_ERR(hidpp_led_get_effect(dev, 1, 3, &e), HIDPP_EINVAL);
    fake_free(f);

    f = make_rgb();
    dev = t_open(f);
    T_ERR(hidpp_led_get_effect(dev, 0, HIDPP_LED_VOLATILE, &e), HIDPP_ENOSYS);
    fake_free(f);
    return 0;
}

static int test_led_nv_config(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_color_led();
    hidpp_device_t *dev = t_open(f);
    struct hidpp_led_nv nv;
    T_OK(hidpp_led_nv_get(dev, HIDPP_LED_NV_DEMO, &nv));
    T_EQ(T_FUNC(f), 4);
    T_EQ(nv.state, 1);
    T_EQ(nv.params[0], 0xAA);
    T_EQ(nv.params[1], 0xBB);

    nv = (struct hidpp_led_nv) { .capability = HIDPP_LED_NV_BOOT_UP,
                                 .state = 2,
                                 .params = { 9, 8 } };
    T_OK(hidpp_led_nv_set(dev, &nv));
    T_EQ(T_FUNC(f), 5);
    T_MEM(&fake_last(f)->raw[4], "\x00\x01\x02\x09\x08", 5);

    nv.params[2] = 1; /* only two parameters */
    T_ERR(hidpp_led_nv_set(dev, &nv), HIDPP_EINVAL);
    T_ERR(hidpp_led_nv_get(dev, HIDPP_LED_NV_SHUTDOWN, &nv), HIDPP_EINVAL);
    T_ERR(hidpp_led_nv_get(dev, 0x0003, &nv), HIDPP_EINVAL);
    fake_free(f);

    f = make_rgb();
    dev = t_open(f);
    T_OK(hidpp_led_nv_get(dev, HIDPP_LED_NV_SHUTDOWN, &nv));
    T_EQ(T_FUNC(f), 3);
    T_EQ(T_PARAM(f, 0), 0); /* get */
    T_EQ(nv.state, 2);
    T_MEM(nv.params, "\x01\x02\x03\x04\x05\x06", 6);

    nv = (struct hidpp_led_nv) { .capability = HIDPP_LED_NV_SHUTDOWN,
                                 .state = 1,
                                 .params = { 1, 2, 3, 4, 5, 6 } };
    T_OK(hidpp_led_nv_set(dev, &nv));
    T_MEM(
        &fake_last(f)->raw[4], "\x01\x00\x40\x01\x01\x02\x03\x04\x05\x06", 10
    );
    fake_free(f);
    return 0;
}

static int test_led_sw_control(int seed, int i) {
    (void)seed, (void)i;
    uint8_t control, notify;
    fake_t *f = make_color_led();
    hidpp_device_t *dev = t_open(f);
    T_OK(hidpp_led_sw_control_get(dev, &control, &notify));
    T_EQ(control, HIDPP_LED_SW_EFFECTS);
    T_EQ(notify, 0);
    T_OK(hidpp_led_sw_control_set(dev, 1, HIDPP_LED_NOTIFY_SYNC));
    T_EQ(T_FUNC(f), 8);
    T_EQ(T_PARAM(f, 1), 1);
    T_ERR(
        hidpp_led_sw_control_set(dev, HIDPP_LED_SW_POWER_MODES, 0), HIDPP_EINVAL
    );
    T_ERR(
        hidpp_led_sw_control_set(dev, 0, HIDPP_LED_NOTIFY_ACTIVITY),
        HIDPP_EINVAL
    );
    fake_free(f);

    f = make_rgb();
    dev = t_open(f);
    T_OK(hidpp_led_sw_control_get(dev, &control, &notify));
    T_EQ(control, HIDPP_LED_SW_EFFECTS | HIDPP_LED_SW_POWER_MODES);
    T_EQ(notify, HIDPP_LED_NOTIFY_SYNC | HIDPP_LED_NOTIFY_TIMEOUT);
    T_OK(hidpp_led_sw_control_set(dev, 2, 6));
    T_EQ(T_FUNC(f), 5);
    T_MEM(&fake_last(f)->raw[4], "\x01\x02\x06", 3);
    T_ERR(hidpp_led_sw_control_set(dev, 4, 0), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

static int test_led_bin_and_sync(int seed, int i) {
    (void)seed, (void)i;
    struct hidpp_led_bin bin;
    fake_t *f = make_color_led();
    hidpp_device_t *dev = t_open(f);
    T_OK(hidpp_led_bin_get(dev, 1, 4, 0, &bin));
    T_EQ(T_FUNC(f), 6);
    T_MEM(bin.data, "\x01\x02\x03\x04\x05\x06\x07\x08", 8);
    T_ERR(hidpp_led_bin_get(dev, 1, 4, 1, &bin), HIDPP_ENOSYS);
    T_ERR(hidpp_led_bin_get(dev, 1, 6, 0, &bin), HIDPP_EINVAL);
    T_OK(hidpp_led_bin_set(dev, &bin));
    T_EQ(T_FUNC(f), 15);
    T_EQ(T_PARAM(f, 1), 4);
    T_OK(hidpp_led_sync(dev, HIDPP_LED_ALL_ZONES, -2));
    T_EQ(T_FUNC(f), 13);
    T_MEM(&fake_last(f)->raw[4], "\xFF\x00\xFF\xFE", 4);
    fake_free(f);

    f = make_rgb();
    dev = t_open(f);
    T_OK(hidpp_led_bin_get(dev, 0, 3, 1, &bin));
    T_EQ(T_FUNC(f), 4);
    T_EQ(T_PARAM(f, 0), 2); /* backup */
    T_MEM(bin.data, "\x11\x22\x33\x44\x55\x66\x77\x88", 8);
    T_OK(hidpp_led_bin_set(dev, &bin));
    T_MEM(&fake_last(f)->raw[4], "\x01\x00\x03\x11\x22", 5);
    T_OK(hidpp_led_sync(dev, 0, 300));
    T_EQ(T_FUNC(f), 6);
    T_EQ(HIDPP_WORD(T_PARAM(f, 2), T_PARAM(f, 3)), 300);
    fake_free(f);
    return 0;
}

static int test_led_color_led_only(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_color_led();
    hidpp_device_t *dev = t_open(f);
    struct hidpp_led_settings s;
    T_OK(hidpp_led_settings(dev, 0, HIDPP_LED_VOLATILE, &s));
    T_EQ(T_FUNC(f), 9);
    T_EQ(s.red, 0x10);
    T_EQ(s.blue, 0x30);
    T_EQ(s.period, 2000);
    T_EQ(s.brightness, 80);
    T_EQ(s.param, 2);
    T_OK(hidpp_led_clear_settings(dev, 1));
    T_EQ(T_FUNC(f), 10);
    T_OK(hidpp_led_cycling_direction(dev, 1));
    T_EQ(T_FUNC(f), 11);

    uint8_t rgb[3];
    T_OK(hidpp_led_current_color(dev, 1, rgb));
    T_EQ(T_FUNC(f), 12);
    T_MEM(rgb, "\x0A\x14\x1E", 3);

    T_ERR(hidpp_led_set_pattern(dev, 0, 1), HIDPP_ENOSYS);
    T_ERR(hidpp_led_power_mode(dev), HIDPP_ENOSYS);
    T_ERR(hidpp_led_shutdown(dev), HIDPP_ENOSYS);
    fake_free(f);
    return 0;
}

static int test_led_rgb_only(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_rgb();
    hidpp_device_t *dev = t_open(f);

    T_OK(hidpp_led_set_pattern(dev, 0, 0x03));
    T_EQ(T_FUNC(f), 2);
    T_EQ(T_PARAM(f, 1), 3);

    struct hidpp_led_power_config cfg;
    T_OK(hidpp_led_power_config_get(dev, &cfg));
    T_EQ(cfg.toPowerSave, 60);
    T_EQ(cfg.toOff, 0xFFFF);
    cfg.toOff = 600;
    T_OK(hidpp_led_power_config_set(dev, &cfg));
    T_MEM(&fake_last(f)->raw[4], "\x01\x00\x00\x00\x3C\x02\x58", 7);

    T_EQ(hidpp_led_power_mode(dev), HIDPP_LED_POWER_SAVE);
    T_OK(hidpp_led_set_power_mode(dev, HIDPP_LED_POWER_OFF));
    T_MEM(&fake_last(f)->raw[4], "\x01\x03", 2);
    T_ERR(hidpp_led_set_power_mode(dev, 0), HIDPP_EINVAL);

    T_OK(hidpp_led_shutdown(dev));
    T_EQ(T_FUNC(f), 9);

    uint8_t onboard[13];
    T_OK(hidpp_led_onboard_info(dev, 2, 1, 4, onboard));
    T_MEM(&fake_last(f)->raw[4], "\xFF\x02\x01\x01\x04", 5);
    T_EQ(onboard[0], 0x10);
    T_EQ(onboard[12], 0x1C);
    T_ERR(hidpp_led_onboard_info(dev, 2, 1, 7, onboard), HIDPP_EINVAL);

    struct hidpp_led_settings s;
    T_ERR(hidpp_led_settings(dev, 0, HIDPP_LED_VOLATILE, &s), HIDPP_ENOSYS);
    T_ERR(hidpp_led_cycling_direction(dev, 0), HIDPP_ENOSYS);
    fake_free(f);
    return 0;
}

static int test_led_events(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_color_led();
    hidpp_device_t *dev = t_open(f);
    const uint8_t sync[] = { 1, 0x01, 0xF4 };
    struct hidpp_event e = t_event(f, dev, led_index, 0, sync, 3);
    T_EQ(e.type, HIDPP_EVENT_LED);
    T_EQ(e.as.led.kind, HIDPP_LED_EVENT_SYNC);
    T_EQ(e.as.led.zone, 1);
    T_EQ(e.as.led.counter, 500);

    /* 0x8070 has a single event */
    e = t_event(f, dev, led_index, 1, sync, 3);
    T_EQ(e.type, HIDPP_EVENT_UNKNOWN);
    fake_free(f);

    f = make_rgb();
    dev = t_open(f);
    e = t_event(f, dev, led_index, 1, (const uint8_t[]) { 1 }, 1);
    T_EQ(e.as.led.kind, HIDPP_LED_EVENT_ACTIVITY);
    T_EQ(e.as.led.activity, 1);

    uint8_t changed[13] = { 0, 1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 0x07 };
    e = t_event(f, dev, led_index, 2, changed, sizeof(changed));
    T_EQ(e.as.led.kind, HIDPP_LED_EVENT_CHANGED);
    T_EQ(e.as.led.zone, 0);
    T_EQ(e.as.led.effect, 1);
    T_MEM(e.as.led.params, &changed[2], 10);
    T_EQ(e.as.led.persistence, 3);
    T_EQ(e.as.led.powerMode, 1);
    fake_free(f);
    return 0;
}

static int test_led_unsupported(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = t_new();
    hidpp_device_t *dev = t_open(f);
    struct hidpp_led_effect e = { 0 };
    T_NULL(hidpp_led_info(dev));
    T_NULL(hidpp_led_zone(dev, 0));
    T_ERR(hidpp_led_set_effect(dev, &e), HIDPP_ENOSYS);
    T_ERR(hidpp_led_power_mode(dev), HIDPP_ENOSYS);
    T_ERR(hidpp_led_set_effect(NULL, &e), HIDPP_EINVAL);
    T_STR(hidpp_led_effect_name(HIDPP_LED_RIPPLE), "Ripple");
    T_STR(hidpp_led_effect_name(0x1234), "Unknown");
    fake_free(f);
    return 0;
}

static int test_led_cache(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = t_from_cache(make_rgb, &f);
    T_EQ(hidpp_led_info(dev)->multiZoneEffects, 1);
    T_TRUE(hidpp_led_info(dev)->hasShutdown);
    T_STR(hidpp_led_zone(dev, 0)->locationName, "Logo");
    T_STR(hidpp_led_effect_info(dev, 0, 1)->name, "Color wave");
    T_EQ(hidpp_led_effect_info(dev, HIDPP_LED_ALL_ZONES, 0)->id, 3);
    T_EQ(f->requests, 0);
    fake_free(f);

    dev = t_from_cache(make_color_led, &f);
    T_EQ(hidpp_led_zone(dev, 1)->effectCount, 2);
    T_EQ(hidpp_led_effect_info(dev, 0, 2)->period, 1000);
    T_TRUE(hidpp_led_info(dev)->hasZoneEffect);
    T_EQ(f->requests, 0);
    fake_free(f);
    return 0;
}

static const pf_test_t led_tests[] = {
    { test_led_info_8070, "info_8070" },
    { test_led_info_8071, "info_8071" },
    { test_led_pack, "pack" },
    { test_led_unpack_roundtrip, "unpack_roundtrip" },
    { test_led_set_effect_8070, "set_effect_8070" },
    { test_led_set_effect_8071, "set_effect_8071" },
    { test_led_get_effect, "get_effect" },
    { test_led_nv_config, "nv_config" },
    { test_led_sw_control, "sw_control" },
    { test_led_bin_and_sync, "bin_and_sync" },
    { test_led_color_led_only, "color_led_only" },
    { test_led_rgb_only, "rgb_only" },
    { test_led_events, "events" },
    { test_led_unsupported, "unsupported" },
    { test_led_cache, "cache" },
    { 0 },
};

const pf_suite_t suite_led = { "led", 1, led_tests };

/* ------------------------------ perkey ------------------------------- */

static int perkey_handler(
    fake_t *f, uint8_t func, const hidpp_packet_t *req, hidpp_packet_t *res
) {
    (void)f;

    if (func > 7)
        return HIDPP_ERR_INVALID_FUNCTION_ID;

    if (func != 0)
        return 0;

    /* zones 1-10 and 100, then 120 */
    res->params[0] = req->params[0];
    res->params[1] = req->params[1];

    if (req->params[1] == 0) {
        res->params[2] = 0xFE;
        res->params[3] = 0x07;
        res->params[2 + 100 / 8] = 1u << (100 % 8);
    } else if (req->params[1] == 1) {
        res->params[2 + (120 - 112) / 8] = 1u << (120 % 8);
    } else if (req->params[1] > 2) {
        return HIDPP_ERR_INVALID_ARGUMENT;
    }

    return 0;
}

static int perkey_index;

static fake_t *make_perkey(void) {
    fake_t *f = t_new();
    fake_add_firmware_info(f);
    perkey_index = fake_add_feature(f, 0x8081, 0, perkey_handler);
    return f;
}

static hidpp_device_t *open_perkey(fake_t **out) {
    fake_t *f = make_perkey();
    hidpp_device_t *dev = t_open(f);
    T_EQ(hidpp_perkey_has_zone(dev, 1), 1);
    fake_reset_log(f);
    *out = f;
    return dev;
}

static int test_perkey_zones(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_perkey();
    hidpp_device_t *dev = t_open(f);
    uint8_t zones[16];
    T_EQ(hidpp_perkey_zones(dev, zones, 16), 12);
    T_EQ(zones[0], 1);
    T_EQ(zones[9], 10);
    T_EQ(zones[10], 100);
    T_EQ(zones[11], 120);
    T_EQ(hidpp_perkey_zones(dev, zones, 2), 12);
    T_EQ(hidpp_perkey_zones(dev, NULL, 0), 12);

    T_EQ(hidpp_perkey_has_zone(dev, 100), 1);
    T_EQ(hidpp_perkey_has_zone(dev, 11), 0);
    T_EQ(hidpp_perkey_has_zone(dev, 0), 0);
    T_EQ(hidpp_perkey_has_zone(dev, 255), 0);
    fake_free(f);
    return 0;
}

static int test_perkey_set(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_perkey(&f);
    const uint8_t ids[] = { 1, 5, 9, 2, 100, 7 };
    struct hidpp_color colors[6];
    for (int k = 0; k < 6; k++)
        colors[k] = (struct hidpp_color) { (uint8_t)k, 0x80, 0xFF };

    T_OK(hidpp_perkey_set(dev, ids, colors, 6));
    T_EQ(f->requests, 2);
    T_EQ(T_FUNC(f), 1);
    T_MEM(
        &fake_logged(f, 0)->raw[4],
        "\x01\x00\x80\xFF\x05\x01\x80\xFF\x09\x02\x80\xFF\x02\x03\x80\xFF",
        16
    );
    T_MEM(&fake_last(f)->raw[4], "\x64\x04\x80\xFF\x07\x05\x80\xFF\x00", 9);

    const uint8_t bad[] = { 3, 255 };
    T_ERR(hidpp_perkey_set(dev, bad, colors, 2), HIDPP_EINVAL);
    T_EQ(f->requests, 2);
    fake_free(f);
    return 0;
}

static int test_perkey_set_consecutive(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_perkey(&f);
    struct hidpp_color colors[7];
    for (int k = 0; k < 7; k++)
        colors[k] = (struct hidpp_color) { (uint8_t)(k + 1), 0, 0 };

    T_OK(hidpp_perkey_set_consecutive(dev, 3, colors, 7));
    T_EQ(f->requests, 2);
    T_EQ(fake_logged(f, 0)->raw[3] >> 4, 2);
    T_MEM(&fake_logged(f, 0)->raw[4], "\x03\x01\x00\x00\x02\x00\x00", 7);
    T_EQ(fake_logged(f, 0)->raw[4 + 13], 5);

    /* the two remaining zones are not padded with black */
    T_EQ(T_FUNC(f), 1);
    T_MEM(&fake_last(f)->raw[4], "\x08\x06\x00\x00\x09\x07\x00\x00\x00", 9);

    T_ERR(hidpp_perkey_set_consecutive(dev, 250, colors, 7), HIDPP_EINVAL);
    T_ERR(hidpp_perkey_set_consecutive(dev, 0, colors, 1), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

static int test_perkey_set_range_and_color(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_perkey(&f);
    struct hidpp_color c = { 1, 2, 3 };

    T_OK(hidpp_perkey_set_range(dev, 2, 9, c));
    T_EQ(T_FUNC(f), 5);
    T_MEM(&fake_last(f)->raw[4], "\x02\x09\x01\x02\x03\x00", 6);
    T_OK(hidpp_perkey_set_range(dev, 4, 4, c));
    T_EQ(T_FUNC(f), 1);
    T_ERR(hidpp_perkey_set_range(dev, 9, 2, c), HIDPP_EINVAL);
    T_ERR(hidpp_perkey_set_range(dev, 0, 2, c), HIDPP_EINVAL);

    uint8_t ids[15];
    for (int k = 0; k < 15; k++)
        ids[k] = (uint8_t)(k + 1);

    fake_reset_log(f);
    T_OK(hidpp_perkey_set_color(dev, ids, 15, c));
    T_EQ(f->requests, 2);
    T_EQ(T_FUNC(f), 6);
    T_MEM(&fake_logged(f, 0)->raw[4], "\x01\x02\x03\x01\x02", 5);
    T_EQ(fake_logged(f, 0)->raw[4 + 15], 13);
    T_MEM(&fake_last(f)->raw[4], "\x01\x02\x03\x0E\x0F\x00", 6);
    fake_free(f);
    return 0;
}

static int test_perkey_update_delta4(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_perkey(&f);
    struct hidpp_color prev[10], cur[10];

    for (int k = 0; k < 10; k++) {
        prev[k] = (struct hidpp_color) { 10, 0, 255 };
        cur[k] = (struct hidpp_color) { 11, 255, 0 }; /* +1, -1, +1 */
    }

    T_EQ(hidpp_perkey_update(dev, 1, prev, cur, 10), 1);
    T_EQ(T_FUNC(f), 4);
    T_EQ(T_PARAM(f, 0), 1);
    /* nibbles 1 F 1 1 F 1 ... */
    T_MEM(&fake_last(f)->raw[5], "\x1F\x11\xF1\x1F\x11\xF1", 6);
    fake_free(f);
    return 0;
}

static int test_perkey_update_delta5(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_perkey(&f);
    struct hidpp_color prev[8] = { 0 }, cur[8] = { 0 };
    cur[0] = (struct hidpp_color) { 10, 240, 15 }; /* +10, -16, +15 */
    cur[7] = (struct hidpp_color) { 0, 0, 1 };

    T_EQ(hidpp_perkey_update(dev, 2, prev, cur, 8), 1);
    T_EQ(T_FUNC(f), 3);
    T_EQ(T_PARAM(f, 0), 2);
    /* 01010 10000 01111 00000... */
    T_EQ(T_PARAM(f, 1), 0x54);
    T_EQ(T_PARAM(f, 2), 0x1E);
    T_EQ(T_PARAM(f, 15), 0x01); /* the last 5 bits are the last blue */
    fake_free(f);
    return 0;
}

static int test_perkey_update_absolute(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_perkey(&f);
    struct hidpp_color prev[10] = { 0 }, cur[10] = { 0 };

    /* five large changes in a row use one consecutive request */
    for (int k = 0; k < 5; k++)
        cur[k] = (struct hidpp_color) { 200, 100, (uint8_t)k };

    T_EQ(hidpp_perkey_update(dev, 1, prev, cur, 10), 1);
    T_EQ(T_FUNC(f), 2);
    T_EQ(T_PARAM(f, 0), 1);
    T_MEM(&fake_last(f)->raw[5], "\xC8\x64\x00\xC8\x64\x01", 6);

    /* scattered ones are sent individually */
    fake_reset_log(f);
    memset(cur, 0, sizeof(cur));
    cur[1] = (struct hidpp_color) { 100, 0, 0 };
    cur[6] = (struct hidpp_color) { 0, 100, 0 };
    T_EQ(hidpp_perkey_update(dev, 1, prev, cur, 10), 1);
    T_EQ(T_FUNC(f), 1);
    T_MEM(&fake_last(f)->raw[4], "\x02\x64\x00\x00\x07\x00\x64\x00\x00", 9);

    /* nothing to do */
    fake_reset_log(f);
    T_EQ(hidpp_perkey_update(dev, 1, prev, prev, 10), 0);
    T_EQ(f->requests, 0);
    fake_free(f);
    return 0;
}

static int test_perkey_update_stays_on_the_device(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_perkey(&f);
    struct hidpp_color prev[3] = { 0 }, cur[3] = { 0 };
    cur[2] = (struct hidpp_color) { 1, 1, 1 };

    /* a delta window from zone 120 would pass the last zone */
    T_EQ(hidpp_perkey_update(dev, 118, prev, cur, 3), 1);
    T_EQ(T_FUNC(f), 1);
    T_MEM(&fake_last(f)->raw[4], "\x78\x01\x01\x01\x00", 5);

    T_ERR(hidpp_perkey_update(dev, 250, prev, cur, 6), HIDPP_EINVAL);
    T_ERR(hidpp_perkey_update(dev, 1, NULL, cur, 3), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

static int test_perkey_frame_end(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_perkey(&f);
    T_OK(hidpp_perkey_frame_end(dev, 1, 0x0102, 3));
    T_EQ(T_FUNC(f), 7);
    T_MEM(&fake_last(f)->raw[4], "\x01\x01\x02\x00\x03", 5);
    T_ERR(hidpp_perkey_frame_end(dev, 2, 0, 0), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

static int test_perkey_unsupported_and_cache(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = t_new();
    hidpp_device_t *dev = t_open(f);
    uint8_t zones[4];
    T_ERR(hidpp_perkey_zones(dev, zones, 4), HIDPP_ENOSYS);
    T_ERR(hidpp_perkey_frame_end(dev, 0, 0, 0), HIDPP_ENOSYS);
    T_ERR(hidpp_perkey_has_zone(NULL, 1), HIDPP_EINVAL);
    fake_free(f);

    dev = t_from_cache(make_perkey, &f);
    T_EQ(hidpp_perkey_zones(dev, zones, 4), 12);
    T_EQ(hidpp_perkey_has_zone(dev, 120), 1);
    T_EQ(f->requests, 0);
    fake_free(f);
    return 0;
}

static const pf_test_t perkey_tests[] = {
    { test_perkey_zones, "zones" },
    { test_perkey_set, "set" },
    { test_perkey_set_consecutive, "set_consecutive" },
    { test_perkey_set_range_and_color, "set_range_and_color" },
    { test_perkey_update_delta4, "update_delta4" },
    { test_perkey_update_delta5, "update_delta5" },
    { test_perkey_update_absolute, "update_absolute" },
    { test_perkey_update_stays_on_the_device, "update_stays_on_device" },
    { test_perkey_frame_end, "frame_end" },
    { test_perkey_unsupported_and_cache, "unsupported_and_cache" },
    { 0 },
};

const pf_suite_t suite_perkey = { "perkey", 1, perkey_tests };
