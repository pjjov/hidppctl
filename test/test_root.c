/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

/*  Root (0x0000) and Feature Set (0x0001): feature discovery, caching,
    feature information and ping. */

#include "test.h"

static int wrong_echo_hook(
    hidpp_receiver_t *rcv, hidpp_packet_t *req, hidpp_packet_t *res, void *user
) {
    (void)rcv, (void)user;
    if (res && req && req->feat == 0 && HIDPP_MSN(req->func) == 1)
        res->params[2] ^= 0x01;
    return 0;
}

/* ------------------------------ index -------------------------------- */

static int test_index_of_known_features(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    T_EQ(hidpp_feature_index(dev, 0x0001), FAKE_IDX_FEATURE_SET);
    T_EQ(hidpp_feature_index(dev, 0x0003), FAKE_IDX_FIRMWARE);
    T_EQ(hidpp_feature_index(dev, 0x0005), FAKE_IDX_DEVICE_NAME);
    T_EQ(hidpp_feature_index(dev, 0x0007), FAKE_IDX_NICKNAME);
    T_EQ(hidpp_feature_index(dev, 0x1000), FAKE_IDX_BATTERY);
    T_EQ(hidpp_feature_index(dev, 0x1B04), FAKE_IDX_KEYMAP);
    fake_free(f);
    return 0;
}

static int test_index_of_root_is_zero(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    T_EQ(hidpp_feature_index(dev, 0x0000), 0);
    fake_free(f);
    return 0;
}

static int test_index_of_absent_feature(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    hidpp_device_t *dev = t_open(f);
    T_ERR(hidpp_feature_index(dev, 0x1000), HIDPP_ENOENT);
    T_ERR(hidpp_feature_index(dev, 0xFFFF), HIDPP_ENOENT);
    fake_free(f);
    return 0;
}

static int test_absent_feature_does_not_poison_cache(int seed, int i) {
    (void)seed, (void)i;
    /* Root answers "index 0" for missing features. That answer must never be
       stored as information about the root feature itself. */
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    T_ERR(hidpp_feature_index(dev, 0x2201), HIDPP_ENOENT);

    struct hidpp_feature_info *root = hidpp_feature_info(dev, 0x0000);
    T_NOT_NULL(root);
    T_EQ(root->id, 0x0000);
    T_EQ(hidpp_feature_id(dev, 0), 0x0000);
    fake_free(f);
    return 0;
}

static int test_index_is_cached(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    T_EQ(hidpp_feature_index(dev, 0x1000), FAKE_IDX_BATTERY);

    fake_reset_log(f);
    for (int n = 0; n < 20; n++)
        T_EQ(hidpp_feature_index(dev, 0x1000), FAKE_IDX_BATTERY);
    T_EQ(f->requests, 0);
    fake_free(f);
    return 0;
}

static int test_index_null_device(int seed, int i) {
    (void)seed, (void)i;
    T_ERR(hidpp_feature_index(NULL, 0x1000), HIDPP_EINVAL);
    return 0;
}

static int test_index_without_feature_set(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    f->features[0].id = 0x7777; /* the device does not implement 0x0001 */
    hidpp_device_t *dev = t_open(f);
    T_TRUE(hidpp_feature_index(dev, 0x1000) < 0);
    T_TRUE(hidpp_feature_id(dev, 3) < 0);
    T_EQ(hidpp_feature_list(dev, (uint16_t[8]) { 0 }, 8), 0);
    fake_free(f);
    return 0;
}

/* ------------------------------- id ---------------------------------- */

static int test_id_of_every_index(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    T_EQ(hidpp_feature_id(dev, 0), 0x0000);
    T_EQ(hidpp_feature_id(dev, FAKE_IDX_FEATURE_SET), 0x0001);
    T_EQ(hidpp_feature_id(dev, FAKE_IDX_FIRMWARE), 0x0003);
    T_EQ(hidpp_feature_id(dev, FAKE_IDX_DEVICE_NAME), 0x0005);
    T_EQ(hidpp_feature_id(dev, FAKE_IDX_NICKNAME), 0x0007);
    T_EQ(hidpp_feature_id(dev, FAKE_IDX_BATTERY), 0x1000);
    T_EQ(hidpp_feature_id(dev, FAKE_IDX_KEYMAP), 0x1B04);
    fake_free(f);
    return 0;
}

static int test_id_out_of_range(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    T_ERR(hidpp_feature_id(dev, FAKE_IDX_KEYMAP + 1), HIDPP_ENOENT);
    T_ERR(hidpp_feature_id(dev, 100), HIDPP_ENOENT);
    T_ERR(hidpp_feature_id(dev, 255), HIDPP_ENOENT);
    fake_free(f);
    return 0;
}

static int test_id_is_cached(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    T_EQ(hidpp_feature_id(dev, FAKE_IDX_BATTERY), 0x1000);
    fake_reset_log(f);
    for (int n = 0; n < 20; n++)
        T_EQ(hidpp_feature_id(dev, FAKE_IDX_BATTERY), 0x1000);
    T_EQ(f->requests, 0);
    fake_free(f);
    return 0;
}

static int test_id_root_needs_no_request(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    T_EQ(hidpp_feature_id(dev, 1), 0x0001); /* initialises the root cache */
    fake_reset_log(f);
    T_EQ(hidpp_feature_id(dev, 0), 0x0000);
    T_EQ(f->requests, 0);
    fake_free(f);
    return 0;
}

static int test_id_null_device(int seed, int i) {
    (void)seed, (void)i;
    T_ERR(hidpp_feature_id(NULL, 1), HIDPP_EINVAL);
    return 0;
}

static int test_index_and_id_are_inverse(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);

    for (int idx = 0; idx <= FAKE_IDX_KEYMAP; idx++) {
        int id = hidpp_feature_id(dev, (uint8_t)idx);
        T_TRUE(id >= 0);
        T_EQ(hidpp_feature_index(dev, (uint16_t)id), idx);
    }
    fake_free(f);
    return 0;
}

/* ------------------------------- list -------------------------------- */

static int test_list_contains_every_feature(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);

    uint16_t ids[32];
    size_t n = hidpp_feature_list(dev, ids, 32);

    /* root plus six features; the last one used to be dropped */
    T_EQ(n, 7);
    T_EQ(ids[0], 0x0000);
    T_EQ(ids[1], 0x0001);
    T_EQ(ids[2], 0x0003);
    T_EQ(ids[3], 0x0005);
    T_EQ(ids[4], 0x0007);
    T_EQ(ids[5], 0x1000);
    T_EQ(ids[6], 0x1B04);
    fake_free(f);
    return 0;
}

static int test_list_of_minimal_device(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    hidpp_device_t *dev = t_open(f);
    uint16_t ids[8];
    T_EQ(hidpp_feature_list(dev, ids, 8), 2);
    T_EQ(ids[0], 0x0000);
    T_EQ(ids[1], 0x0001);
    fake_free(f);
    return 0;
}

static int test_list_is_truncated_to_max(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    uint16_t ids[16];
    memset(ids, 0xEE, sizeof(ids));

    T_EQ(hidpp_feature_list(dev, ids, 3), 3);
    T_EQ(ids[2], 0x0003);
    T_EQ(ids[3], 0xEEEE); /* nothing written past `max` */
    T_EQ(hidpp_feature_list(dev, ids, 1), 1);
    T_EQ(ids[0], 0x0000);
    fake_free(f);
    return 0;
}

static int test_list_does_not_write_past_count(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    uint16_t ids[16];
    memset(ids, 0xEE, sizeof(ids));
    T_EQ(hidpp_feature_list(dev, ids, 16), 7);
    T_EQ(ids[7], 0xEEEE);
    fake_free(f);
    return 0;
}

static int test_list_invalid_arguments(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    uint16_t ids[4];
    T_EQ(hidpp_feature_list(NULL, ids, 4), 0);
    T_EQ(hidpp_feature_list(dev, NULL, 4), 0);
    T_EQ(hidpp_feature_list(dev, ids, 0), 0);
    fake_free(f);
    return 0;
}

static int test_list_with_maximum_features(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();

    for (uint16_t id = 0x2000; f->featureCount < FAKE_MAX_FEATURES; id++)
        T_TRUE(fake_add_feature(f, id, 0, t_null_handler) > 0);

    hidpp_device_t *dev = t_open(f);
    uint16_t ids[64];
    T_EQ(hidpp_feature_list(dev, ids, 64), FAKE_MAX_FEATURES + 1);
    T_EQ(ids[FAKE_MAX_FEATURES], 0x2000 + FAKE_MAX_FEATURES - 2);

    for (unsigned n = 1; n <= f->featureCount; n++)
        T_EQ(hidpp_feature_index(dev, f->features[n - 1].id), n);
    fake_free(f);
    return 0;
}

static int test_list_trusts_reported_count(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    f->featureSetCountOverride = 3; /* device claims fewer features */
    hidpp_device_t *dev = t_open(f);
    uint16_t ids[16];
    T_EQ(hidpp_feature_list(dev, ids, 16), 4);
    T_ERR(hidpp_feature_id(dev, 5), HIDPP_ENOENT);
    fake_free(f);
    return 0;
}

static int test_list_overreported_count(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    f->featureSetCountOverride = 20; /* device claims more than it has */
    hidpp_device_t *dev = t_open(f);
    uint16_t ids[64];
    memset(ids, 0xEE, sizeof(ids));

    size_t n = hidpp_feature_list(dev, ids, 64);
    T_EQ(n, 21);
    /* unknown slots are reported as 0, existing ones are intact */
    T_EQ(ids[6], 0x1B04);
    T_EQ(ids[7], 0x0000);
    T_EQ(ids[20], 0x0000);
    T_EQ(ids[21], 0xEEEE);
    fake_free(f);
    return 0;
}

/* ------------------------------- info -------------------------------- */

static int test_info_fields(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    f->features[FAKE_IDX_BATTERY - 1].type = 0x00;
    f->features[FAKE_IDX_BATTERY - 1].version = 3;
    hidpp_device_t *dev = t_open(f);

    struct hidpp_feature_info *info = hidpp_feature_info(dev, 0x1000);
    T_NOT_NULL(info);
    T_EQ(info->id, 0x1000);
    T_EQ(info->index, FAKE_IDX_BATTERY);
    T_EQ(info->version, 3);
    T_EQ(info->flags, 0);
    T_TRUE(info->initialized);
    T_FALSE(info->isObsolete);
    T_FALSE(info->isHidden);
    T_FALSE(info->isEngineering);
    T_FALSE(info->isManufacturingDeactivatable);
    T_FALSE(info->isComplianceDeactivatable);
    fake_free(f);
    return 0;
}

static int test_info_name(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    struct hidpp_feature_info *info = hidpp_feature_info(dev, 0x1000);
    T_NOT_NULL(info);
    T_STR(info->name, "Battery status");
    fake_free(f);
    return 0;
}

static int test_info_all_flags(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    f->features[FAKE_IDX_BATTERY - 1].type = 0xF8;
    hidpp_device_t *dev = t_open(f);

    struct hidpp_feature_info *info = hidpp_feature_info(dev, 0x1000);
    T_NOT_NULL(info);
    T_EQ(info->flags, 0xF8);
    T_TRUE(info->isObsolete);
    T_TRUE(info->isHidden);
    T_TRUE(info->isEngineering);
    T_TRUE(info->isManufacturingDeactivatable);
    T_TRUE(info->isComplianceDeactivatable);
    fake_free(f);
    return 0;
}

static int test_info_each_flag_alone(int seed, int i) {
    (void)seed, (void)i;
    static const struct {
        uint8_t bit;
        int which;
    } cases[] = {
        { 0x80, 0 }, { 0x40, 1 }, { 0x20, 2 }, { 0x10, 3 }, { 0x08, 4 }
    };

    for (size_t n = 0; n < sizeof(cases) / sizeof(cases[0]); n++) {
        fake_t *f = fake_new();
        f->features[FAKE_IDX_BATTERY - 1].type = cases[n].bit;
        hidpp_device_t *dev = t_open(f);
        struct hidpp_feature_info *info = hidpp_feature_info(dev, 0x1000);
        T_NOT_NULL(info);

        int flags[5] = { info->isObsolete,
                         info->isHidden,
                         info->isEngineering,
                         info->isManufacturingDeactivatable,
                         info->isComplianceDeactivatable };

        for (int k = 0; k < 5; k++)
            T_EQ(flags[k], k == cases[n].which);
        fake_free(f);
    }
    return 0;
}

static int test_info_ignores_unused_flag_bits(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    f->features[FAKE_IDX_BATTERY - 1].type = 0x07; /* reserved bits only */
    hidpp_device_t *dev = t_open(f);
    struct hidpp_feature_info *info = hidpp_feature_info(dev, 0x1000);
    T_NOT_NULL(info);
    T_FALSE(info->isObsolete || info->isHidden || info->isEngineering);
    T_FALSE(info->isManufacturingDeactivatable);
    T_FALSE(info->isComplianceDeactivatable);
    fake_free(f);
    return 0;
}

static int test_info_absent_feature(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    T_NULL(hidpp_feature_info(dev, 0x2201));
    T_NULL(hidpp_feature_info(NULL, 0x1000));
    fake_free(f);
    return 0;
}

static int test_info_root(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    struct hidpp_feature_info *info = hidpp_feature_info(dev, 0x0000);
    T_NOT_NULL(info);
    T_EQ(info->index, 0);
    T_EQ(info->id, 0x0000);
    fake_free(f);
    return 0;
}

static int test_info_pointer_is_stable(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    struct hidpp_feature_info *a = hidpp_feature_info(dev, 0x1B04);
    hidpp_feature_index(dev, 0x0005);
    struct hidpp_feature_info *b = hidpp_feature_info(dev, 0x1B04);
    T_NOT_NULL(a);
    T_TRUE(a == b);
    fake_free(f);
    return 0;
}

static int test_info_is_cached(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    T_NOT_NULL(hidpp_feature_info(dev, 0x1000));
    fake_reset_log(f);
    for (int n = 0; n < 10; n++)
        T_NOT_NULL(hidpp_feature_info(dev, 0x1000));
    T_EQ(f->requests, 0);
    fake_free(f);
    return 0;
}

/* ------------------------------- ping -------------------------------- */

static int test_ping_every_value(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);

    for (int v = 0; v < 256; v++)
        T_OK(hidpp_ping(dev, (uint8_t)v));
    fake_free(f);
    return 0;
}

static int test_ping_sends_data_in_third_parameter(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    T_OK(hidpp_ping(dev, 0x6B));

    const struct fake_packet *p = fake_last(f);
    T_EQ(p->raw[2], 0);            /* root */
    T_EQ(HIDPP_MSN(p->raw[3]), 1); /* ping */
    T_EQ(p->raw[4], 0);
    T_EQ(p->raw[5], 0);
    T_EQ(p->raw[6], 0x6B);
    fake_free(f);
    return 0;
}

static int test_ping_detects_wrong_echo(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    hidpp_set_protocol_hook(f->rcv, wrong_echo_hook, NULL);
    T_ERR(hidpp_ping(dev, 0x10), HIDPP_EIO);
    fake_free(f);
    return 0;
}

static int test_ping_unresponsive_device(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    f->silent = 1;
    T_ERR(hidpp_ping(dev, 1), HIDPP_EIO);
    f->silent = 0;
    T_OK(hidpp_ping(dev, 1));
    fake_free(f);
    return 0;
}

static int test_ping_null_device(int seed, int i) {
    (void)seed, (void)i;
    T_ERR(hidpp_ping(NULL, 1), HIDPP_EINVAL);
    return 0;
}

static const pf_test_t root_tests[] = {
    { test_index_of_known_features, "index_of_known_features" },
    { test_index_of_root_is_zero, "index_of_root_is_zero" },
    { test_index_of_absent_feature, "index_of_absent_feature" },
    { test_absent_feature_does_not_poison_cache,
      "absent_does_not_poison_cache" },
    { test_index_is_cached, "index_is_cached" },
    { test_index_null_device, "index_null_device" },
    { test_index_without_feature_set, "index_without_feature_set" },
    { test_id_of_every_index, "id_of_every_index" },
    { test_id_out_of_range, "id_out_of_range" },
    { test_id_is_cached, "id_is_cached" },
    { test_id_root_needs_no_request, "id_root_needs_no_request" },
    { test_id_null_device, "id_null_device" },
    { test_index_and_id_are_inverse, "index_and_id_are_inverse" },
    { test_list_contains_every_feature, "list_contains_every_feature" },
    { test_list_of_minimal_device, "list_of_minimal_device" },
    { test_list_is_truncated_to_max, "list_truncated_to_max" },
    { test_list_does_not_write_past_count, "list_no_write_past_count" },
    { test_list_invalid_arguments, "list_invalid_arguments" },
    { test_list_with_maximum_features, "list_with_maximum_features" },
    { test_list_trusts_reported_count, "list_trusts_reported_count" },
    { test_list_overreported_count, "list_overreported_count" },
    { test_info_fields, "info_fields" },
    { test_info_name, "info_name" },
    { test_info_all_flags, "info_all_flags" },
    { test_info_each_flag_alone, "info_each_flag_alone" },
    { test_info_ignores_unused_flag_bits, "info_ignores_unused_bits" },
    { test_info_absent_feature, "info_absent_feature" },
    { test_info_root, "info_root" },
    { test_info_pointer_is_stable, "info_pointer_is_stable" },
    { test_info_is_cached, "info_is_cached" },
    { test_ping_every_value, "ping_every_value" },
    { test_ping_sends_data_in_third_parameter, "ping_data_in_third_param" },
    { test_ping_detects_wrong_echo, "ping_detects_wrong_echo" },
    { test_ping_unresponsive_device, "ping_unresponsive_device" },
    { test_ping_null_device, "ping_null_device" },
    { 0 },
};

const pf_suite_t suite_root = { "root", 1, root_tests };
