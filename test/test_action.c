/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

/*  Persistent remappable action (0x1C00). */

#include "test.h"

#define CACHE_CAPACITY (128 * 1024)

static hidpp_device_t *open_all(fake_t **out) {
    *out = fake_new_all();
    return t_open(*out);
}

static int test_info(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_all(&f);
    struct hidpp_action_info *info = hidpp_action_info(dev);
    T_NOT_NULL(info);
    T_TRUE(info->initialized);
    T_EQ(info->flags, 0x01FF);
    T_EQ(info->count, 3);
    T_EQ(info->hostCount, 3);
    T_TRUE(info->canKey && info->canMouseButton);
    T_TRUE(info->canXDisplacement && info->canYDisplacement);
    T_TRUE(info->canVerticalRoller && info->canHorizontalRoller);
    T_TRUE(info->canConsumer && info->canInternal && info->canPower);
    fake_free(f);
    return 0;
}

static int test_capability_bits_follow_action_ids(int seed, int i) {
    (void)seed, (void)i;

    for (int bit = 0; bit < 9; bit++) {
        fake_t *f = fake_new_all();
        f->acCaps = (uint16_t)(1u << bit);
        hidpp_device_t *dev = t_open(f);
        struct hidpp_action_info *info = hidpp_action_info(dev);
        T_NOT_NULL(info);
        int flags[9] = { info->canKey,
                         info->canMouseButton,
                         info->canXDisplacement,
                         info->canYDisplacement,
                         info->canVerticalRoller,
                         info->canHorizontalRoller,
                         info->canConsumer,
                         info->canInternal,
                         info->canPower };
        for (int n = 0; n < 9; n++)
            T_EQ(flags[n], n == bit);

        /* action id N + 1 is accepted exactly when bit N is set */
        for (int type = 1; type <= 9; type++) {
            int rc = hidpp_action_set(dev, 0x0050, 0, (uint8_t)type, 1, 0);
            T_EQ(rc, type == bit + 1 ? HIDPP_OK : HIDPP_ENOSYS);
        }
        fake_free(f);
    }
    return 0;
}

static int test_table(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_all(&f);
    uint16_t ids[8];

    T_EQ(hidpp_action_list(dev, ids, 8), 3);
    T_EQ(ids[0], 0x0050);
    T_EQ(ids[1], 0x0051);
    T_EQ(ids[2], 0x00C3);
    T_EQ(hidpp_action_list(dev, ids, 2), 2);
    T_EQ(hidpp_action_list(dev, ids, 0), 0);
    T_EQ(hidpp_action_list(dev, NULL, 8), 0);

    T_EQ(hidpp_action_id(dev, 0), 0x0050);
    T_EQ(hidpp_action_id(dev, 2), 0x00C3);
    T_ERR(hidpp_action_id(dev, 3), HIDPP_ENOENT);
    fake_free(f);
    return 0;
}

static int test_get_default(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_all(&f);
    struct hidpp_action a;
    T_OK(hidpp_action_get(dev, 0x0050, 0, &a));
    T_EQ(a.id, 0x0050);
    T_EQ(a.host, 0);
    T_EQ(a.type, HIDPP_ACTION_NONE);
    T_FALSE(a.isRemapped);
    T_NOT_NULL(a.name);
    T_STR(a.typeName, "None");
    fake_free(f);
    return 0;
}

static int test_get_fields_come_from_the_right_bytes(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_all(&f);
    f->actions[2][1] = (struct fake_action) { .type = HIDPP_ACTION_CONSUMER,
                                              .value = 0x1234,
                                              .modifiers = 0,
                                              .status = 0x01 };
    f->actions[2][2] = (struct fake_action) { .type = HIDPP_ACTION_KEY,
                                              .value = 0x00AB,
                                              .modifiers = 0x41,
                                              .status = 0xFE };
    struct hidpp_action a;
    T_OK(hidpp_action_get(dev, 0x0051, 2, &a));
    T_EQ(a.host, 2);
    T_EQ(a.type, HIDPP_ACTION_CONSUMER);
    T_EQ(a.value, 0x1234);
    T_TRUE(a.isRemapped);
    T_STR(a.typeName, "Consumer control");

    T_OK(hidpp_action_get(dev, 0x00C3, 2, &a));
    T_EQ(a.value, 0x00AB);
    T_EQ(a.modifiers, 0x41);
    T_EQ(a.status, 0xFE);
    T_FALSE(a.isRemapped); /* only bit 0 counts */
    fake_free(f);
    return 0;
}

static int test_set(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_all(&f);
    T_NOT_NULL(hidpp_action_info(dev));
    fake_reset_log(f);

    T_OK(hidpp_action_set(
        dev, 0x0051, 0, HIDPP_ACTION_KEY, 0x0004, HIDPP_MOD_LEFT_SHIFT
    ));
    T_EQ(f->actions[0][1].type, HIDPP_ACTION_KEY);
    T_EQ(f->actions[0][1].value, 0x0004);
    T_EQ(f->actions[0][1].modifiers, 0x02);

    const struct fake_packet *p = fake_last(f);
    T_EQ(p->raw[0], HIDPP_KIND_LONG);
    T_EQ(p->raw[3] >> 4, 4);
    T_EQ(p->raw[4], 0x00);
    T_EQ(p->raw[5], 0x51);
    T_EQ(p->raw[6], 0); /* host */
    T_EQ(p->raw[7], HIDPP_ACTION_KEY);
    T_EQ(p->raw[8], 0x00);
    T_EQ(p->raw[9], 0x04);
    T_EQ(p->raw[10], 0x02);

    struct hidpp_action a;
    T_OK(hidpp_action_get(dev, 0x0051, 0, &a));
    T_EQ(a.type, HIDPP_ACTION_KEY);
    T_EQ(a.value, 4);
    T_EQ(a.modifiers, 2);
    T_TRUE(a.isRemapped);
    fake_free(f);
    return 0;
}

static int test_hosts_are_independent(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_all(&f);
    T_OK(
        hidpp_action_set(dev, 0x0050, 0, HIDPP_ACTION_MOUSE_BUTTON, 0x0002, 0)
    );
    T_OK(hidpp_action_set(dev, 0x0050, 2, HIDPP_ACTION_CONSUMER, 0x00E9, 0));

    struct hidpp_action a;
    T_OK(hidpp_action_get(dev, 0x0050, 0, &a));
    T_EQ(a.type, HIDPP_ACTION_MOUSE_BUTTON);
    T_OK(hidpp_action_get(dev, 0x0050, 1, &a));
    T_EQ(a.type, HIDPP_ACTION_NONE);
    T_OK(hidpp_action_get(dev, 0x0050, 2, &a));
    T_EQ(a.type, HIDPP_ACTION_CONSUMER);
    fake_free(f);
    return 0;
}

static int test_current_host(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_all(&f);
    f->chCurrent = 2;
    T_OK(hidpp_action_set(
        dev, 0x0050, HIDPP_HOST_CURRENT, HIDPP_ACTION_KEY, 5, 0
    ));
    T_EQ(f->actions[2][0].value, 5);
    struct hidpp_action a;
    T_OK(hidpp_action_get(dev, 0x0050, HIDPP_HOST_CURRENT, &a));
    T_EQ(a.host, 2);
    T_EQ(a.value, 5);
    fake_free(f);
    return 0;
}

static int test_none_clears_data(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_all(&f);
    T_OK(hidpp_action_set(dev, 0x0050, 0, HIDPP_ACTION_NONE, 0x1234, 0));
    T_EQ(f->actions[0][0].value, 0);
    T_EQ(f->actions[0][0].type, HIDPP_ACTION_NONE);
    T_EQ(f->actions[0][0].status, 1);
    fake_free(f);
    return 0;
}

static int test_set_validation(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_all(&f);
    T_NOT_NULL(hidpp_action_info(dev));
    fake_reset_log(f);

    T_ERR(
        hidpp_action_set(dev, 0x0999, 0, HIDPP_ACTION_KEY, 1, 0), HIDPP_ENOENT
    );
    T_ERR(
        hidpp_action_set(dev, 0x0050, 3, HIDPP_ACTION_KEY, 1, 0), HIDPP_EINVAL
    );
    T_ERR(hidpp_action_set(dev, 0x0050, 0, 10, 1, 0), HIDPP_EINVAL);
    T_ERR(hidpp_action_set(dev, 0x0050, 0, 0xFF, 1, 0), HIDPP_EINVAL);
    T_ERR(
        hidpp_action_set(dev, 0x0050, 0, HIDPP_ACTION_CONSUMER, 1, 0x02),
        HIDPP_EINVAL
    );
    T_ERR(
        hidpp_action_set(dev, 0x0000, 0, HIDPP_ACTION_KEY, 1, 0), HIDPP_ENOENT
    );
    T_EQ(f->requests, 0);
    fake_free(f);
    return 0;
}

static int test_signed_displacement_travels_as_two_bytes(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_all(&f);
    T_OK(hidpp_action_set(
        dev, 0x0050, 0, HIDPP_ACTION_X_DISPLACEMENT, (uint16_t)(int16_t)-5, 0
    ));
    struct hidpp_action a;
    T_OK(hidpp_action_get(dev, 0x0050, 0, &a));
    T_EQ((int16_t)a.value, -5);
    T_EQ(fake_logged(f, f->requests - 2)->raw[8], 0xFF);
    T_EQ(fake_logged(f, f->requests - 2)->raw[9], 0xFB);
    fake_free(f);
    return 0;
}

static int test_get_validation(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_all(&f);
    struct hidpp_action a;
    T_ERR(hidpp_action_get(dev, 0x0999, 0, &a), HIDPP_ENOENT);
    T_ERR(hidpp_action_get(dev, 0x0050, 3, &a), HIDPP_EINVAL);
    T_ERR(hidpp_action_get(dev, 0x0050, 0, NULL), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

static int test_unknown_host_count_skips_host_check(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_all();
    f->acHosts = 0xFF;
    hidpp_device_t *dev = t_open(f);
    T_OK(hidpp_action_set(dev, 0x0050, 2, HIDPP_ACTION_KEY, 1, 0));
    fake_free(f);
    return 0;
}

static int test_reset(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_all(&f);
    T_OK(hidpp_action_set(dev, 0x0050, 1, HIDPP_ACTION_KEY, 9, 0));
    T_OK(hidpp_action_reset(dev, 0x0050, 1));

    const struct fake_packet *p = fake_last(f);
    T_EQ(p->raw[3] >> 4, 5);
    T_EQ(p->raw[5], 0x50);
    T_EQ(p->raw[6], 1);

    struct hidpp_action a;
    T_OK(hidpp_action_get(dev, 0x0050, 1, &a));
    T_FALSE(a.isRemapped);
    T_EQ(a.type, HIDPP_ACTION_NONE);
    T_ERR(hidpp_action_reset(dev, 0x0999, 1), HIDPP_ENOENT);
    T_ERR(hidpp_action_reset(dev, 0x0050, 3), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

static int test_reset_all(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_all(&f);
    for (int h = 0; h < 3; h++)
        T_OK(hidpp_action_set(dev, 0x0050, (uint8_t)h, HIDPP_ACTION_KEY, 4, 0));

    T_OK(hidpp_action_reset_all(dev, 0x05)); /* hosts 0 and 2 */
    T_EQ(f->acResetAll, 0x05);
    T_EQ(f->actions[0][0].type, HIDPP_ACTION_NONE);
    T_EQ(f->actions[1][0].type, HIDPP_ACTION_KEY);
    T_EQ(f->actions[2][0].type, HIDPP_ACTION_NONE);

    fake_reset_log(f);
    T_ERR(hidpp_action_reset_all(dev, 0), HIDPP_EINVAL);
    T_EQ(f->requests, 0);
    fake_free(f);
    return 0;
}

static int test_type_names(int seed, int i) {
    (void)seed, (void)i;
    T_STR(hidpp_action_type_name(HIDPP_ACTION_KEY), "Key");
    T_STR(hidpp_action_type_name(HIDPP_ACTION_POWER), "Power key");
    T_STR(hidpp_action_type_name(10), "Unknown");
    return 0;
}

static int test_unsupported_and_null(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    struct hidpp_action a;
    uint16_t ids[4];
    T_NULL(hidpp_action_info(dev));
    T_ERR(hidpp_action_id(dev, 0), HIDPP_ENOSYS);
    T_EQ(hidpp_action_list(dev, ids, 4), 0);
    T_ERR(hidpp_action_get(dev, 0x50, 0, &a), HIDPP_ENOSYS);
    T_ERR(hidpp_action_set(dev, 0x50, 0, 1, 1, 0), HIDPP_ENOSYS);
    T_ERR(hidpp_action_reset(dev, 0x50, 0), HIDPP_ENOSYS);
    T_ERR(hidpp_action_reset_all(dev, 1), HIDPP_ENOSYS);
    fake_free(f);

    T_NULL(hidpp_action_info(NULL));
    T_ERR(hidpp_action_id(NULL, 0), HIDPP_EINVAL);
    T_ERR(hidpp_action_get(NULL, 0x50, 0, &a), HIDPP_EINVAL);
    T_ERR(hidpp_action_set(NULL, 0x50, 0, 1, 1, 0), HIDPP_EINVAL);
    T_ERR(hidpp_action_reset(NULL, 0x50, 0), HIDPP_EINVAL);
    T_ERR(hidpp_action_reset_all(NULL, 1), HIDPP_EINVAL);
    return 0;
}

static int test_table_is_cached(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_all(&f);
    T_NOT_NULL(hidpp_action_info(dev));
    fake_reset_log(f);
    uint16_t ids[4];
    for (int n = 0; n < 5; n++) {
        T_NOT_NULL(hidpp_action_info(dev));
        T_EQ(hidpp_action_list(dev, ids, 4), 3);
        T_EQ(hidpp_action_id(dev, 1), 0x0051);
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

    struct hidpp_action_info *info = hidpp_action_info(dev);
    T_NOT_NULL(info);
    T_EQ(info->count, 3);
    T_EQ(info->flags, 0x01FF);
    T_TRUE(info->canPower);
    uint16_t ids[4];
    T_EQ(hidpp_action_list(dev, ids, 4), 3);
    T_EQ(ids[2], 0x00C3);
    T_EQ(f->requests, 0);

    /* the stored actions are never cached */
    T_OK(hidpp_action_set(dev, 0x0050, 0, HIDPP_ACTION_KEY, 7, 0));
    struct hidpp_action a;
    T_OK(hidpp_action_get(dev, 0x0050, 0, &a));
    T_EQ(a.value, 7);
    fake_free(f);
    return 0;
}

static int test_cache_holds_the_largest_table(int seed, int i) {
    (void)seed, (void)i;
    static char buf[CACHE_CAPACITY];
    fake_t *f = fake_new_all();
    f->acCount = 255;
    for (int n = 0; n < 255 && n < FAKE_MAX_CONTROLS; n++)
        f->acCids[n] = (uint16_t)(0x1000 + n);
    f->acCount = FAKE_MAX_CONTROLS - 1;
    hidpp_device_t *dev = t_open(f);
    hidpp_cache_collect(dev);

    size_t size = sizeof(buf);
    T_OK(hidpp_cache_save(dev, buf, &size));
    T_TRUE(size < sizeof(buf));
    fake_free(f);
    return 0;
}

static int test_garbage_responses(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_all(&f);
    f->garbage = 1;
    f->garbageState = (uint32_t)seed | 1;
    struct hidpp_action a;
    uint16_t ids[256];

    for (int n = 0; n < 30; n++) {
        (void)hidpp_action_info(dev);
        size_t count = hidpp_action_list(dev, ids, 256);
        for (size_t c = 0; c < count && c < 4; c++) {
            (void)hidpp_action_get(dev, ids[c], (uint8_t)(n % 4), &a);
            (void)hidpp_action_set(
                dev, ids[c], 0, (uint8_t)(n % 11), (uint16_t)n, 0
            );
            (void)hidpp_action_reset(dev, ids[c], 0);
        }
        (void)hidpp_action_id(dev, (uint8_t)n);
        hidpp_cache_clear(dev);
    }
    return 0;
}

static const pf_test_t action_tests[] = {
    { test_info, "info" },
    { test_capability_bits_follow_action_ids, "capability_bits" },
    { test_table, "table" },
    { test_get_default, "get_default" },
    { test_get_fields_come_from_the_right_bytes, "get_right_bytes" },
    { test_set, "set" },
    { test_hosts_are_independent, "hosts_are_independent" },
    { test_current_host, "current_host" },
    { test_none_clears_data, "none_clears_data" },
    { test_set_validation, "set_validation" },
    { test_signed_displacement_travels_as_two_bytes, "signed_displacement" },
    { test_get_validation, "get_validation" },
    { test_unknown_host_count_skips_host_check, "unknown_host_count" },
    { test_reset, "reset" },
    { test_reset_all, "reset_all" },
    { test_type_names, "type_names" },
    { test_unsupported_and_null, "unsupported_and_null" },
    { test_table_is_cached, "table_is_cached" },
    { test_cache_roundtrip, "cache_roundtrip" },
    { test_cache_holds_the_largest_table, "cache_largest_table" },
    { test_garbage_responses, "garbage_responses" },
    { 0 },
};

const pf_suite_t suite_action = { "action", 1, action_tests };
