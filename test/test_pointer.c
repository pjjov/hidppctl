/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

/*  Pointer: mouse pointer information (0x2200) and extended adjustable
    DPI (0x2202). */

#include "test.h"

/* ----------------------------- pointer ------------------------------- */

static int pointer_index;

static fake_t *make_pointer(void) {
    fake_t *f = t_new();
    fake_add_firmware_info(f);
    pointer_index = t_add_canned(f, 0x2200, 0);
    /* 1000 DPI, vertical tuning + OS ballistics + medium acceleration */
    t_answer(pointer_index, 0, (const uint8_t[]) { 0x03, 0xE8, 0x0E }, 3);
    return f;
}

static int test_pointer_info(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_pointer();
    hidpp_device_t *dev = t_open(f);
    struct hidpp_pointer_info *info = hidpp_pointer_info(dev);
    T_NOT_NULL(info);
    T_EQ(info->resolution, 1000);
    T_EQ(info->flags, 0x0E);
    T_EQ(info->acceleration, HIDPP_POINTER_ACCEL_MEDIUM);
    T_TRUE(info->suggestOsBallistics && info->suggestVerticalTuning);
    fake_free(f);
    return 0;
}

static int test_pointer_flag_bits(int seed, int i) {
    (void)seed, (void)i;

    for (int bit = 0; bit < 4; bit++) {
        fake_t *f = make_pointer();
        t_answer(pointer_index, 0, (const uint8_t[]) { 0, 0, 1u << bit }, 3);
        hidpp_device_t *dev = t_open(f);
        struct hidpp_pointer_info *info = hidpp_pointer_info(dev);
        T_EQ(info->acceleration, bit < 2 ? 1u << bit : 0);
        T_EQ(info->suggestOsBallistics, bit == 2);
        T_EQ(info->suggestVerticalTuning, bit == 3);
        fake_free(f);
    }
    return 0;
}

static int test_pointer_unsupported_and_cache(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = t_new();
    hidpp_device_t *dev = t_open(f);
    T_NULL(hidpp_pointer_info(dev));
    T_NULL(hidpp_pointer_info(NULL));
    fake_free(f);

    dev = t_from_cache(make_pointer, &f);
    T_EQ(hidpp_pointer_info(dev)->resolution, 1000);
    T_TRUE(hidpp_pointer_info(dev)->suggestVerticalTuning);
    T_EQ(f->requests, 0);
    fake_free(f);
    return 0;
}

static const pf_test_t pointer_tests[] = {
    { test_pointer_info, "info" },
    { test_pointer_flag_bits, "flag_bits" },
    { test_pointer_unsupported_and_cache, "unsupported_and_cache" },
    { 0 },
};

const pf_suite_t suite_pointer = { "pointer", 1, pointer_tests };

/* ------------------------------- dpi --------------------------------- */

static struct {
    uint8_t sensorCount;
    uint8_t flags[2];
    uint8_t levels[2];
    uint8_t ranges[2][2][32]; /* sensor, direction, words with a 0 end */
    unsigned rangeRequests;
} dpi_state;

static void put_words(uint8_t *out, const uint16_t *words, size_t n) {
    for (size_t k = 0; k < n; k++) {
        out[2 * k] = HIDPP_MSB(words[k]);
        out[2 * k + 1] = HIDPP_LSB(words[k]);
    }
}

static int dpi_handler(
    fake_t *f, uint8_t func, const hidpp_packet_t *req, hidpp_packet_t *res
) {
    (void)f;
    uint8_t sensor = req->params[0];

    if (func != 0 && sensor >= dpi_state.sensorCount)
        return HIDPP_ERR_INVALID_ARGUMENT;

    res->params[0] = sensor;

    switch (func) {
    case 0:
        res->params[0] = dpi_state.sensorCount;
        return 0;
    case 1:
        res->params[1] = dpi_state.levels[sensor];
        res->params[2] = dpi_state.flags[sensor];
        return 0;
    case 2: {
        uint8_t dir = req->params[1], chunk = req->params[2];
        dpi_state.rangeRequests++;

        if (dir > 1 || chunk * 13 >= sizeof(dpi_state.ranges[0][0]))
            return HIDPP_ERR_INVALID_ARGUMENT;

        res->params[1] = dir;
        res->params[2] = chunk;
        size_t n = sizeof(dpi_state.ranges[0][0]) - chunk * 13;
        memcpy(
            &res->params[3],
            &dpi_state.ranges[sensor][dir][chunk * 13],
            n < 13 ? n : 13
        );
        return 0;
    }
    case 3:
        res->params[1] = req->params[1];
        put_words(&res->params[2], (const uint16_t[]) { 800, 1600, 3200 }, 3);
        return 0;
    case 4:
        res->params[1] = 1;
        res->params[2] = 2;
        res->params[3] = 3;
        return 0;
    case 5:
        put_words(
            &res->params[1], (const uint16_t[]) { 1600, 1000, 1600, 1000 }, 4
        );
        res->params[9] = 2;
        return 0;
    case 8:
        res->params[1] = 60;
        put_words(&res->params[2], (const uint16_t[]) { 120, 1000, 1000 }, 3);
        return 0;
    case 6:
    case 7:
    case 9:
    case 10:
        return 0;
    default:
        return HIDPP_ERR_INVALID_FUNCTION_ID;
    }
}

static int dpi_index;

static fake_t *make_dpi(void) {
    fake_t *f = t_new();
    fake_add_firmware_info(f);
    memset(&dpi_state, 0, sizeof(dpi_state));
    dpi_state.sensorCount = 2;
    dpi_state.flags[0] = 0x0F;
    dpi_state.levels[0] = 4;

    /* 200-1000 by 50, 1000-8000 by 100, then 12000; spans two requests */
    put_words(
        dpi_state.ranges[0][0],
        (const uint16_t[]) { 200, 0xE032, 1000, 0xE064, 8000, 12000 },
        6
    );
    put_words(dpi_state.ranges[0][1], (const uint16_t[]) { 400 }, 1);
    dpi_index = fake_add_feature(f, 0x2202, 0, dpi_handler);
    return f;
}

static int test_dpi_sensors(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_dpi();
    hidpp_device_t *dev = t_open(f);
    T_EQ(hidpp_dpi_sensor_count(dev), 2);

    struct hidpp_dpi_sensor *s = hidpp_dpi_sensor(dev, 0);
    T_NOT_NULL(s);
    T_EQ(s->levelCount, 4);
    T_TRUE(s->hasY && s->hasLod && s->hasCalibration && s->hasProfile);

    s = hidpp_dpi_sensor(dev, 1);
    T_NOT_NULL(s);
    T_FALSE(s->hasY || s->hasLod || s->hasCalibration || s->hasProfile);
    T_NULL(hidpp_dpi_sensor(dev, 2));
    fake_free(f);
    return 0;
}

static int test_dpi_ranges(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_dpi();
    hidpp_device_t *dev = t_open(f);
    struct hidpp_dpi_range r[8];

    T_EQ(hidpp_dpi_ranges(dev, 0, HIDPP_DPI_X, r, 8), 3);
    T_EQ(r[0].min, 200);
    T_EQ(r[0].max, 1000);
    T_EQ(r[0].step, 50);
    T_EQ(r[1].min, 1000);
    T_EQ(r[1].max, 8000);
    T_EQ(r[1].step, 100);
    T_EQ(r[2].min, 12000);
    T_EQ(r[2].max, 12000);
    T_EQ(r[2].step, 0);

    T_EQ(hidpp_dpi_ranges(dev, 0, HIDPP_DPI_Y, r, 8), 1);
    T_EQ(r[0].min, 400);
    T_EQ(hidpp_dpi_ranges(dev, 0, HIDPP_DPI_X, r, 2), 2);

    /* the second sensor has no Y resolution */
    T_ERR(hidpp_dpi_ranges(dev, 1, HIDPP_DPI_Y, r, 8), HIDPP_EINVAL);
    T_EQ(hidpp_dpi_ranges(dev, 1, HIDPP_DPI_X, r, 8), 0);

    /* two requests for the X list of sensor 0, one for each other list */
    T_EQ(dpi_state.rangeRequests, 2 + 1 + 1);
    fake_free(f);
    return 0;
}

static int test_dpi_levels_and_lods(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_dpi();
    hidpp_device_t *dev = t_open(f);
    uint16_t levels[8];
    T_EQ(hidpp_dpi_levels(dev, 0, HIDPP_DPI_Y, levels, 8), 3);
    T_EQ(T_FUNC(f), 3);
    T_EQ(T_PARAM(f, 1), HIDPP_DPI_Y);
    T_EQ(levels[0], 800);
    T_EQ(levels[2], 3200);
    T_EQ(hidpp_dpi_levels(dev, 0, HIDPP_DPI_X, levels, 2), 2);

    uint8_t lods[4];
    T_EQ(hidpp_dpi_lods(dev, 0, lods, 4), 3);
    T_EQ(lods[0], HIDPP_DPI_LOD_LOW);
    T_EQ(lods[2], HIDPP_DPI_LOD_HIGH);
    T_ERR(hidpp_dpi_lods(dev, 1, lods, 4), HIDPP_ENOSYS);
    fake_free(f);
    return 0;
}

static int test_dpi_get_set(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_dpi();
    hidpp_device_t *dev = t_open(f);

    struct hidpp_dpi d;
    T_OK(hidpp_dpi_get(dev, 0, &d));
    T_EQ(d.x, 1600);
    T_EQ(d.defaultX, 1000);
    T_EQ(d.y, 1600);
    T_EQ(d.defaultY, 1000);
    T_EQ(d.lod, HIDPP_DPI_LOD_MEDIUM);

    /* without a Y resolution and LOD those are reported as 0 */
    T_OK(hidpp_dpi_get(dev, 1, &d));
    T_EQ(d.y, 0);
    T_EQ(d.lod, 0);

    T_OK(hidpp_dpi_set(dev, 0, 950, 400, 3));
    T_EQ(T_FUNC(f), 6);
    T_EQ(T_PARAM(f, 0), 0);
    T_EQ(HIDPP_WORD(T_PARAM(f, 1), T_PARAM(f, 2)), 950);
    T_EQ(HIDPP_WORD(T_PARAM(f, 3), T_PARAM(f, 4)), 400);
    T_EQ(T_PARAM(f, 5), 3);
    T_OK(hidpp_dpi_set(dev, 0, 12000, 400, 0));
    T_OK(hidpp_dpi_set(dev, 1, HIDPP_DPI_MAX, 0, 0));
    fake_free(f);
    return 0;
}

static int test_dpi_set_validation(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_dpi();
    hidpp_device_t *dev = t_open(f);
    T_EQ(hidpp_dpi_sensor_count(dev), 2);
    unsigned before = f->requests;

    T_ERR(hidpp_dpi_set(dev, 0, 1050, 400, 0), HIDPP_EINVAL); /* step */
    T_ERR(hidpp_dpi_set(dev, 0, 150, 400, 0), HIDPP_EINVAL);  /* below */
    T_ERR(hidpp_dpi_set(dev, 0, 12100, 400, 0), HIDPP_EINVAL);
    T_ERR(hidpp_dpi_set(dev, 0, 1000, 500, 0), HIDPP_EINVAL); /* Y */
    T_ERR(hidpp_dpi_set(dev, 0, 1000, 400, 4), HIDPP_EINVAL); /* LOD */
    T_ERR(hidpp_dpi_set(dev, 1, 1000, 400, 0), HIDPP_EINVAL); /* no Y */
    T_ERR(hidpp_dpi_set(dev, 1, 1000, 0, 1), HIDPP_EINVAL);   /* no LOD */
    T_ERR(hidpp_dpi_set(dev, 1, 0, 0, 0), HIDPP_EINVAL);
    T_ERR(hidpp_dpi_set(dev, 1, HIDPP_DPI_MAX + 1, 0, 0), HIDPP_EINVAL);
    T_ERR(hidpp_dpi_set(dev, 2, 1000, 0, 0), HIDPP_EINVAL);
    T_EQ(f->requests, before);
    fake_free(f);
    return 0;
}

static int test_dpi_show(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_dpi();
    hidpp_device_t *dev = t_open(f);
    T_OK(hidpp_dpi_show(dev, 0, 4, HIDPP_DPI_LED_EVENT, 6));
    T_EQ(T_FUNC(f), 7);
    T_EQ(T_PARAM(f, 1), 4);
    T_EQ(T_PARAM(f, 2), HIDPP_DPI_LED_EVENT);
    T_EQ(T_PARAM(f, 3), 6);
    T_ERR(hidpp_dpi_show(dev, 0, 5, 0, 1), HIDPP_EINVAL);
    T_ERR(hidpp_dpi_show(dev, 0, 0, 0, 1), HIDPP_EINVAL);
    T_ERR(hidpp_dpi_show(dev, 0, 1, 4, 1), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

static int test_dpi_calibration(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_dpi();
    hidpp_device_t *dev = t_open(f);

    struct hidpp_dpi_calibration_info info;
    T_OK(hidpp_dpi_calibration_info(dev, 0, &info));
    T_EQ(info.mouseWidth, 60);
    T_EQ(info.mouseLength, 120);
    T_EQ(info.dpiX, 1000);
    T_EQ(info.dpiY, 1000);
    T_ERR(hidpp_dpi_calibration_info(dev, 1, &info), HIDPP_ENOSYS);

    struct hidpp_dpi_calibration cal = { .direction = HIDPP_DPI_Y,
                                         .expectedCount = 0x1234,
                                         .type = 1,
                                         .startTimeout = 10,
                                         .hwProcessTimeout = 20,
                                         .swProcessTimeout = 60 };
    T_OK(hidpp_dpi_start_calibration(dev, 0, &cal));
    T_EQ(T_FUNC(f), 9);
    T_MEM(&fake_last(f)->raw[4], "\x00\x01\x12\x34\x01\x0A\x14\x3C", 8);
    cal.swProcessTimeout = 61;
    T_ERR(hidpp_dpi_start_calibration(dev, 0, &cal), HIDPP_EINVAL);
    cal.swProcessTimeout = 0, cal.type = 2;
    T_ERR(hidpp_dpi_start_calibration(dev, 0, &cal), HIDPP_EINVAL);

    T_OK(hidpp_dpi_set_calibration(dev, 0, HIDPP_DPI_X, -1023));
    T_EQ(T_FUNC(f), 10);
    T_EQ(HIDPP_WORD(T_PARAM(f, 2), T_PARAM(f, 3)), 0xFC01);
    T_ERR(hidpp_dpi_set_calibration(dev, 0, HIDPP_DPI_X, 1024), HIDPP_EINVAL);
    T_ERR(hidpp_dpi_set_calibration(dev, 1, HIDPP_DPI_X, 0), HIDPP_ENOSYS);
    fake_free(f);
    return 0;
}

static int test_dpi_events(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_dpi();
    hidpp_device_t *dev = t_open(f);
    const uint8_t params[] = { 1, 0x06, 0x40, 0x03, 0x20, 2 };
    struct hidpp_event e = t_event(f, dev, dpi_index, 0, params, 6);
    T_EQ(e.type, HIDPP_EVENT_DPI);
    T_EQ(e.as.dpi.sensor, 1);
    T_EQ(e.as.dpi.x, 1600);
    T_EQ(e.as.dpi.y, 800);
    T_EQ(e.as.dpi.lod, 2);

    const uint8_t cal[] = { 0, 1, 0xFF, 0xF6, 0x00, 0x20 };
    e = t_event(f, dev, dpi_index, 1, cal, 6);
    T_EQ(e.type, HIDPP_EVENT_DPI_CALIBRATION);
    T_EQ(e.as.dpiCalibration.direction, HIDPP_DPI_Y);
    T_EQ(e.as.dpiCalibration.correction, -10);
    T_EQ(e.as.dpiCalibration.delta, 32);
    fake_free(f);
    return 0;
}

static int test_dpi_unsupported_and_cache(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = t_new();
    hidpp_device_t *dev = t_open(f);
    struct hidpp_dpi d;
    T_ERR(hidpp_dpi_sensor_count(dev), HIDPP_ENOSYS);
    T_ERR(hidpp_dpi_get(dev, 0, &d), HIDPP_ENOSYS);
    T_NULL(hidpp_dpi_sensor(dev, 0));
    T_ERR(hidpp_dpi_sensor_count(NULL), HIDPP_EINVAL);
    fake_free(f);

    dev = t_from_cache(make_dpi, &f);
    struct hidpp_dpi_range r[4];
    T_EQ(hidpp_dpi_sensor_count(dev), 2);
    T_TRUE(hidpp_dpi_sensor(dev, 0)->hasCalibration);
    T_EQ(hidpp_dpi_ranges(dev, 0, HIDPP_DPI_X, r, 4), 3);
    T_EQ(r[1].step, 100);
    T_ERR(hidpp_dpi_set(dev, 0, 1050, 400, 0), HIDPP_EINVAL);
    T_EQ(f->requests, 0);
    fake_free(f);
    return 0;
}

static const pf_test_t dpi_tests[] = {
    { test_dpi_sensors, "sensors" },
    { test_dpi_ranges, "ranges" },
    { test_dpi_levels_and_lods, "levels_and_lods" },
    { test_dpi_get_set, "get_set" },
    { test_dpi_set_validation, "set_validation" },
    { test_dpi_show, "show" },
    { test_dpi_calibration, "calibration" },
    { test_dpi_events, "events" },
    { test_dpi_unsupported_and_cache, "unsupported_and_cache" },
    { 0 },
};

const pf_suite_t suite_dpi = { "dpi", 1, dpi_tests };
