/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

/*  Touch surfaces: touchpad raw XY (0x6100) and touch mouse (0x6110). */

#include "test.h"

/* ----------------------------- touchpad ------------------------------ */

static int touchpad_index;

static fake_t *make_touchpad(void) {
    fake_t *f = t_new();
    fake_add_firmware_info(f);
    touchpad_index = t_add_canned(f, 0x6100, 0);
    const uint8_t info[] = {
        0x0F, 0xA0, 0x0B, 0xB8, 0x0F, 0x08, 10, 5, HIDPP_ORIGIN_UPPER_LEFT,
        1,    0,    0,    2,    0x01, 0x90
    };
    t_answer(touchpad_index, 0, info, sizeof(info));
    t_answer(touchpad_index, 1, (const uint8_t[]) { 0x05 }, 1);
    return f;
}

static int test_touchpad_info(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_touchpad();
    hidpp_device_t *dev = t_open(f);
    struct hidpp_touchpad_info *info = hidpp_touchpad_info(dev);
    T_NOT_NULL(info);
    T_EQ(info->width, 4000);
    T_EQ(info->height, 3000);
    T_EQ(info->zRange, 0x0F);
    T_EQ(info->areaRange, 0x08);
    T_EQ(info->timestampUnit, 10);
    T_EQ(info->maxFingers, 5);
    T_EQ(info->origin, HIDPP_ORIGIN_UPPER_LEFT);
    T_TRUE(info->hasPen);
    T_EQ(info->mappingVersion, 2);
    T_EQ(info->dpi, 400);
    fake_free(f);
    return 0;
}

static int test_touchpad_mode(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_touchpad();
    hidpp_device_t *dev = t_open(f);
    T_EQ(
        hidpp_touchpad_get_mode(dev),
        HIDPP_TOUCHPAD_RAW | HIDPP_TOUCHPAD_ENHANCED
    );

    T_OK(hidpp_touchpad_set_mode(
        dev, HIDPP_TOUCHPAD_RAW | HIDPP_TOUCHPAD_MAJOR_MINOR
    ));
    T_EQ(T_FUNC(f), 2);
    T_EQ(T_PARAM(f, 0), 0x21);
    T_ERR(hidpp_touchpad_set_mode(dev, 0x80), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

static int test_touchpad_unsupported_and_cache(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = t_new();
    hidpp_device_t *dev = t_open(f);
    T_NULL(hidpp_touchpad_info(dev));
    T_ERR(hidpp_touchpad_get_mode(dev), HIDPP_ENOSYS);
    T_ERR(hidpp_touchpad_set_mode(NULL, 0), HIDPP_EINVAL);
    fake_free(f);

    dev = t_from_cache(make_touchpad, &f);
    T_EQ(hidpp_touchpad_info(dev)->dpi, 400);
    T_TRUE(hidpp_touchpad_info(dev)->hasPen);
    T_EQ(f->requests, 0);
    fake_free(f);
    return 0;
}

static const pf_test_t touchpad_tests[] = {
    { test_touchpad_info, "info" },
    { test_touchpad_mode, "mode" },
    { test_touchpad_unsupported_and_cache, "unsupported_and_cache" },
    { 0 },
};

const pf_suite_t suite_touchpad = { "touchpad", 1, touchpad_tests };

/* ---------------------------- touch mouse ---------------------------- */

static int touch_mouse_index;

static fake_t *make_touch_mouse(void) {
    fake_t *f = t_new();
    fake_add_firmware_info(f);
    touch_mouse_index = t_add_canned(f, 0x6110, 0);
    const uint8_t info[] = {
        0x02, 0x00, 0x01, 0x80, 0x01, 0x90, HIDPP_ORIGIN_LOWER_LEFT, 4, 15
    };
    t_answer(touch_mouse_index, 0, info, sizeof(info));
    t_answer(touch_mouse_index, 1, (const uint8_t[]) { 3 }, 1);
    return f;
}

static int test_touch_mouse_info(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_touch_mouse();
    hidpp_device_t *dev = t_open(f);
    struct hidpp_touch_mouse_info *info = hidpp_touch_mouse_info(dev);
    T_NOT_NULL(info);
    T_EQ(info->xMax, 512);
    T_EQ(info->yMax, 384);
    T_EQ(info->dpi, 400);
    T_EQ(info->origin, HIDPP_ORIGIN_LOWER_LEFT);
    T_EQ(info->maxFingers, 4);
    T_EQ(info->contactMax, 15);
    fake_free(f);
    return 0;
}

static int test_touch_mouse_mode(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_touch_mouse();
    hidpp_device_t *dev = t_open(f);
    T_EQ(hidpp_touch_mouse_get_mode(dev), HIDPP_TOUCH_MOUSE_RAW_ALWAYS);
    T_OK(hidpp_touch_mouse_set_mode(dev, HIDPP_TOUCH_MOUSE_RAW_GESTURES_Z));
    T_EQ(T_FUNC(f), 2);
    T_EQ(T_PARAM(f, 0), 4);
    T_ERR(hidpp_touch_mouse_set_mode(dev, 5), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

/* The four points use four bytes each, the last one ends the payload. */
static int test_touch_mouse_points(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_touch_mouse();
    hidpp_device_t *dev = t_open(f);
    const uint8_t data[16] = {
        0x12, 0x34, 0x65, 0x21, /* x 0x125, y 0x346, wx 1, wy 2 */
        0xFF, 0xFF, 0xFF, 0xFF, /* lifted */
        0x00, 0x01, 0xF0, 0x00, /* x 0x000, y 0x01F */
        0xAB, 0xCD, 0x1E, 0xF3, /* x 0xABE, y 0xCD1, wx 3, wy 15 */
    };
    struct hidpp_event e = t_event(f, dev, touch_mouse_index, 0, data, 16);
    T_EQ(e.type, HIDPP_EVENT_TOUCH_MOUSE_POINTS);
    T_EQ(e.as.touchMousePoints[0].x, 0x125);
    T_EQ(e.as.touchMousePoints[0].y, 0x346);
    T_EQ(e.as.touchMousePoints[0].wx, 1);
    T_EQ(e.as.touchMousePoints[0].wy, 2);
    T_EQ(e.as.touchMousePoints[1].x, 0xFFF);
    T_EQ(e.as.touchMousePoints[2].x, 0x000);
    T_EQ(e.as.touchMousePoints[2].y, 0x01F);
    T_EQ(e.as.touchMousePoints[3].x, 0xABE);
    T_EQ(e.as.touchMousePoints[3].y, 0xCD1);
    T_EQ(e.as.touchMousePoints[3].wx, 3);
    T_EQ(e.as.touchMousePoints[3].wy, 15);
    fake_free(f);
    return 0;
}

static int test_touch_mouse_unsupported_and_cache(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = t_new();
    hidpp_device_t *dev = t_open(f);
    T_NULL(hidpp_touch_mouse_info(dev));
    T_ERR(hidpp_touch_mouse_get_mode(dev), HIDPP_ENOSYS);
    fake_free(f);

    dev = t_from_cache(make_touch_mouse, &f);
    T_EQ(hidpp_touch_mouse_info(dev)->yMax, 384);
    T_EQ(f->requests, 0);
    fake_free(f);
    return 0;
}

static const pf_test_t touch_mouse_tests[] = {
    { test_touch_mouse_info, "info" },
    { test_touch_mouse_mode, "mode" },
    { test_touch_mouse_points, "points" },
    { test_touch_mouse_unsupported_and_cache, "unsupported_and_cache" },
    { 0 },
};

const pf_suite_t suite_touch_mouse = { "touch_mouse", 1, touch_mouse_tests };
