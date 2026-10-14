/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

/*  Backlight (0x1982), versions 1 to 3. */

#include "test.h"

#define CACHE_CAPACITY (128 * 1024)

static hidpp_device_t *open_version(fake_t **out, uint8_t version) {
    fake_t *f = fake_new_bare();
    fake_add_backlight(f, version);
    *out = f;
    return t_open(f);
}

static hidpp_device_t *open_v3(fake_t **out) { return open_version(out, 3); }

static struct hidpp_backlight_config current(hidpp_device_t *dev) {
    struct hidpp_backlight_config c;
    T_OK(hidpp_backlight_get(dev, &c));
    return c;
}

static int test_info_v3(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_v3(&f);
    struct hidpp_backlight_info *info = hidpp_backlight_info(dev);
    T_NOT_NULL(info);
    T_TRUE(info->initialized);
    T_EQ(info->version, 3);
    T_EQ(info->levelCount, 8);
    T_EQ(info->effectList, 0x007F);
    T_TRUE(info->hasEffects && info->hasDurations);
    T_TRUE(info->supportsWow && info->supportsCrown && info->supportsPowerSave);
    T_TRUE(info->supportsAutoMode && info->supportsTempManualMode);
    T_TRUE(info->supportsPermManualMode);
    T_EQ(info->defaultDurationHandsOut, 30);
    T_EQ(info->defaultDurationHandsIn, 60);
    T_EQ(info->defaultDurationPowered, 300);
    fake_free(f);
    return 0;
}

static int test_info_supported_bits(int seed, int i) {
    (void)seed, (void)i;

    for (int bit = 0; bit < 6; bit++) {
        fake_t *f = fake_new_bare();
        fake_add_backlight(f, 3);
        f->blSupported = (uint8_t)(1u << bit);
        hidpp_device_t *dev = t_open(f);
        struct hidpp_backlight_info *info = hidpp_backlight_info(dev);
        T_NOT_NULL(info);
        T_EQ(info->supportsWow, bit == 0);
        T_EQ(info->supportsCrown, bit == 1);
        T_EQ(info->supportsPowerSave, bit == 2);
        T_EQ(info->supportsAutoMode, bit == 3);
        T_EQ(info->supportsTempManualMode, bit == 4);
        T_EQ(info->supportsPermManualMode, bit == 5);
        fake_free(f);
    }
    return 0;
}

static int test_info_older_versions(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_version(&f, 2);
    struct hidpp_backlight_info *info = hidpp_backlight_info(dev);
    T_NOT_NULL(info);
    T_TRUE(info->hasEffects);
    T_FALSE(info->hasDurations);
    T_EQ(info->effectList, 0x007F);
    T_EQ(info->defaultDurationHandsOut, 0);
    fake_free(f);

    dev = open_version(&f, 1);
    info = hidpp_backlight_info(dev);
    T_NOT_NULL(info);
    T_FALSE(info->hasEffects);
    T_EQ(info->effectList, 0);
    fake_free(f);
    return 0;
}

static int test_newer_versions_behave_like_v3(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_version(&f, 3);
    f->features[1].version = 4; /* getFeature reports a newer version */
    hidpp_cache_clear(dev);
    struct hidpp_backlight_info *info = hidpp_backlight_info(dev);
    T_NOT_NULL(info);
    T_EQ(info->version, 4);
    T_TRUE(info->hasDurations);
    fake_free(f);
    return 0;
}

static int test_get_v3(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_v3(&f);
    struct hidpp_backlight_config c = current(dev);
    T_TRUE(c.enabled);
    T_TRUE(c.wow);
    T_FALSE(c.crown);
    T_FALSE(c.powerSave);
    T_EQ(c.mode, HIDPP_BACKLIGHT_MODE_AUTO);
    T_EQ(c.effect, HIDPP_BACKLIGHT_EFFECT_BREATHING);
    T_EQ(c.level, 3);
    T_EQ(c.durationHandsOut, 30);
    T_EQ(c.durationHandsIn, 60);
    T_EQ(c.durationPowered, 0x0120 * 5);
    fake_free(f);
    return 0;
}

static int test_get_fields_come_from_the_right_bytes(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_v3(&f);
    f->blEnabled = 0;
    f->blOptions = 0x06 | (3 << 3); /* crown + pwrSave + permanent manual */
    f->blLevel = 7;
    f->blDur[0] = 0x0102;
    f->blDur[1] = 0x0001;
    f->blDur[2] = 0x05A0;
    struct hidpp_backlight_config c = current(dev);
    T_FALSE(c.enabled);
    T_FALSE(c.wow);
    T_TRUE(c.crown);
    T_TRUE(c.powerSave);
    T_EQ(c.mode, 3);
    T_EQ(c.level, 7);
    T_EQ(c.durationHandsOut, 0x0102 * 5); /* little endian, 5 s units */
    T_EQ(c.durationHandsIn, 5);
    T_EQ(c.durationPowered, 7200);
    fake_free(f);
    return 0;
}

static int test_get_v2_has_no_modes_or_durations(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_version(&f, 2);
    struct hidpp_backlight_config c = current(dev);
    T_TRUE(c.enabled);
    T_EQ(c.mode, 0);
    T_EQ(c.level, 0);
    T_EQ(c.durationHandsOut, 0);
    T_EQ(c.effect, HIDPP_BACKLIGHT_EFFECT_BREATHING);
    fake_free(f);
    return 0;
}

static int test_set_roundtrip_v3(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_v3(&f);
    struct hidpp_backlight_config c = current(dev);

    c.crown = HIDPP_TRUE;
    c.mode = HIDPP_BACKLIGHT_MODE_PERM_MANUAL;
    c.level = 5;
    c.effect = HIDPP_BACKLIGHT_EFFECT_WAVES;
    c.durationHandsOut = 300;
    c.durationHandsIn = 5;
    c.durationPowered = 7200;
    T_OK(hidpp_backlight_set(dev, &c));

    T_EQ(f->blEnabled, 1);
    T_EQ(f->blOptions, 0x01 | 0x02 | (3 << 3));
    T_EQ(f->blEffect, HIDPP_BACKLIGHT_EFFECT_WAVES);
    T_EQ(f->blLevel, 5);
    T_EQ(f->blDur[0], 60);
    T_EQ(f->blDur[1], 1);
    T_EQ(f->blDur[2], 0x05A0);

    struct hidpp_backlight_config back = current(dev);
    T_MEM(&back, &c, sizeof(c));
    fake_free(f);
    return 0;
}

static int test_set_unchanged_config_is_a_noop(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_v3(&f);
    struct hidpp_backlight_config c = current(dev);
    uint8_t options = f->blOptions, effect = f->blEffect, level = f->blLevel;
    uint16_t d0 = f->blDur[0], d1 = f->blDur[1], d2 = f->blDur[2];
    T_OK(hidpp_backlight_set(dev, &c));
    T_EQ(f->blOptions, options);
    T_EQ(f->blEffect, effect);
    T_EQ(f->blLevel, level);
    T_EQ(f->blDur[0], d0);
    T_EQ(f->blDur[1], d1);
    T_EQ(f->blDur[2], d2);
    fake_free(f);
    return 0;
}

static int test_set_wire_format_v3(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_v3(&f);
    struct hidpp_backlight_config c = current(dev);
    c.durationHandsOut = 0x0102 * 5;
    fake_reset_log(f);
    T_OK(hidpp_backlight_set(dev, &c));

    const struct fake_packet *p = fake_last(f);
    T_EQ(p->raw[0], HIDPP_KIND_LONG);
    T_EQ(p->raw[3] >> 4, 1);
    T_EQ(p->raw[4], 1);               /* enabled */
    T_EQ(p->raw[5], 0x01 | (1 << 3)); /* wow + auto */
    T_EQ(p->raw[6], HIDPP_BACKLIGHT_EFFECT_BREATHING);
    T_EQ(p->raw[7], 3);    /* level */
    T_EQ(p->raw[8], 0x02); /* LSB first */
    T_EQ(p->raw[9], 0x01);
    fake_free(f);
    return 0;
}

static int test_set_wire_format_v2(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_version(&f, 2);
    struct hidpp_backlight_config c = current(dev);
    c.effect = HIDPP_BACKLIGHT_EFFECT_STATIC;
    c.crown = HIDPP_TRUE;
    fake_reset_log(f);
    T_OK(hidpp_backlight_set(dev, &c));

    const struct fake_packet *p = fake_last(f);
    T_EQ(p->raw[0], HIDPP_KIND_SHORT);
    T_EQ(p->raw[4], 1);
    T_EQ(p->raw[5], 0x03);
    T_EQ(p->raw[6], 0);
    T_EQ(f->blEffect, 0);
    fake_free(f);
    return 0;
}

static int test_set_validation(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_v3(&f);
    struct hidpp_backlight_config base = current(dev), c;
    T_NOT_NULL(hidpp_backlight_info(dev));
    fake_reset_log(f);

    c = base;
    c.mode = 4;
    T_ERR(hidpp_backlight_set(dev, &c), HIDPP_EINVAL);
    c = base;
    c.mode = 0;
    T_ERR(hidpp_backlight_set(dev, &c), HIDPP_EINVAL);
    c = base;
    c.level = 8;
    T_ERR(hidpp_backlight_set(dev, &c), HIDPP_EINVAL);
    c = base;
    c.effect = 7;
    T_ERR(hidpp_backlight_set(dev, &c), HIDPP_EINVAL);
    c = base;
    c.effect = 200;
    T_ERR(hidpp_backlight_set(dev, &c), HIDPP_EINVAL);
    c = base;
    c.durationHandsOut = 0;
    T_ERR(hidpp_backlight_set(dev, &c), HIDPP_EINVAL);
    c = base;
    c.durationHandsIn = 7;
    T_ERR(hidpp_backlight_set(dev, &c), HIDPP_EINVAL);
    c = base;
    c.durationPowered = 7205;
    T_ERR(hidpp_backlight_set(dev, &c), HIDPP_EINVAL);
    T_ERR(hidpp_backlight_set(dev, NULL), HIDPP_EINVAL);

    T_EQ(f->requests, 0);
    fake_free(f);
    return 0;
}

static int test_set_rejects_unsupported_options(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    fake_add_backlight(f, 3);
    f->blSupported = 0x08 | 0x01; /* wow and automatic mode only */
    hidpp_device_t *dev = t_open(f);
    struct hidpp_backlight_config c = current(dev);
    T_OK(hidpp_backlight_set(dev, &c));

    c.crown = HIDPP_TRUE;
    T_ERR(hidpp_backlight_set(dev, &c), HIDPP_EINVAL);
    c.crown = HIDPP_FALSE;
    c.powerSave = HIDPP_TRUE;
    T_ERR(hidpp_backlight_set(dev, &c), HIDPP_EINVAL);
    c.powerSave = HIDPP_FALSE;
    c.mode = HIDPP_BACKLIGHT_MODE_PERM_MANUAL;
    T_ERR(hidpp_backlight_set(dev, &c), HIDPP_EINVAL);
    c.mode = HIDPP_BACKLIGHT_MODE_TEMP_MANUAL;
    T_ERR(hidpp_backlight_set(dev, &c), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

static int test_set_v2_rejects_modes(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_version(&f, 2);
    struct hidpp_backlight_config c = current(dev);
    T_OK(hidpp_backlight_set(dev, &c));
    c.mode = HIDPP_BACKLIGHT_MODE_AUTO;
    T_ERR(hidpp_backlight_set(dev, &c), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

static int test_state(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_v3(&f);
    struct hidpp_backlight_state s;
    T_OK(hidpp_backlight_state(dev, &s));
    T_EQ(s.levelCount, 8);
    T_EQ(s.level, 3);
    T_EQ(s.status, HIDPP_BACKLIGHT_ALS);
    T_STR(s.statusName, "Automatic");
    T_EQ(s.effect, HIDPP_BACKLIGHT_EFFECT_BREATHING);
    T_STR(s.effectName, "Breathing light");

    f->blStatus = HIDPP_BACKLIGHT_MANUAL_PERMANENT;
    T_OK(hidpp_backlight_state(dev, &s));
    T_STR(s.statusName, "Manual, permanent");
    f->blStatus = 99;
    T_OK(hidpp_backlight_state(dev, &s));
    T_STR(s.statusName, "Unknown");
    T_ERR(hidpp_backlight_state(dev, NULL), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

static int test_set_effect(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_v3(&f);
    T_OK(hidpp_backlight_set_effect(dev, HIDPP_BACKLIGHT_EFFECT_RANDOM));
    T_EQ(f->blEffect, HIDPP_BACKLIGHT_EFFECT_RANDOM);
    T_EQ(fake_last(f)->raw[3] >> 4, 3);
    T_ERR(hidpp_backlight_set_effect(dev, 9), HIDPP_EINVAL);
    T_EQ(f->blEffect, HIDPP_BACKLIGHT_EFFECT_RANDOM);

    f->blEffectList = 0x0005; /* static and breathing only */
    hidpp_cache_clear(dev);
    T_ERR(
        hidpp_backlight_set_effect(dev, HIDPP_BACKLIGHT_EFFECT_WAVES),
        HIDPP_EINVAL
    );
    T_OK(hidpp_backlight_set_effect(dev, HIDPP_BACKLIGHT_EFFECT_STATIC));
    fake_free(f);

    dev = open_version(&f, 1);
    T_ERR(hidpp_backlight_set_effect(dev, 0), HIDPP_ENOSYS);
    fake_free(f);
    return 0;
}

static int test_effect_names(int seed, int i) {
    (void)seed, (void)i;
    T_STR(hidpp_backlight_effect_name(0), "Static");
    T_STR(hidpp_backlight_effect_name(1), "None");
    T_STR(hidpp_backlight_effect_name(2), "Breathing light");
    T_STR(hidpp_backlight_effect_name(3), "Contrast");
    T_STR(hidpp_backlight_effect_name(4), "Reaction");
    T_STR(hidpp_backlight_effect_name(5), "Random");
    T_STR(hidpp_backlight_effect_name(6), "Waves");
    T_STR(hidpp_backlight_effect_name(7), "Unknown");
    return 0;
}

static int test_unsupported_and_null(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    struct hidpp_backlight_config c = { 0 };
    struct hidpp_backlight_state s;
    T_NULL(hidpp_backlight_info(dev));
    T_ERR(hidpp_backlight_get(dev, &c), HIDPP_ENOSYS);
    T_ERR(hidpp_backlight_set(dev, &c), HIDPP_ENOSYS);
    T_ERR(hidpp_backlight_state(dev, &s), HIDPP_ENOSYS);
    T_ERR(hidpp_backlight_set_effect(dev, 0), HIDPP_ENOSYS);
    fake_free(f);

    T_NULL(hidpp_backlight_info(NULL));
    T_ERR(hidpp_backlight_get(NULL, &c), HIDPP_EINVAL);
    T_ERR(hidpp_backlight_set(NULL, &c), HIDPP_EINVAL);
    T_ERR(hidpp_backlight_state(NULL, &s), HIDPP_EINVAL);
    T_ERR(hidpp_backlight_set_effect(NULL, 0), HIDPP_EINVAL);
    return 0;
}

static int test_event(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_all();
    hidpp_device_t *dev = t_open(f);
    uint16_t ids[32];
    T_TRUE(hidpp_feature_list(dev, ids, 32) > 0); /* resolve ids up front */

    const uint8_t data[] = { 8, 4, HIDPP_BACKLIGHT_MANUAL_TEMPORARY, 6 };
    fake_notify(f, FAKE_IDX_BACKLIGHT, 0, data, sizeof(data));
    struct hidpp_event e;
    T_OK(hidpp_device_poll(dev, &e));
    T_EQ(e.type, HIDPP_EVENT_BACKLIGHT);
    T_EQ(e.as.backlight.levelCount, 8);
    T_EQ(e.as.backlight.level, 4);
    T_EQ(e.as.backlight.status, HIDPP_BACKLIGHT_MANUAL_TEMPORARY);
    T_EQ(e.as.backlight.effect, 6);

    /* other functions are not events of this feature */
    fake_notify(f, FAKE_IDX_BACKLIGHT, 1, data, sizeof(data));
    T_OK(hidpp_device_poll(dev, &e));
    T_EQ(e.type, HIDPP_EVENT_UNKNOWN);
    fake_free(f);
    return 0;
}

static int test_info_is_cached(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_v3(&f);
    T_NOT_NULL(hidpp_backlight_info(dev));
    fake_reset_log(f);
    for (int n = 0; n < 5; n++)
        T_NOT_NULL(hidpp_backlight_info(dev));
    T_EQ(f->requests, 0);
    fake_free(f);
    return 0;
}

static int test_cache_roundtrip(int seed, int i) {
    (void)seed, (void)i;
    static char buf[CACHE_CAPACITY];
    fake_t *src = fake_new_all();
    hidpp_device_t *sdev = t_open(src);
    hidpp_cache_collect(sdev);
    size_t size = sizeof(buf);
    T_OK(hidpp_cache_save(sdev, buf, &size));
    fake_free(src);

    fake_t *f = fake_new_all();
    hidpp_device_t *dev = t_open(f);
    T_OK(hidpp_cache_load(dev, buf, size));
    fake_reset_log(f);

    struct hidpp_backlight_info *info = hidpp_backlight_info(dev);
    T_NOT_NULL(info);
    T_EQ(info->version, 3);
    T_EQ(info->levelCount, 8);
    T_EQ(info->effectList, 0x007F);
    T_TRUE(info->supportsPermManualMode && info->hasDurations);
    T_EQ(info->defaultDurationPowered, 300);
    T_EQ(f->requests, 0);
    fake_free(f);
    return 0;
}

static int test_garbage_responses(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_v3(&f);
    f->garbage = 1;
    f->garbageState = (uint32_t)seed | 1;
    struct hidpp_backlight_config c;
    struct hidpp_backlight_state s;

    for (int n = 0; n < 30; n++) {
        (void)hidpp_backlight_info(dev);
        if (!hidpp_backlight_get(dev, &c))
            (void)hidpp_backlight_set(dev, &c);
        (void)hidpp_backlight_state(dev, &s);
        (void)hidpp_backlight_set_effect(dev, (uint8_t)n);
        hidpp_cache_clear(dev);
    }
    return 0;
}

static const pf_test_t backlight_tests[] = {
    { test_info_v3, "info_v3" },
    { test_info_supported_bits, "info_supported_bits" },
    { test_info_older_versions, "info_older_versions" },
    { test_newer_versions_behave_like_v3, "newer_versions_behave_like_v3" },
    { test_get_v3, "get_v3" },
    { test_get_fields_come_from_the_right_bytes, "get_right_bytes" },
    { test_get_v2_has_no_modes_or_durations, "get_v2" },
    { test_set_roundtrip_v3, "set_roundtrip_v3" },
    { test_set_unchanged_config_is_a_noop, "set_unchanged_is_noop" },
    { test_set_wire_format_v3, "set_wire_format_v3" },
    { test_set_wire_format_v2, "set_wire_format_v2" },
    { test_set_validation, "set_validation" },
    { test_set_rejects_unsupported_options, "set_rejects_unsupported" },
    { test_set_v2_rejects_modes, "set_v2_rejects_modes" },
    { test_state, "state" },
    { test_set_effect, "set_effect" },
    { test_effect_names, "effect_names" },
    { test_unsupported_and_null, "unsupported_and_null" },
    { test_event, "event" },
    { test_info_is_cached, "info_is_cached" },
    { test_cache_roundtrip, "cache_roundtrip" },
    { test_garbage_responses, "garbage_responses" },
    { 0 },
};

const pf_suite_t suite_backlight = { "backlight", 1, backlight_tests };
