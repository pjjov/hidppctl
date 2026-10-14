/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

/*  Error codes of the device, retrying failed initialization and the
    construction of fake devices. */

#include "test.h"

/* ---- fake devices with a chosen set of features ------------------------ */

static int test_with_mask_adds_only_those(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_with(FAKE_WITH_BACKLIGHT | FAKE_WITH_ACTION);
    T_EQ(fake_index(f, 0x0001), FAKE_IDX_FEATURE_SET);
    T_EQ(fake_index(f, 0x1982), 2);
    T_EQ(fake_index(f, 0x1C00), 3);
    T_EQ(fake_index(f, 0x0003), -1);
    T_EQ(fake_index(f, 0x1B04), -1);

    hidpp_device_t *dev = t_open(f);
    T_NOT_NULL(hidpp_backlight_info(dev));
    T_NOT_NULL(hidpp_action_info(dev));
    T_NULL(hidpp_host_info(dev));
    T_NULL(hidpp_firmware_info(dev));
    fake_free(f);
    return 0;
}

static int test_with_matches_the_older_constructors(int seed, int i) {
    (void)seed, (void)i;
    fake_t *a = fake_new();
    fake_t *b = fake_new_with(FAKE_WITH_CORE);
    T_EQ(a->featureCount, b->featureCount);
    T_EQ(fake_index(b, 0x1B04), FAKE_IDX_KEYMAP);

    fake_t *all = fake_new_all();
    T_EQ(fake_index(all, 0x1814), FAKE_IDX_CHANGE_HOST);
    T_EQ(fake_index(all, 0x1815), FAKE_IDX_HOSTS_INFO);
    T_EQ(fake_index(all, 0x1982), FAKE_IDX_BACKLIGHT);
    T_EQ(fake_index(all, 0x1990), FAKE_IDX_ILLUMINATION);
    T_EQ(fake_index(all, 0x1C00), FAKE_IDX_ACTION);

    fake_free(a);
    fake_free(b);
    fake_free(all);
    return 0;
}

static int test_with_nothing_is_a_bare_device(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_with(0);
    T_EQ(f->featureCount, 1);
    fake_free(f);
    return 0;
}

/* ---- error codes -------------------------------------------------------- */

static int test_error_code_of_a_device_error(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_all();
    hidpp_device_t *dev = t_open(f);
    T_NOT_NULL(hidpp_action_info(dev));

    T_EQ(hidpp_error_code(f->rcv), HIDPP_ERR_NONE);

    f->errFeature = FAKE_IDX_ACTION;
    f->errFunc = 3;
    f->errCode = HIDPP_ERR_OUT_OF_RANGE;
    struct hidpp_action a;
    T_ERR(hidpp_action_get(dev, 0x0050, 0, &a), HIDPP_EIO);
    T_EQ(hidpp_error_code(f->rcv), HIDPP_ERR_OUT_OF_RANGE);

    f->errCode = HIDPP_ERR_BUSY;
    T_ERR(hidpp_action_get(dev, 0x0050, 0, &a), HIDPP_EIO);
    T_EQ(hidpp_error_code(f->rcv), HIDPP_ERR_BUSY);

    /* the next request starts from a clean slate */
    f->errFunc = -1;
    T_OK(hidpp_action_get(dev, 0x0050, 0, &a));
    T_EQ(hidpp_error_code(f->rcv), HIDPP_ERR_NONE);
    fake_free(f);
    return 0;
}

static int test_error_code_is_zero_for_transport_failures(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_all();
    hidpp_device_t *dev = t_open(f);
    T_NOT_NULL(hidpp_action_info(dev));

    f->errFeature = FAKE_IDX_ACTION;
    f->errFunc = 3;
    f->errCode = HIDPP_ERR_HW_ERROR;
    struct hidpp_action a;
    T_ERR(hidpp_action_get(dev, 0x0050, 0, &a), HIDPP_EIO);
    T_EQ(hidpp_error_code(f->rcv), HIDPP_ERR_HW_ERROR);

    f->errFunc = -1;
    f->silent = 1;
    T_TRUE(hidpp_action_get(dev, 0x0050, 0, &a) < 0);
    T_EQ(hidpp_error_code(f->rcv), HIDPP_ERR_NONE);
    fake_free(f);
    return 0;
}

static int test_error_code_arguments(int seed, int i) {
    (void)seed, (void)i;
    T_EQ(hidpp_error_code(NULL), 0);
    T_STR(hidpp_protocol_error_str(HIDPP_ERR_BUSY), "Device busy");
    T_NOT_NULL(hidpp_protocol_error_str(0xEE));
    return 0;
}

static int test_effective_max_busy_is_eagain(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_all();
    hidpp_device_t *dev = t_open(f);
    T_EQ(hidpp_illumination_effective_max(dev), 800);

    f->illBusy = 1;
    T_ERR(hidpp_illumination_effective_max(dev), HIDPP_EAGAIN);

    /* other device errors keep being plain I/O errors */
    f->illBusy = 0;
    f->errFeature = FAKE_IDX_ILLUMINATION;
    f->errFunc = 12;
    f->errCode = HIDPP_ERR_HW_ERROR;
    T_ERR(hidpp_illumination_effective_max(dev), HIDPP_EIO);
    fake_free(f);
    return 0;
}

/* ---- initialization that fails is retried ------------------------------- */

struct probe {
    const char *name;
    unsigned mask;
    uint16_t id;
    int (*works)(hidpp_device_t *dev);
};

static int p_firmware(hidpp_device_t *d) {
    struct hidpp_firmware_info *i = hidpp_firmware_info(d);
    return i && i->entityCount > 0;
}
static int p_device_name(hidpp_device_t *d) {
    const char *n = hidpp_device_name(d);
    return n && n[0];
}
static int p_nickname(hidpp_device_t *d) {
    return hidpp_nickname_get(d) != NULL;
}
static int p_battery(hidpp_device_t *d) {
    struct hidpp_battery_info *i = hidpp_battery_info(d);
    return i && i->initialized;
}
static int p_keymap(hidpp_device_t *d) {
    hidpp_keymap_t *m = hidpp_keymap(d);
    return m && hidpp_keymap_id(m, 0) > 0;
}
static int p_hosts(hidpp_device_t *d) {
    struct hidpp_host_info *i = hidpp_host_info(d);
    return i && i->initialized && i->count == 3;
}
static int p_backlight(hidpp_device_t *d) {
    struct hidpp_backlight_info *i = hidpp_backlight_info(d);
    return i && i->initialized && i->levelCount == 8;
}
static int p_illumination(hidpp_device_t *d) {
    struct hidpp_illumination_info *i = hidpp_illumination_info(d, 0);
    return i && i->max == 1000;
}
static int p_action(hidpp_device_t *d) {
    struct hidpp_action_info *i = hidpp_action_info(d);
    return i && i->initialized && i->count == 3;
}

static const struct probe probes[] = {
    { "firmware", FAKE_WITH_FIRMWARE, 0x0003, p_firmware },
    { "device_name", FAKE_WITH_DEVICE_NAME, 0x0005, p_device_name },
    { "nickname", FAKE_WITH_NICKNAME, 0x0007, p_nickname },
    { "battery", FAKE_WITH_BATTERY, 0x1000, p_battery },
    { "keymap", FAKE_WITH_KEYMAP, 0x1B04, p_keymap },
    { "hosts", FAKE_WITH_CHANGE_HOST | FAKE_WITH_HOSTS_INFO, 0x1815, p_hosts },
    { "backlight", FAKE_WITH_BACKLIGHT, 0x1982, p_backlight },
    { "illumination", FAKE_WITH_ILLUMINATION, 0x1990, p_illumination },
    { "action", FAKE_WITH_ACTION, 0x1C00, p_action },
};

static int test_every_feature_survives_a_lost_first_contact(int seed, int i) {
    (void)seed, (void)i;

    for (size_t n = 0; n < sizeof(probes) / sizeof(probes[0]); n++) {
        const struct probe *p = &probes[n];
        fake_t *f = fake_new_with(p->mask);
        hidpp_device_t *dev = t_open(f);

        /* nothing at all reaches the device: not even the root answers */
        f->silent = 1;
        T_FALSE(p->works(dev));
        f->silent = 0;
        T_TRUE(p->works(dev));
        fake_free(f);
    }
    return 0;
}

static int test_every_feature_survives_lost_queries(int seed, int i) {
    (void)seed, (void)i;

    for (size_t n = 0; n < sizeof(probes) / sizeof(probes[0]); n++) {
        const struct probe *p = &probes[n];
        fake_t *f = fake_new_with(p->mask);
        hidpp_device_t *dev = t_open(f);

        /* the feature is found, but the queries after that are lost */
        int index = fake_index(f, p->id);
        T_TRUE(index > 0);
        T_TRUE(hidpp_feature_index(dev, p->id) > 0);

        f->silent = 1;
        T_FALSE(p->works(dev));
        f->silent = 0;
        T_TRUE(p->works(dev));
        fake_free(f);
    }
    return 0;
}

static int test_lost_answers_are_not_cached_by_cache_save(int seed, int i) {
    (void)seed, (void)i;
    static char buf[128 * 1024];
    fake_t *src = fake_new_all();
    hidpp_device_t *sdev = t_open(src);

    src->silent = 1;
    hidpp_cache_collect(sdev); /* learns nothing */
    src->silent = 0;
    size_t size = sizeof(buf);
    T_OK(hidpp_cache_save(sdev, buf, &size));
    fake_free(src);

    fake_t *f = fake_new_all();
    hidpp_device_t *dev = t_open(f);
    T_OK(hidpp_cache_load(dev, buf, size));

    /* nothing wrong was stored: everything is queried when first needed */
    for (size_t n = 0; n < sizeof(probes) / sizeof(probes[0]); n++)
        T_TRUE(probes[n].works(dev));
    fake_free(f);
    return 0;
}

static int test_missing_features_are_final(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_with(FAKE_WITH_BATTERY);
    hidpp_device_t *dev = t_open(f);

    T_NULL(hidpp_host_info(dev));
    T_NULL(hidpp_backlight_info(dev));
    T_NULL(hidpp_action_info(dev));
    T_NULL(hidpp_illumination_info(dev, 0));
    T_NULL(hidpp_firmware_info(dev));
    T_NULL(hidpp_nickname_get(dev));
    T_NULL(hidpp_device_name(dev));

    /* asking again costs nothing */
    fake_reset_log(f);
    for (int n = 0; n < 3; n++) {
        T_NULL(hidpp_host_info(dev));
        T_NULL(hidpp_backlight_info(dev));
        T_NULL(hidpp_action_info(dev));
        T_NULL(hidpp_illumination_info(dev, 0));
        T_NULL(hidpp_firmware_info(dev));
    }
    T_EQ(f->requests, 0);
    fake_free(f);
    return 0;
}

static int test_refused_queries_are_final(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_all();
    hidpp_device_t *dev = t_open(f);

    /* the device has no color temperature and says so */
    f->errFeature = FAKE_IDX_ILLUMINATION;
    f->errFunc = 7;
    f->errCode = HIDPP_ERR_INVALID_FUNCTION_ID;
    T_NOT_NULL(hidpp_illumination_info(dev, 0));
    T_NULL(hidpp_illumination_info(dev, 1));

    f->errFunc = -1;
    fake_reset_log(f);
    T_NOT_NULL(hidpp_illumination_info(dev, 0));
    T_NULL(hidpp_illumination_info(dev, 1));
    T_EQ(f->requests, 0);
    fake_free(f);
    return 0;
}

static int test_refused_info_does_not_hide_the_feature(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_all();
    hidpp_device_t *dev = t_open(f);

    f->errFeature = FAKE_IDX_BACKLIGHT;
    f->errFunc = 2;
    f->errCode = HIDPP_ERR_HW_ERROR;
    struct hidpp_backlight_info *info = hidpp_backlight_info(dev);
    T_NOT_NULL(info);
    T_FALSE(info->initialized); /* what the device refused is not known */
    fake_free(f);
    return 0;
}

static int test_keymap_retries_when_the_table_is_cut_short(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_with(FAKE_WITH_KEYMAP);
    for (int n = 0; n < 12; n++)
        fake_add_control(
            f, (uint16_t)(0x0050 + n), (uint16_t)(0x0010 + n), 0, 1
        );
    hidpp_device_t *dev = t_open(f);
    hidpp_keymap_t *map = hidpp_keymap(dev);
    T_NOT_NULL(map);

    uint16_t ids[256];
    size_t count = hidpp_keymap_list(map, ids, 256);
    T_TRUE(count > 8);
    hidpp_cache_clear(dev);

    /* the device stops answering in the middle of the control table */
    f->silentAfter = 8;
    T_TRUE(hidpp_keymap_id(map, 0) < 0);

    f->silentAfter = -1;
    T_TRUE(hidpp_keymap_id(map, 0) > 0);
    T_EQ(hidpp_keymap_list(map, ids, 256), count);
    T_TRUE(hidpp_keymap_info(map, ids[count - 1]) != NULL);
    T_TRUE(hidpp_keymap_info(map, ids[count - 1])->initialized);
    fake_free(f);
    return 0;
}

static int test_hosts_retry_when_the_query_is_cut_short(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_all();
    hidpp_device_t *dev = t_open(f);
    hidpp_cache_clear(dev);

    /* the root and feature lookups get through, the host queries do not */
    f->silentAfter = 4;
    struct hidpp_host_info *info = hidpp_host_info(dev);
    T_TRUE(info == NULL || !info->initialized);

    f->silentAfter = -1;
    info = hidpp_host_info(dev);
    T_NOT_NULL(info);
    T_TRUE(info->initialized);
    T_EQ(info->count, 3);
    fake_free(f);
    return 0;
}

static const pf_test_t retry_tests[] = {
    { test_with_mask_adds_only_those, "with_mask_adds_only_those" },
    { test_with_matches_the_older_constructors,
      "with_matches_older_constructors" },
    { test_with_nothing_is_a_bare_device, "with_nothing" },
    { test_error_code_of_a_device_error, "error_code_of_a_device_error" },
    { test_error_code_is_zero_for_transport_failures, "error_code_transport" },
    { test_error_code_arguments, "error_code_arguments" },
    { test_effective_max_busy_is_eagain, "effective_max_busy_is_eagain" },
    { test_every_feature_survives_a_lost_first_contact,
      "retry_lost_first_contact" },
    { test_every_feature_survives_lost_queries, "retry_lost_queries" },
    { test_lost_answers_are_not_cached_by_cache_save,
      "retry_not_saved_in_cache" },
    { test_missing_features_are_final, "missing_features_are_final" },
    { test_refused_queries_are_final, "refused_queries_are_final" },
    { test_refused_info_does_not_hide_the_feature,
      "refused_info_keeps_feature" },
    { test_keymap_retries_when_the_table_is_cut_short, "keymap_retries" },
    { test_hosts_retry_when_the_query_is_cut_short, "hosts_retry_cut_short" },
    { 0 },
};

const pf_suite_t suite_retry = { "retry", 1, retry_tests };
