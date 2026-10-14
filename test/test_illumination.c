/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

/*  Illumination light (0x1990), versions 0 and 1. */

#include "test.h"

#define CACHE_CAPACITY (128 * 1024)

#define BRIGHTNESS HIDPP_ILLUMINATION_BRIGHTNESS
#define COLOR HIDPP_ILLUMINATION_COLOR_TEMPERATURE

static hidpp_device_t *open_version(fake_t **out, uint8_t version) {
    fake_t *f = fake_new_bare();
    fake_add_illumination(f, version);
    *out = f;
    return t_open(f);
}

static hidpp_device_t *open_v1(fake_t **out) { return open_version(out, 1); }

static int test_info(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_v1(&f);

    struct hidpp_illumination_info *b = hidpp_illumination_info(
        dev, BRIGHTNESS
    );
    T_NOT_NULL(b);
    T_TRUE(b->initialized && b->isSettable);
    T_EQ(b->min, 10);
    T_EQ(b->max, 1000);
    T_EQ(b->res, 10);
    T_EQ(b->maxLevels, 6);
    T_TRUE(b->hasEvents && b->hasLinearLevels && b->hasNonLinearLevels);
    T_TRUE(b->hasDynamicMaximum);

    struct hidpp_illumination_info *c = hidpp_illumination_info(dev, COLOR);
    T_NOT_NULL(c);
    T_EQ(c->min, 2700);
    T_EQ(c->max, 6500);
    T_EQ(c->res, 50);
    T_EQ(c->maxLevels, 0);
    T_TRUE(c->hasEvents && c->hasLinearLevels);
    T_FALSE(c->hasNonLinearLevels);
    T_FALSE(c->hasDynamicMaximum); /* brightness only */

    T_NULL(hidpp_illumination_info(dev, 2));
    T_NULL(hidpp_illumination_info(dev, -1));
    fake_free(f);
    return 0;
}

static int test_info_bytes(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_v1(&f);
    /* all distinct, a high maxLevels nibble and junk in the reserved one */
    f->ill[0] = (struct fake_illum_control) { .flags = 0x01,
                                              .min = 0x0102,
                                              .max = 0x0304,
                                              .res = 0x0506,
                                              .maxLevels = 0xF9 };
    hidpp_cache_clear(dev);
    struct hidpp_illumination_info *b = hidpp_illumination_info(
        dev, BRIGHTNESS
    );
    T_NOT_NULL(b);
    T_EQ(b->min, 0x0102);
    T_EQ(b->max, 0x0304);
    T_EQ(b->res, 0x0506);
    T_EQ(b->maxLevels, 9);
    T_TRUE(b->hasEvents);
    T_FALSE(b->hasLinearLevels || b->hasNonLinearLevels);
    fake_free(f);
    return 0;
}

static int test_info_v0_has_no_dynamic_maximum(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_version(&f, 0);
    f->ill[0].flags |= 0x08;
    hidpp_cache_clear(dev);
    T_FALSE(hidpp_illumination_info(dev, BRIGHTNESS)->hasDynamicMaximum);
    T_ERR(hidpp_illumination_effective_max(dev), HIDPP_ENOSYS);
    fake_free(f);
    return 0;
}

static int test_single_value_control(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_v1(&f);
    f->ill[1].min = f->ill[1].max = 4000;
    f->ill[1].res = 0;
    hidpp_cache_clear(dev);
    T_FALSE(hidpp_illumination_info(dev, COLOR)->isSettable);
    T_EQ(hidpp_illumination_value(dev, COLOR), 4000);
    T_ERR(hidpp_illumination_set_value(dev, COLOR, 4000), HIDPP_ENOSYS);
    fake_free(f);
    return 0;
}

static int test_state(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_v1(&f);
    T_EQ(hidpp_illumination_get(dev), 1);
    T_OK(hidpp_illumination_set(dev, 0));
    T_EQ(f->illState, 0);
    T_EQ(hidpp_illumination_get(dev), 0);
    T_OK(hidpp_illumination_set(dev, 42));
    T_EQ(f->illState, 1);
    T_EQ(fake_last(f)->raw[4], 1);
    fake_free(f);
    return 0;
}

static int test_value(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_v1(&f);
    T_EQ(hidpp_illumination_value(dev, BRIGHTNESS), 500);
    T_EQ(hidpp_illumination_value(dev, COLOR), 4000);

    T_OK(hidpp_illumination_set_value(dev, BRIGHTNESS, 610));
    T_EQ(f->ill[0].value, 610);
    T_OK(hidpp_illumination_set_value(dev, COLOR, 6500));
    T_EQ(f->ill[1].value, 6500);
    T_EQ(hidpp_illumination_value(dev, COLOR), 6500); /* 16 bit, big endian */

    T_ERR(hidpp_illumination_value(dev, 5), HIDPP_EINVAL);
    T_ERR(hidpp_illumination_set_value(dev, -1, 0), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

static int test_function_numbers(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_v1(&f);
    T_NOT_NULL(hidpp_illumination_info(dev, BRIGHTNESS));
    fake_reset_log(f);

    (void)hidpp_illumination_value(dev, BRIGHTNESS);
    T_EQ(fake_last(f)->raw[3] >> 4, 3);
    (void)hidpp_illumination_set_value(dev, BRIGHTNESS, 100);
    T_EQ(fake_last(f)->raw[3] >> 4, 4);
    (void)hidpp_illumination_value(dev, COLOR);
    T_EQ(fake_last(f)->raw[3] >> 4, 8);
    (void)hidpp_illumination_set_value(dev, COLOR, 3000);
    T_EQ(fake_last(f)->raw[3] >> 4, 9);
    (void)hidpp_illumination_effective_max(dev);
    T_EQ(fake_last(f)->raw[3] >> 4, 12);
    fake_free(f);
    return 0;
}

static int test_set_value_validation(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_v1(&f);
    T_NOT_NULL(hidpp_illumination_info(dev, BRIGHTNESS));
    fake_reset_log(f);

    T_ERR(hidpp_illumination_set_value(dev, BRIGHTNESS, 605), HIDPP_EINVAL);
    T_ERR(hidpp_illumination_set_value(dev, BRIGHTNESS, 5), HIDPP_EINVAL);
    T_ERR(hidpp_illumination_set_value(dev, BRIGHTNESS, 1010), HIDPP_EINVAL);
    T_ERR(hidpp_illumination_set_value(dev, COLOR, 2699), HIDPP_EINVAL);
    T_ERR(hidpp_illumination_set_value(dev, COLOR, 2725), HIDPP_EINVAL);
    T_EQ(f->requests, 0);
    fake_free(f);
    return 0;
}

static int test_levels_non_linear(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_v1(&f);
    struct hidpp_illumination_levels l;

    T_OK(hidpp_illumination_levels(dev, BRIGHTNESS, &l));
    T_FALSE(l.linear);
    T_EQ(l.count, 4);
    T_EQ(l.values[0], 10);
    T_EQ(l.values[1], 300);
    T_EQ(l.values[2], 700);
    T_EQ(l.values[3], 1000);

    struct hidpp_illumination_levels set = { .count = 3,
                                             .values = { 20, 500, 900 } };
    T_OK(hidpp_illumination_set_levels(dev, BRIGHTNESS, &set));
    T_FALSE(f->ill[0].linear);
    T_EQ(f->ill[0].levelCount, 3);
    T_EQ(f->ill[0].levels[2], 900);
    T_OK(hidpp_illumination_levels(dev, BRIGHTNESS, &l));
    T_EQ(l.count, 3);
    T_EQ(l.values[1], 500);
    fake_free(f);
    return 0;
}

static int test_levels_wire_format(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_v1(&f);
    T_NOT_NULL(hidpp_illumination_info(dev, BRIGHTNESS));
    struct hidpp_illumination_levels set = { .count = 2,
                                             .values = { 0x0136, 0x0262 } };
    fake_reset_log(f);
    T_OK(hidpp_illumination_set_levels(dev, BRIGHTNESS, &set));

    const struct fake_packet *p = fake_last(f);
    T_EQ(p->raw[0], HIDPP_KIND_LONG);
    T_EQ(p->raw[3] >> 4, 6);
    T_EQ(p->raw[4], 2 << 5); /* validCount, not linear, not reset */
    T_EQ(p->raw[5], 0x02);   /* start 0, levelCount 2 */
    T_EQ(p->raw[6], 0x01);
    T_EQ(p->raw[7], 0x36);
    T_EQ(p->raw[8], 0x02);
    T_EQ(p->raw[9], 0x62);
    fake_free(f);
    return 0;
}

static int test_levels_are_split_into_chunks(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_v1(&f);
    f->ill[0].maxLevels = 15;
    hidpp_cache_clear(dev);
    T_NOT_NULL(hidpp_illumination_info(dev, BRIGHTNESS));

    struct hidpp_illumination_levels set = { .count = 10 };
    for (int n = 0; n < 10; n++)
        set.values[n] = (uint16_t)(100 + 10 * n);

    fake_reset_log(f);
    T_OK(hidpp_illumination_set_levels(dev, BRIGHTNESS, &set));
    T_EQ(f->requests, 2);

    /* the level count grows with the values that have been written */
    T_EQ(fake_logged(f, 0)->raw[4] >> 5, 7);
    T_EQ(fake_logged(f, 0)->raw[5], 0x07);
    T_EQ(fake_logged(f, 1)->raw[4] >> 5, 3);
    T_EQ(fake_logged(f, 1)->raw[5], 0x7A);
    T_EQ(f->ill[0].levelCount, 10);

    struct hidpp_illumination_levels l;
    fake_reset_log(f);
    T_OK(hidpp_illumination_levels(dev, BRIGHTNESS, &l));
    T_EQ(f->requests, 2);
    T_EQ(l.count, 10);
    for (int n = 0; n < 10; n++)
        T_EQ(l.values[n], 100 + 10 * n);
    fake_free(f);
    return 0;
}

static int test_levels_linear(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_v1(&f);
    struct hidpp_illumination_levels l;

    T_OK(hidpp_illumination_levels(dev, COLOR, &l));
    T_TRUE(l.linear);
    T_EQ(l.min, 2700);
    T_EQ(l.max, 6500);
    T_EQ(l.step, 950);
    T_EQ(l.count, 5);

    struct hidpp_illumination_levels set = {
        .linear = 1, .min = 3000, .max = 6000, .step = 1000
    };
    T_OK(hidpp_illumination_set_levels(dev, COLOR, &set));
    T_EQ(f->ill[1].lmin, 3000);
    T_EQ(f->ill[1].lmax, 6000);
    T_EQ(f->ill[1].lstep, 1000);
    const struct fake_packet *p = fake_last(f);
    T_EQ(p->raw[4], 0x01);
    T_EQ(p->raw[6], HIDPP_MSB(3000));
    T_OK(hidpp_illumination_levels(dev, COLOR, &l));
    T_EQ(l.count, 4);
    fake_free(f);
    return 0;
}

static int test_levels_validation(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_v1(&f);
    T_NOT_NULL(hidpp_illumination_info(dev, BRIGHTNESS));
    fake_reset_log(f);
    struct hidpp_illumination_levels l;

    memset(&l, 0, sizeof(l));
    T_ERR(
        hidpp_illumination_set_levels(dev, BRIGHTNESS, &l), HIDPP_EINVAL
    ); /* none */

    memset(&l, 0, sizeof(l));
    l.count = 7; /* above maxLevels */
    for (int n = 0; n < 7; n++)
        l.values[n] = (uint16_t)(100 + 10 * n);
    T_ERR(hidpp_illumination_set_levels(dev, BRIGHTNESS, &l), HIDPP_EINVAL);

    memset(&l, 0, sizeof(l));
    l.count = 2;
    l.values[0] = 500;
    l.values[1] = 400; /* decreasing */
    T_ERR(hidpp_illumination_set_levels(dev, BRIGHTNESS, &l), HIDPP_EINVAL);

    memset(&l, 0, sizeof(l));
    l.count = 2;
    l.values[0] = 105;
    l.values[1] = 400; /* off the resolution */
    T_ERR(hidpp_illumination_set_levels(dev, BRIGHTNESS, &l), HIDPP_EINVAL);

    memset(&l, 0, sizeof(l));
    l.count = 2;
    l.values[0] = 100;
    l.values[1] = 2000; /* out of range */
    T_ERR(hidpp_illumination_set_levels(dev, BRIGHTNESS, &l), HIDPP_EINVAL);

    memset(&l, 0, sizeof(l));
    l.linear = 1;
    l.min = 100;
    l.max = 500;
    l.step = 0;
    T_ERR(hidpp_illumination_set_levels(dev, BRIGHTNESS, &l), HIDPP_EINVAL);
    l.step = 150; /* does not divide the range */
    T_ERR(hidpp_illumination_set_levels(dev, BRIGHTNESS, &l), HIDPP_EINVAL);
    l.min = 500;
    l.max = 100;
    l.step = 100;
    T_ERR(hidpp_illumination_set_levels(dev, BRIGHTNESS, &l), HIDPP_EINVAL);

    /* non-linear levels do not exist for color temperature */
    memset(&l, 0, sizeof(l));
    l.count = 1;
    l.values[0] = 3000;
    T_ERR(hidpp_illumination_set_levels(dev, COLOR, &l), HIDPP_EINVAL);

    T_ERR(hidpp_illumination_set_levels(dev, BRIGHTNESS, NULL), HIDPP_EINVAL);
    T_EQ(f->requests, 0);
    fake_free(f);
    return 0;
}

static int test_levels_unsupported(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_v1(&f);
    f->ill[1].flags = 0x01; /* events only */
    hidpp_cache_clear(dev);
    struct hidpp_illumination_levels l = { 0 };
    T_ERR(hidpp_illumination_levels(dev, COLOR, &l), HIDPP_ENOSYS);
    T_ERR(hidpp_illumination_set_levels(dev, COLOR, &l), HIDPP_ENOSYS);
    T_ERR(hidpp_illumination_reset_levels(dev, COLOR), HIDPP_ENOSYS);
    fake_free(f);
    return 0;
}

static int test_reset_levels(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_v1(&f);
    f->ill[1].levelCount = 9;
    T_OK(hidpp_illumination_reset_levels(dev, COLOR));
    T_FALSE(f->ill[1].linear);
    T_EQ(f->ill[1].levelCount, 4);

    const struct fake_packet *p = fake_last(f);
    T_EQ(p->raw[3] >> 4, 11);
    T_EQ(p->raw[4], 0x02);
    fake_free(f);
    return 0;
}

static int test_effective_max(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_v1(&f);
    T_EQ(hidpp_illumination_effective_max(dev), 800);
    f->illEffMax = 0;
    T_EQ(hidpp_illumination_effective_max(dev), 0);
    f->illBusy = 1;
    T_ERR(hidpp_illumination_effective_max(dev), HIDPP_EAGAIN);
    T_EQ(hidpp_error_code(f->rcv), HIDPP_ERR_BUSY);
    fake_free(f);
    return 0;
}

static int test_unsupported_and_null(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    struct hidpp_illumination_levels l = { 0 };
    T_NULL(hidpp_illumination_info(dev, BRIGHTNESS));
    T_ERR(hidpp_illumination_get(dev), HIDPP_ENOSYS);
    T_ERR(hidpp_illumination_set(dev, 1), HIDPP_ENOSYS);
    T_ERR(hidpp_illumination_value(dev, BRIGHTNESS), HIDPP_ENOSYS);
    T_ERR(hidpp_illumination_set_value(dev, BRIGHTNESS, 1), HIDPP_ENOSYS);
    T_ERR(hidpp_illumination_levels(dev, BRIGHTNESS, &l), HIDPP_ENOSYS);
    T_ERR(hidpp_illumination_effective_max(dev), HIDPP_ENOSYS);
    fake_free(f);

    T_NULL(hidpp_illumination_info(NULL, BRIGHTNESS));
    T_ERR(hidpp_illumination_get(NULL), HIDPP_EINVAL);
    T_ERR(hidpp_illumination_set(NULL, 1), HIDPP_EINVAL);
    T_ERR(hidpp_illumination_value(NULL, 0), HIDPP_EINVAL);
    T_ERR(hidpp_illumination_levels(NULL, 0, &l), HIDPP_EINVAL);
    T_ERR(hidpp_illumination_effective_max(NULL), HIDPP_EINVAL);
    return 0;
}

static struct hidpp_event poll_one(
    fake_t *f, hidpp_device_t *dev, uint8_t func, const uint8_t *data, size_t n
) {
    struct hidpp_event e;
    fake_notify(f, FAKE_IDX_ILLUMINATION, func, data, n);
    T_OK(hidpp_device_poll(dev, &e));
    return e;
}

static int test_events(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_all();
    hidpp_device_t *dev = t_open(f);
    uint16_t ids[32];
    T_TRUE(hidpp_feature_list(dev, ids, 32) > 0);

    const uint8_t state[] = { 1 };
    struct hidpp_event e = poll_one(f, dev, 0, state, 1);
    T_EQ(e.type, HIDPP_EVENT_ILLUMINATION);
    T_EQ(e.as.illumination.kind, HIDPP_ILLUMINATION_EVENT_STATE);
    T_EQ(e.as.illumination.state, 1);

    const uint8_t value[] = { 0x12, 0x34 };
    e = poll_one(f, dev, 1, value, 2);
    T_EQ(e.type, HIDPP_EVENT_ILLUMINATION);
    T_EQ(e.as.illumination.kind, HIDPP_ILLUMINATION_EVENT_BRIGHTNESS);
    T_EQ(e.as.illumination.value, 0x1234);

    e = poll_one(f, dev, 2, value, 2);
    T_EQ(e.as.illumination.kind, HIDPP_ILLUMINATION_EVENT_COLOR_TEMPERATURE);
    T_EQ(e.as.illumination.value, 0x1234);

    e = poll_one(f, dev, 3, value, 2);
    T_EQ(e.as.illumination.kind, HIDPP_ILLUMINATION_EVENT_BRIGHTNESS_MAX);

    /* the layout of the clamped event is not documented: stays raw */
    e = poll_one(f, dev, 4, value, 2);
    T_EQ(e.type, HIDPP_EVENT_UNKNOWN);
    fake_free(f);
    return 0;
}

static int test_info_is_cached(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_v1(&f);
    T_NOT_NULL(hidpp_illumination_info(dev, BRIGHTNESS));
    fake_reset_log(f);
    for (int n = 0; n < 5; n++) {
        T_NOT_NULL(hidpp_illumination_info(dev, BRIGHTNESS));
        T_NOT_NULL(hidpp_illumination_info(dev, COLOR));
    }
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

    struct hidpp_illumination_info *b = hidpp_illumination_info(
        dev, BRIGHTNESS
    );
    struct hidpp_illumination_info *c = hidpp_illumination_info(dev, COLOR);
    T_NOT_NULL(b);
    T_NOT_NULL(c);
    T_EQ(b->max, 1000);
    T_EQ(b->maxLevels, 6);
    T_TRUE(b->hasDynamicMaximum && b->hasNonLinearLevels);
    T_EQ(c->min, 2700);
    T_FALSE(c->hasNonLinearLevels);
    T_EQ(f->requests, 0);

    T_EQ(hidpp_illumination_value(dev, BRIGHTNESS), 500);
    fake_free(f);
    return 0;
}

static int test_garbage_responses(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_v1(&f);
    f->garbage = 1;
    f->garbageState = (uint32_t)seed | 1;
    struct hidpp_illumination_levels l;

    for (int n = 0; n < 30; n++) {
        for (int c = 0; c < 2; c++) {
            (void)hidpp_illumination_info(dev, c);
            (void)hidpp_illumination_value(dev, c);
            (void)hidpp_illumination_set_value(dev, c, (uint16_t)(n * 100));
            if (!hidpp_illumination_levels(dev, c, &l))
                (void)hidpp_illumination_set_levels(dev, c, &l);
        }
        (void)hidpp_illumination_get(dev);
        (void)hidpp_illumination_effective_max(dev);
        hidpp_cache_clear(dev);
    }
    return 0;
}

static const pf_test_t illumination_tests[] = {
    { test_info, "info" },
    { test_info_bytes, "info_bytes" },
    { test_info_v0_has_no_dynamic_maximum, "info_v0_no_dynamic_max" },
    { test_single_value_control, "single_value_control" },
    { test_state, "state" },
    { test_value, "value" },
    { test_function_numbers, "function_numbers" },
    { test_set_value_validation, "set_value_validation" },
    { test_levels_non_linear, "levels_non_linear" },
    { test_levels_wire_format, "levels_wire_format" },
    { test_levels_are_split_into_chunks, "levels_chunks" },
    { test_levels_linear, "levels_linear" },
    { test_levels_validation, "levels_validation" },
    { test_levels_unsupported, "levels_unsupported" },
    { test_reset_levels, "reset_levels" },
    { test_effective_max, "effective_max" },
    { test_unsupported_and_null, "unsupported_and_null" },
    { test_events, "events" },
    { test_info_is_cached, "info_is_cached" },
    { test_cache_roundtrip, "cache_roundtrip" },
    { test_garbage_responses, "garbage_responses" },
    { 0 },
};

const pf_suite_t suite_illumination = { "illumination", 1, illumination_tests };
