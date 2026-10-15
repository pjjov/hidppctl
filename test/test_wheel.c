/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

/*  Wheels: vertical scrolling (0x2100, 0x2120), SmartShift (0x2110,
    0x2111), high resolution wheel (0x2121), thumbwheel (0x2150) and
    crown (0x4600). */

#include "test.h"

/* ---------------------------- scrolling ------------------------------ */

static int roller_index, highres_index;

static fake_t *make_scrolling(void) {
    fake_t *f = t_new();
    fake_add_firmware_info(f);
    roller_index = t_add_canned(f, 0x2100, 0);
    highres_index = t_add_canned(f, 0x2120, 0);
    t_answer(roller_index, 0, (const uint8_t[]) { 3, 24, 3 }, 3);
    t_answer(highres_index, 0, (const uint8_t[]) { 0, 8 }, 2);
    t_answer(highres_index, 1, (const uint8_t[]) { 1, 8 }, 2);
    return f;
}

static int test_scroll_info(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_scrolling();
    hidpp_device_t *dev = t_open(f);
    struct hidpp_scroll_info *info = hidpp_scroll_info(dev);
    T_NOT_NULL(info);
    T_TRUE(info->initialized && info->hasRollerInfo && info->hasHighRes);
    T_EQ(info->rollerType, HIDPP_ROLLER_3G);
    T_EQ(info->ratchetsPerTurn, 24);
    T_EQ(info->scrollLines, 3);
    T_EQ(info->multiplier, 8);
    T_STR(info->rollerTypeName, "3G roller");
    fake_free(f);
    return 0;
}

static int test_scroll_highres_only(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = t_new();
    int idx = t_add_canned(f, 0x2120, 0);
    t_answer(idx, 0, (const uint8_t[]) { 1, 16 }, 2);
    hidpp_device_t *dev = t_open(f);
    struct hidpp_scroll_info *info = hidpp_scroll_info(dev);
    T_NOT_NULL(info);
    T_FALSE(info->hasRollerInfo);
    T_TRUE(info->hasHighRes);
    T_EQ(info->multiplier, 16);
    T_EQ(hidpp_scroll_highres(dev), 1);
    fake_free(f);
    return 0;
}

static int test_scroll_highres(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_scrolling();
    hidpp_device_t *dev = t_open(f);
    T_EQ(hidpp_scroll_highres(dev), 0);
    T_EQ(T_FUNC(f), 0);

    T_OK(hidpp_scroll_set_highres(dev, 1));
    T_EQ(T_FUNC(f), 1);
    T_EQ(T_PARAM(f, 0), 1);

    /* the device confirms with the mode it has set */
    T_ERR(hidpp_scroll_set_highres(dev, 0), HIDPP_EIO);
    fake_free(f);
    return 0;
}

static int test_scroll_roller_only(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = t_new();
    int idx = t_add_canned(f, 0x2100, 0);
    t_answer(idx, 0, (const uint8_t[]) { 1, 18, 0xFF }, 3);
    hidpp_device_t *dev = t_open(f);
    T_EQ(hidpp_scroll_info(dev)->scrollLines, 0xFF);
    T_FALSE(hidpp_scroll_info(dev)->hasHighRes);
    T_ERR(hidpp_scroll_highres(dev), HIDPP_ENOSYS);
    T_ERR(hidpp_scroll_set_highres(dev, 1), HIDPP_ENOSYS);
    fake_free(f);
    return 0;
}

static int test_scroll_unsupported(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = t_new();
    hidpp_device_t *dev = t_open(f);
    T_NULL(hidpp_scroll_info(dev));
    T_ERR(hidpp_scroll_highres(dev), HIDPP_ENOSYS);
    T_NULL(hidpp_scroll_info(NULL));
    T_ERR(hidpp_scroll_set_highres(NULL, 1), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

static int test_scroll_cache(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = t_from_cache(make_scrolling, &f);
    struct hidpp_scroll_info *info = hidpp_scroll_info(dev);
    T_NOT_NULL(info);
    T_EQ(info->ratchetsPerTurn, 24);
    T_EQ(info->multiplier, 8);
    T_TRUE(info->hasHighRes);
    T_STR(info->rollerTypeName, "3G roller");
    T_EQ(f->requests, 0);
    fake_free(f);
    return 0;
}

static const pf_test_t scrolling_tests[] = {
    { test_scroll_info, "info" },
    { test_scroll_highres_only, "highres_only" },
    { test_scroll_highres, "highres" },
    { test_scroll_roller_only, "roller_only" },
    { test_scroll_unsupported, "unsupported" },
    { test_scroll_cache, "cache" },
    { 0 },
};

const pf_suite_t suite_scrolling = { "scrolling", 1, scrolling_tests };

/* ---------------------------- smartshift ----------------------------- */

static int smartshift_index;

static fake_t *make_smartshift(void) {
    fake_t *f = t_new();
    fake_add_firmware_info(f);
    smartshift_index = t_add_canned(f, 0x2111, 0);
    /* tunable torque, disengage at 10/4 t/s, 50 % of 1200 gF */
    t_answer(
        smartshift_index, 0, (const uint8_t[]) { 1, 10, 50, 0x04, 0xB0 }, 5
    );
    t_answer(smartshift_index, 1, (const uint8_t[]) { 2, 30, 75 }, 3);
    return f;
}

static int test_smartshift_basic(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = t_new();
    int idx = t_add_canned(f, 0x2110, 0);
    t_answer(idx, 0, (const uint8_t[]) { 1, 20, 15 }, 3);
    hidpp_device_t *dev = t_open(f);

    struct hidpp_smartshift_info *info = hidpp_smartshift_info(dev);
    T_NOT_NULL(info);
    T_FALSE(info->isEnhanced || info->hasTunableTorque);

    struct hidpp_smartshift s;
    T_OK(hidpp_smartshift_get(dev, &s));
    T_EQ(s.wheelMode, HIDPP_WHEEL_MODE_FREESPIN);
    T_EQ(s.autoDisengage, 20);
    T_EQ(s.autoDisengageDefault, 15);
    T_EQ(s.torque, 0);

    struct hidpp_smartshift set = { .wheelMode = HIDPP_WHEEL_MODE_RATCHET,
                                    .autoDisengageDefault = 0xFF };
    T_OK(hidpp_smartshift_set(dev, &set));
    T_EQ(T_FUNC(f), 1);
    T_EQ(T_PARAM(f, 0), 2);
    T_EQ(T_PARAM(f, 1), 0); /* unchanged */
    T_EQ(T_PARAM(f, 2), 0xFF);

    set.torque = 50; /* needs 0x2111 */
    T_ERR(hidpp_smartshift_set(dev, &set), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

static int test_smartshift_enhanced(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_smartshift();
    hidpp_device_t *dev = t_open(f);

    struct hidpp_smartshift_info *info = hidpp_smartshift_info(dev);
    T_NOT_NULL(info);
    T_TRUE(info->isEnhanced && info->hasTunableTorque);
    T_EQ(info->defaultAutoDisengage, 10);
    T_EQ(info->defaultTorque, 50);
    T_EQ(info->maxForce, 1200);

    struct hidpp_smartshift s;
    T_OK(hidpp_smartshift_get(dev, &s));
    T_EQ(T_FUNC(f), 1);
    T_EQ(s.wheelMode, HIDPP_WHEEL_MODE_RATCHET);
    T_EQ(s.autoDisengage, 30);
    T_EQ(s.torque, 75);
    T_EQ(s.autoDisengageDefault, 0);

    s.torque = 100;
    T_OK(hidpp_smartshift_set(dev, &s));
    T_EQ(T_FUNC(f), 2);
    T_EQ(T_PARAM(f, 0), 2);
    T_EQ(T_PARAM(f, 1), 30);
    T_EQ(T_PARAM(f, 2), 100);
    fake_free(f);
    return 0;
}

static int test_smartshift_prefers_enhanced(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = t_new();
    t_add_canned(f, 0x2110, 0);
    int idx = t_add_canned(f, 0x2111, 0);
    hidpp_device_t *dev = t_open(f);
    T_TRUE(hidpp_smartshift_info(dev)->isEnhanced);
    struct hidpp_smartshift s;
    T_OK(hidpp_smartshift_get(dev, &s));
    T_EQ(fake_last(f)->raw[2], idx);
    fake_free(f);
    return 0;
}

static int test_smartshift_validation(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_smartshift();
    hidpp_device_t *dev = t_open(f);
    T_NOT_NULL(hidpp_smartshift_info(dev));
    unsigned before = f->requests;

    struct hidpp_smartshift s = { .wheelMode = 3 };
    T_ERR(hidpp_smartshift_set(dev, &s), HIDPP_EINVAL);
    s = (struct hidpp_smartshift) { .torque = 101 };
    T_ERR(hidpp_smartshift_set(dev, &s), HIDPP_EINVAL);
    s = (struct hidpp_smartshift) { .autoDisengageDefault = 5 };
    T_ERR(hidpp_smartshift_set(dev, &s), HIDPP_EINVAL);
    T_ERR(hidpp_smartshift_set(dev, NULL), HIDPP_EINVAL);
    T_ERR(hidpp_smartshift_get(dev, NULL), HIDPP_EINVAL);
    T_EQ(f->requests, before);

    /* torque can be refused by an enhanced wheel without it */
    t_answer(smartshift_index, 0, (const uint8_t[]) { 0 }, 1);
    hidpp_cache_clear(dev);
    s = (struct hidpp_smartshift) { .torque = 10 };
    T_ERR(hidpp_smartshift_set(dev, &s), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

static int test_smartshift_unsupported(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = t_new();
    hidpp_device_t *dev = t_open(f);
    struct hidpp_smartshift s = { 0 };
    T_NULL(hidpp_smartshift_info(dev));
    T_ERR(hidpp_smartshift_get(dev, &s), HIDPP_ENOSYS);
    T_ERR(hidpp_smartshift_set(dev, &s), HIDPP_ENOSYS);
    T_ERR(hidpp_smartshift_get(NULL, &s), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

static int test_smartshift_cache(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = t_from_cache(make_smartshift, &f);
    struct hidpp_smartshift_info *info = hidpp_smartshift_info(dev);
    T_NOT_NULL(info);
    T_TRUE(info->isEnhanced && info->hasTunableTorque);
    T_EQ(info->maxForce, 1200);
    T_EQ(f->requests, 0);
    fake_free(f);
    return 0;
}

static const pf_test_t smartshift_tests[] = {
    { test_smartshift_basic, "basic" },
    { test_smartshift_enhanced, "enhanced" },
    { test_smartshift_prefers_enhanced, "prefers_enhanced" },
    { test_smartshift_validation, "validation" },
    { test_smartshift_unsupported, "unsupported" },
    { test_smartshift_cache, "cache" },
    { 0 },
};

const pf_suite_t suite_smartshift = { "smartshift", 1, smartshift_tests };

/* --------------------------- hires wheel ----------------------------- */

static int wheel_index;

static fake_t *make_wheel_version(uint8_t version) {
    fake_t *f = t_new();
    fake_add_firmware_info(f);
    wheel_index = t_add_canned(f, 0x2121, version);
    /* multiplier 8, switch + invert + analytics, 24 ratchets, 32 mm */
    t_answer(wheel_index, 0, (const uint8_t[]) { 8, 0x1C, 24, 32 }, 4);
    t_answer(wheel_index, 1, (const uint8_t[]) { 0x0B }, 1);
    t_answer(wheel_index, 3, (const uint8_t[]) { 1 }, 1);
    t_answer(wheel_index, 4, (const uint8_t[]) { 0xDE, 0xAD }, 2);
    return f;
}

static fake_t *make_wheel(void) { return make_wheel_version(1); }

static int test_wheel_info(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_wheel();
    hidpp_device_t *dev = t_open(f);
    struct hidpp_wheel_info *info = hidpp_wheel_info(dev);
    T_NOT_NULL(info);
    T_EQ(info->version, 1);
    T_EQ(info->multiplier, 8);
    T_TRUE(info->hasSwitch && info->hasInvert && info->hasAnalytics);
    T_EQ(info->ratchetsPerRotation, 24);
    T_EQ(info->diameter, 32);
    fake_free(f);
    return 0;
}

static int test_wheel_info_v0(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_wheel_version(0);
    hidpp_device_t *dev = t_open(f);
    struct hidpp_wheel_info *info = hidpp_wheel_info(dev);
    T_NOT_NULL(info);
    T_TRUE(info->hasSwitch && info->hasInvert);
    T_FALSE(info->hasAnalytics); /* version 1 */
    T_EQ(info->ratchetsPerRotation, 0);
    T_EQ(info->diameter, 0);

    uint8_t data[4];
    T_ERR(hidpp_wheel_analytics(dev, data, sizeof(data)), HIDPP_ENOSYS);

    struct hidpp_wheel_mode m;
    T_OK(hidpp_wheel_get_mode(dev, &m));
    T_FALSE(m.analytics);
    fake_free(f);
    return 0;
}

static int test_wheel_mode(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_wheel();
    hidpp_device_t *dev = t_open(f);

    struct hidpp_wheel_mode m;
    T_OK(hidpp_wheel_get_mode(dev, &m));
    T_TRUE(m.diverted && m.highRes && m.analytics);
    T_FALSE(m.inverted);

    m = (struct hidpp_wheel_mode) { .highRes = 1, .inverted = 1 };
    T_OK(hidpp_wheel_set_mode(dev, &m));
    T_EQ(T_FUNC(f), 2);
    T_EQ(T_PARAM(f, 0), 0x06);

    m = (struct hidpp_wheel_mode) { .diverted = 1, .analytics = 1 };
    T_OK(hidpp_wheel_set_mode(dev, &m));
    T_EQ(T_PARAM(f, 0), 0x09);
    fake_free(f);
    return 0;
}

static int test_wheel_capabilities_are_enforced(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_wheel();
    t_answer(wheel_index, 0, (const uint8_t[]) { 8, 0x00 }, 2);
    hidpp_device_t *dev = t_open(f);
    T_NOT_NULL(hidpp_wheel_info(dev));
    unsigned before = f->requests;

    struct hidpp_wheel_mode m = { .inverted = 1 };
    T_ERR(hidpp_wheel_set_mode(dev, &m), HIDPP_EINVAL);
    m = (struct hidpp_wheel_mode) { .analytics = 1 };
    T_ERR(hidpp_wheel_set_mode(dev, &m), HIDPP_EINVAL);
    T_ERR(hidpp_wheel_ratchet(dev), HIDPP_ENOSYS);
    T_EQ(f->requests, before);
    fake_free(f);
    return 0;
}

static int test_wheel_ratchet_and_analytics(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_wheel();
    hidpp_device_t *dev = t_open(f);
    T_EQ(hidpp_wheel_ratchet(dev), 1);
    T_EQ(T_FUNC(f), 3);

    uint8_t data[2];
    T_EQ(hidpp_wheel_analytics(dev, data, sizeof(data)), 2);
    T_EQ(T_FUNC(f), 4);
    T_EQ(data[0], 0xDE);
    T_EQ(data[1], 0xAD);

    uint8_t all[64];
    T_EQ(hidpp_wheel_analytics(dev, all, sizeof(all)), HIDPP_LEN_LONG - 4);
    T_ERR(hidpp_wheel_analytics(dev, NULL, 1), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

static int test_wheel_event(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_wheel();
    hidpp_device_t *dev = t_open(f);
    const uint8_t data[] = { 0x13, 0xFF, 0xF0 }; /* high res, 3 periods */
    struct hidpp_event e = t_event(f, dev, wheel_index, 0, data, 3);
    T_EQ(e.type, HIDPP_EVENT_WHEEL);
    T_EQ(e.as.wheel.delta, -16);
    T_TRUE(e.as.wheel.highRes);
    T_EQ(e.as.wheel.periods, 3);

    e = t_event(f, dev, wheel_index, 0, (const uint8_t[]) { 0x01, 0, 1 }, 3);
    T_FALSE(e.as.wheel.highRes);
    T_EQ(e.as.wheel.periods, 1);
    fake_free(f);
    return 0;
}

static int test_wheel_cache(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = t_from_cache(make_wheel, &f);
    struct hidpp_wheel_info *info = hidpp_wheel_info(dev);
    T_NOT_NULL(info);
    T_EQ(info->multiplier, 8);
    T_EQ(info->diameter, 32);
    T_TRUE(info->hasAnalytics);
    T_EQ(f->requests, 0);
    fake_free(f);
    return 0;
}

static const pf_test_t hires_wheel_tests[] = {
    { test_wheel_info, "info" },
    { test_wheel_info_v0, "info_v0" },
    { test_wheel_mode, "mode" },
    { test_wheel_capabilities_are_enforced, "capabilities_enforced" },
    { test_wheel_ratchet_and_analytics, "ratchet_and_analytics" },
    { test_wheel_event, "event" },
    { test_wheel_cache, "cache" },
    { 0 },
};

const pf_suite_t suite_hires_wheel = { "hires_wheel", 1, hires_wheel_tests };

/* ---------------------------- thumbwheel ----------------------------- */

static int thumb_index;

static fake_t *make_thumbwheel(void) {
    fake_t *f = t_new();
    fake_add_firmware_info(f);
    thumb_index = t_add_canned(f, 0x2150, 0);
    /* 18 ratchets, 120 increments, positive to the right, touch + proxy,
       time unit 1000 */
    t_answer(
        thumb_index,
        0,
        (const uint8_t[]) { 0, 18, 0, 120, 1, 0x06, 0x03, 0xE8 },
        8
    );
    t_answer(thumb_index, 1, (const uint8_t[]) { 1, 0x05 }, 2);
    return f;
}

static int test_thumbwheel_info(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_thumbwheel();
    hidpp_device_t *dev = t_open(f);
    struct hidpp_thumbwheel_info *info = hidpp_thumbwheel_info(dev);
    T_NOT_NULL(info);
    T_EQ(info->nativeResolution, 18);
    T_EQ(info->divertedResolution, 120);
    T_EQ(info->defaultDirection, 1);
    T_EQ(info->timeUnit, 1000);
    T_FALSE(info->hasTimestamp || info->hasSingleTap);
    T_TRUE(info->hasTouch && info->hasProxy);
    fake_free(f);
    return 0;
}

static int test_thumbwheel_status_and_set(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_thumbwheel();
    hidpp_device_t *dev = t_open(f);
    struct hidpp_thumbwheel_status s;
    T_OK(hidpp_thumbwheel_status(dev, &s));
    T_TRUE(s.diverted && s.inverted && s.proxy);
    T_FALSE(s.touch);

    T_OK(hidpp_thumbwheel_set(dev, 0, 7));
    T_EQ(T_FUNC(f), 2);
    T_EQ(T_PARAM(f, 0), 0);
    T_EQ(T_PARAM(f, 1), 1);
    T_ERR(hidpp_thumbwheel_status(dev, NULL), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

static int test_thumbwheel_event(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_thumbwheel();
    hidpp_device_t *dev = t_open(f);
    const uint8_t data[] = { 0xFF, 0xFD, 0x01, 0x02, 2, 0x05 };
    struct hidpp_event e = t_event(f, dev, thumb_index, 0, data, 6);
    T_EQ(e.type, HIDPP_EVENT_THUMBWHEEL);
    T_EQ(e.as.thumbwheel.rotation, -3);
    T_EQ(e.as.thumbwheel.timestamp, 0x0102);
    T_EQ(e.as.thumbwheel.status, HIDPP_ROTATION_ACTIVE);
    T_TRUE(e.as.thumbwheel.touch && e.as.thumbwheel.singleTap);
    T_FALSE(e.as.thumbwheel.proxy);
    fake_free(f);
    return 0;
}

static int test_thumbwheel_unsupported_and_cache(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = t_new();
    hidpp_device_t *dev = t_open(f);
    T_NULL(hidpp_thumbwheel_info(dev));
    T_ERR(hidpp_thumbwheel_set(dev, 1, 0), HIDPP_ENOSYS);
    fake_free(f);

    dev = t_from_cache(make_thumbwheel, &f);
    T_EQ(hidpp_thumbwheel_info(dev)->divertedResolution, 120);
    T_TRUE(hidpp_thumbwheel_info(dev)->hasProxy);
    T_EQ(f->requests, 0);
    fake_free(f);
    return 0;
}

static const pf_test_t thumbwheel_tests[] = {
    { test_thumbwheel_info, "info" },
    { test_thumbwheel_status_and_set, "status_and_set" },
    { test_thumbwheel_event, "event" },
    { test_thumbwheel_unsupported_and_cache, "unsupported_and_cache" },
    { 0 },
};

const pf_suite_t suite_thumbwheel = { "thumbwheel", 1, thumbwheel_tests };

/* ------------------------------ crown -------------------------------- */

static int crown_index;

static fake_t *make_crown(void) {
    fake_t *f = t_new();
    fake_add_firmware_info(f);
    crown_index = t_add_canned(f, 0x4600, 0);
    /* everything but the double tap speed and the double tap gesture,
       24 slots and 12 ratchets per revolution */
    t_answer(crown_index, 0, (const uint8_t[]) { 0x1F, 0x07, 0, 24, 0, 12 }, 6);
    t_answer(crown_index, 1, (const uint8_t[]) { 2, 1, 0x20, 50, 30 }, 5);
    return f;
}

static int test_crown_info(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_crown();
    hidpp_device_t *dev = t_open(f);
    struct hidpp_crown_info *info = hidpp_crown_info(dev);
    T_NOT_NULL(info);
    T_EQ(info->capabilities, 0x1F07);
    T_EQ(info->slots, 24);
    T_EQ(info->ratchets, 12);
    T_TRUE(info->hasButton && info->hasLongPress && info->isMechanized);
    T_TRUE(info->canSetRotationTimeout && info->canSetShortLongTimeout);
    T_FALSE(info->canSetDoubleTapSpeed);
    T_TRUE(info->hasProximity && info->hasTouch && info->hasTap);
    T_FALSE(info->hasDoubleTap);
    fake_free(f);
    return 0;
}

static int test_crown_mode(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_crown();
    hidpp_device_t *dev = t_open(f);
    struct hidpp_crown_mode m;
    T_OK(hidpp_crown_get(dev, &m));
    T_EQ(m.reporting, HIDPP_CROWN_DIVERTED);
    T_EQ(m.ratchet, HIDPP_WHEEL_MODE_FREESPIN);
    T_EQ(m.rotationTimeout, 320);
    T_EQ(m.shortLongTimeout, 500);
    T_EQ(m.doubleTapSpeed, 300);

    m = (struct hidpp_crown_mode) { .reporting = HIDPP_CROWN_HID,
                                    .ratchet = HIDPP_WHEEL_MODE_RATCHET,
                                    .rotationTimeout = 640,
                                    .shortLongTimeout = 2550 };
    T_OK(hidpp_crown_set(dev, &m));
    T_EQ(T_FUNC(f), 2);
    T_EQ(T_PARAM(f, 0), 1);
    T_EQ(T_PARAM(f, 1), 2);
    T_EQ(T_PARAM(f, 2), 0x40);
    T_EQ(T_PARAM(f, 3), 255);
    T_EQ(T_PARAM(f, 4), 0);
    fake_free(f);
    return 0;
}

static int test_crown_validation(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_crown();
    hidpp_device_t *dev = t_open(f);
    T_NOT_NULL(hidpp_crown_info(dev));
    unsigned before = f->requests;

    struct hidpp_crown_mode m = { .rotationTimeout = 650 };
    T_ERR(hidpp_crown_set(dev, &m), HIDPP_EINVAL);
    m = (struct hidpp_crown_mode) { .shortLongTimeout = 15 };
    T_ERR(hidpp_crown_set(dev, &m), HIDPP_EINVAL);
    m = (struct hidpp_crown_mode) { .doubleTapSpeed = 100 }; /* fixed */
    T_ERR(hidpp_crown_set(dev, &m), HIDPP_EINVAL);
    m = (struct hidpp_crown_mode) { .reporting = 3 };
    T_ERR(hidpp_crown_set(dev, &m), HIDPP_EINVAL);
    m = (struct hidpp_crown_mode) { .ratchet = 3 };
    T_ERR(hidpp_crown_set(dev, &m), HIDPP_EINVAL);
    T_EQ(f->requests, before);

    /* a ratchet without a motor cannot be switched */
    t_answer(crown_index, 0, (const uint8_t[]) { 0x00 }, 1);
    hidpp_cache_clear(dev);
    m = (struct hidpp_crown_mode) { .ratchet = HIDPP_WHEEL_MODE_RATCHET };
    T_ERR(hidpp_crown_set(dev, &m), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

static int test_crown_event(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_crown();
    hidpp_device_t *dev = t_open(f);
    uint8_t data[16] = { 2, 0xFE, 3, 1, 2, 2, 4 };
    data[14] = 0xFF;
    data[15] = 0x9C; /* -100 slots per second */
    struct hidpp_event e = t_event(f, dev, crown_index, 0, data, 16);
    T_EQ(e.type, HIDPP_EVENT_CROWN);
    T_EQ(e.as.crown.rotation, HIDPP_ROTATION_ACTIVE);
    T_EQ(e.as.crown.slots, -2);
    T_EQ(e.as.crown.ratchets, 3);
    T_EQ(e.as.crown.proximity, HIDPP_ROTATION_START);
    T_EQ(e.as.crown.touch, HIDPP_ROTATION_ACTIVE);
    T_EQ(e.as.crown.gesture, 2);
    T_EQ(e.as.crown.button, HIDPP_CROWN_BUTTON_LONG_ACTIVE);
    T_EQ(e.as.crown.speed, -100);
    fake_free(f);
    return 0;
}

static int test_crown_cache(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = t_from_cache(make_crown, &f);
    struct hidpp_crown_info *info = hidpp_crown_info(dev);
    T_NOT_NULL(info);
    T_EQ(info->slots, 24);
    T_TRUE(info->isMechanized && info->hasTap);
    T_EQ(f->requests, 0);
    fake_free(f);
    return 0;
}

static const pf_test_t crown_tests[] = {
    { test_crown_info, "info" },
    { test_crown_mode, "mode" },
    { test_crown_validation, "validation" },
    { test_crown_event, "event" },
    { test_crown_cache, "cache" },
    { 0 },
};

const pf_suite_t suite_crown = { "crown", 1, crown_tests };
