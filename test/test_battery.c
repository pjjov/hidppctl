/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

/*  Battery status (0x1000). */

#include "test.h"

static int fail_battery_hook(
    hidpp_receiver_t *rcv, hidpp_packet_t *req, hidpp_packet_t *res, void *user
) {
    (void)rcv, (void)user;
    if (res && req && req->feat == FAKE_IDX_BATTERY
        && HIDPP_MSN(req->func) == 0)
        return HIDPP_EIO;
    return 0;
}

static int test_info(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);

    struct hidpp_battery_info *info = hidpp_battery_info(dev);
    T_NOT_NULL(info);
    T_TRUE(info->initialized);
    T_EQ(info->levelCount, 4);
    T_EQ(info->flags, 0x06);
    T_EQ(info->nominalLife, 3000);
    T_EQ(info->criticalLevel, 5);
    T_FALSE(info->disabledOSD);
    T_TRUE(info->enabledMileage);
    T_TRUE(info->isRechargable);
    fake_free(f);
    return 0;
}

static int test_info_fields_come_from_the_right_bytes(int seed, int i) {
    (void)seed, (void)i;
    /* every byte distinct, so any off-by-one shows up */
    fake_t *f = fake_new();
    f->batLevelCount = 0x11;
    f->batFlags = 0x01;
    f->batNominal = 0x2233;
    f->batCritical = 0x44;
    hidpp_device_t *dev = t_open(f);

    struct hidpp_battery_info *info = hidpp_battery_info(dev);
    T_NOT_NULL(info);
    T_EQ(info->levelCount, 0x11);
    T_EQ(info->flags, 0x01);
    T_EQ(info->nominalLife, 0x2233);
    T_EQ(info->criticalLevel, 0x44);
    fake_free(f);
    return 0;
}

static int test_info_nominal_life_is_16_bit(int seed, int i) {
    (void)seed, (void)i;
    static const uint16_t values[] = { 0, 1, 255, 256, 3000, 0x8000, 0xFFFF };

    for (size_t n = 0; n < sizeof(values) / sizeof(values[0]); n++) {
        fake_t *f = fake_new();
        f->batNominal = values[n];
        hidpp_device_t *dev = t_open(f);
        T_EQ(hidpp_battery_info(dev)->nominalLife, values[n]);
        fake_free(f);
    }
    return 0;
}

static int test_info_flags(int seed, int i) {
    (void)seed, (void)i;

    for (int flags = 0; flags < 8; flags++) {
        fake_t *f = fake_new();
        f->batFlags = (uint8_t)flags;
        hidpp_device_t *dev = t_open(f);
        struct hidpp_battery_info *info = hidpp_battery_info(dev);
        T_NOT_NULL(info);
        T_EQ(info->disabledOSD, !!(flags & 0x1));
        T_EQ(info->enabledMileage, !!(flags & 0x2));
        T_EQ(info->isRechargable, !!(flags & 0x4));
        fake_free(f);
    }
    return 0;
}

static int test_info_is_cached(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    struct hidpp_battery_info *a = hidpp_battery_info(dev);
    T_NOT_NULL(a);
    fake_reset_log(f);

    for (int n = 0; n < 10; n++)
        T_TRUE(hidpp_battery_info(dev) == a);
    T_EQ(f->requests, 0);
    fake_free(f);
    return 0;
}

static int test_status(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    f->batLevel = 75;
    f->batNext = 50;
    f->batStatus = HIDPP_BATTERY_RECHARGING;
    hidpp_device_t *dev = t_open(f);

    struct hidpp_battery_status *s = hidpp_battery_status(dev);
    T_NOT_NULL(s);
    T_EQ(s->dischargeLevel, 75);
    T_EQ(s->dischargeNextLevel, 50);
    T_EQ(s->status, HIDPP_BATTERY_RECHARGING);
    T_STR(s->statusName, "Recharging");
    T_STR(s->levelName, "Good");
    fake_free(f);
    return 0;
}

static int test_status_is_read_from_the_right_bytes(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    f->batLevel = 41;
    f->batNext = 22;
    f->batStatus = 3;
    hidpp_device_t *dev = t_open(f);

    struct hidpp_battery_status *s = hidpp_battery_status(dev);
    T_NOT_NULL(s);
    T_EQ(s->dischargeLevel, 41);
    T_EQ(s->dischargeNextLevel, 22);
    T_EQ(s->status, 3);
    fake_free(f);
    return 0;
}

static int test_status_is_never_cached(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);

    for (uint8_t level = 100; level > 0; level -= 11) {
        f->batLevel = level;
        struct hidpp_battery_status *s = hidpp_battery_status(dev);
        T_NOT_NULL(s);
        T_EQ(s->dischargeLevel, level);
    }
    fake_free(f);
    return 0;
}

static int test_status_costs_one_request(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    T_NOT_NULL(hidpp_battery_info(dev));
    fake_reset_log(f);
    T_NOT_NULL(hidpp_battery_status(dev));
    T_EQ(f->requests, 1);
    fake_free(f);
    return 0;
}

static int test_level_names(int seed, int i) {
    (void)seed, (void)i;
    static const struct {
        uint8_t level;
        const char *name;
    } cases[] = {
        { 0, "Critical" },  { 1, "Critical" },  { 10, "Critical" },
        { 11, "Low" },      { 20, "Low" },      { 30, "Low" },
        { 31, "Good" },     { 50, "Good" },     { 80, "Good" },
        { 81, "Full" },     { 99, "Full" },     { 100, "Full" },
        { 101, "Unknown" }, { 200, "Unknown" }, { 255, "Unknown" },
    };

    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);

    for (size_t n = 0; n < sizeof(cases) / sizeof(cases[0]); n++) {
        f->batLevel = cases[n].level;
        struct hidpp_battery_status *s = hidpp_battery_status(dev);
        T_NOT_NULL(s);
        T_STR(s->levelName, cases[n].name);
    }
    fake_free(f);
    return 0;
}

static int test_status_names(int seed, int i) {
    (void)seed, (void)i;
    static const char *const names[] = {
        "Discharging",
        "Recharging",
        "Charge in final state",
        "Charge complete",
        "Recharging below optimal speed",
        "Invalid battery type",
        "Thermal error",
        "Other charging error",
    };

    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);

    for (int status = 0; status < 8; status++) {
        f->batStatus = (uint8_t)status;
        struct hidpp_battery_status *s = hidpp_battery_status(dev);
        T_NOT_NULL(s);
        T_EQ(s->status, status);
        T_STR(s->statusName, names[status]);
    }

    f->batStatus = 8;
    T_STR(hidpp_battery_status(dev)->statusName, "Unknown");
    f->batStatus = 255;
    T_STR(hidpp_battery_status(dev)->statusName, "Unknown");
    fake_free(f);
    return 0;
}

static int test_status_device_failure(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    T_NOT_NULL(hidpp_battery_info(dev));

    hidpp_set_protocol_hook(f->rcv, fail_battery_hook, NULL);
    T_NULL(hidpp_battery_status(dev));
    hidpp_set_protocol_hook(f->rcv, NULL, NULL);
    T_NOT_NULL(hidpp_battery_status(dev)); /* recovers afterwards */
    fake_free(f);
    return 0;
}

static int test_status_device_silent(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    T_NOT_NULL(hidpp_battery_info(dev));
    f->silent = 1;
    T_NULL(hidpp_battery_status(dev));
    fake_free(f);
    return 0;
}

static int test_unsupported_feature(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    hidpp_device_t *dev = t_open(f);

    for (int n = 0; n < 3; n++) {
        T_NULL(hidpp_battery_info(dev));
        T_NULL(hidpp_battery_status(dev));
    }
    fake_free(f);
    return 0;
}

static int test_status_first_on_unsupported_device(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    hidpp_device_t *dev = t_open(f);
    /* must not fall through and query feature index 0 (the root) */
    T_NULL(hidpp_battery_status(dev));
    fake_reset_log(f);
    T_NULL(hidpp_battery_status(dev));
    T_EQ(fake_calls(f, 0), 0);
    fake_free(f);
    return 0;
}

static int test_null_device(int seed, int i) {
    (void)seed, (void)i;
    T_NULL(hidpp_battery_info(NULL));
    T_NULL(hidpp_battery_status(NULL));
    return 0;
}

static int test_status_before_info(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    T_NOT_NULL(hidpp_battery_status(dev));
    struct hidpp_battery_info *info = hidpp_battery_info(dev);
    T_NOT_NULL(info);
    T_EQ(info->levelCount, 4);
    fake_free(f);
    return 0;
}

static const pf_test_t battery_tests[] = {
    { test_info, "info" },
    { test_info_fields_come_from_the_right_bytes, "info_right_bytes" },
    { test_info_nominal_life_is_16_bit, "info_nominal_life_16_bit" },
    { test_info_flags, "info_flags" },
    { test_info_is_cached, "info_is_cached" },
    { test_status, "status" },
    { test_status_is_read_from_the_right_bytes, "status_right_bytes" },
    { test_status_is_never_cached, "status_is_never_cached" },
    { test_status_costs_one_request, "status_costs_one_request" },
    { test_level_names, "level_names" },
    { test_status_names, "status_names" },
    { test_status_device_failure, "status_device_failure" },
    { test_status_device_silent, "status_device_silent" },
    { test_unsupported_feature, "unsupported_feature" },
    { test_status_first_on_unsupported_device, "status_first_on_unsupported" },
    { test_null_device, "null_device" },
    { test_status_before_info, "status_before_info" },
    { 0 },
};

const pf_suite_t suite_battery = { "battery", 1, battery_tests };
