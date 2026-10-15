/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

/*  Keyboards: Fn inversion (0x40A2, 0x40A3), solar dashboard (0x4301),
    disable keys (0x4521, 0x4522) and platforms (0x4530, 0x4531). */

#include "test.h"

/* --------------------------- fn inversion ---------------------------- */

static int fn_index;

static fake_t *make_fn_multi_host(void) {
    fake_t *f = t_new();
    fake_add_firmware_info(f);
    fn_index = t_add_canned(f, 0x40A3, 0);
    t_answer(fn_index, 0, (const uint8_t[]) { 1, 1, 0, 1 }, 4);
    return f;
}

static int test_fn_single_host(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = t_new();
    int idx = t_add_canned(f, 0x40A2, 0);
    t_answer(idx, 0, (const uint8_t[]) { 0x02 }, 1);
    hidpp_device_t *dev = t_open(f);

    struct hidpp_fn_inversion fn;
    T_OK(hidpp_fn_inversion_get(dev, HIDPP_HOST_CURRENT, &fn));
    T_EQ(fn.host, HIDPP_HOST_CURRENT);
    T_FALSE(fn.enabled || fn.hasFnLock);
    T_TRUE(fn.defaultEnabled);

    T_OK(hidpp_fn_inversion_set(dev, HIDPP_HOST_CURRENT, 1));
    T_EQ(T_FUNC(f), 1);
    T_EQ(T_PARAM(f, 0), 1);

    /* hosts cannot be selected */
    T_ERR(hidpp_fn_inversion_get(dev, 0, &fn), HIDPP_EINVAL);
    T_ERR(hidpp_fn_inversion_set(dev, 1, 0), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

static int test_fn_multi_host(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_fn_multi_host();
    hidpp_device_t *dev = t_open(f);

    struct hidpp_fn_inversion fn;
    T_OK(hidpp_fn_inversion_get(dev, 1, &fn));
    T_EQ(T_PARAM(f, 0), 1);
    T_EQ(fn.host, 1);
    T_TRUE(fn.enabled && fn.hasFnLock);
    T_FALSE(fn.defaultEnabled);

    /* the device has to answer for the host that was asked for */
    T_ERR(hidpp_fn_inversion_get(dev, 2, &fn), HIDPP_EIO);
    T_OK(hidpp_fn_inversion_get(dev, HIDPP_HOST_CURRENT, &fn));

    T_OK(hidpp_fn_inversion_set(dev, 2, 0));
    T_EQ(T_FUNC(f), 1);
    T_EQ(T_PARAM(f, 0), 2); /* host first */
    T_EQ(T_PARAM(f, 1), 0);
    fake_free(f);
    return 0;
}

static int test_fn_prefers_multi_host(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = t_new();
    t_add_canned(f, 0x40A2, 0);
    int idx = t_add_canned(f, 0x40A3, 0);
    hidpp_device_t *dev = t_open(f);
    T_OK(hidpp_fn_inversion_set(dev, 0, 1));
    T_EQ(fake_last(f)->raw[2], idx);
    fake_free(f);
    return 0;
}

static int test_fn_event(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_fn_multi_host();
    hidpp_device_t *dev = t_open(f);
    const uint8_t data[] = { 2, 0, 1, 1 };
    struct hidpp_event e = t_event(f, dev, fn_index, 0, data, 4);
    T_EQ(e.type, HIDPP_EVENT_FN_INVERSION);
    T_EQ(e.as.fnInversion.host, 2);
    T_FALSE(e.as.fnInversion.enabled);
    T_TRUE(e.as.fnInversion.defaultEnabled && e.as.fnInversion.hasFnLock);
    fake_free(f);
    return 0;
}

static int test_fn_unsupported_and_cache(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = t_new();
    hidpp_device_t *dev = t_open(f);
    struct hidpp_fn_inversion fn;
    T_ERR(hidpp_fn_inversion_get(dev, 0xFF, &fn), HIDPP_ENOSYS);
    T_ERR(hidpp_fn_inversion_get(dev, 0xFF, NULL), HIDPP_EINVAL);
    T_ERR(hidpp_fn_inversion_set(NULL, 0xFF, 1), HIDPP_EINVAL);
    fake_free(f);

    dev = t_from_cache(make_fn_multi_host, &f);
    T_OK(hidpp_fn_inversion_get(dev, 1, &fn));
    T_EQ(f->requests, 1); /* only the request itself */
    fake_free(f);
    return 0;
}

static const pf_test_t fn_inversion_tests[] = {
    { test_fn_single_host, "single_host" },
    { test_fn_multi_host, "multi_host" },
    { test_fn_prefers_multi_host, "prefers_multi_host" },
    { test_fn_event, "event" },
    { test_fn_unsupported_and_cache, "unsupported_and_cache" },
    { 0 },
};

const pf_suite_t suite_fn_inversion = { "fn_inversion", 1, fn_inversion_tests };

/* ------------------------------ solar -------------------------------- */

static int test_solar(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = t_new();
    int idx = t_add_canned(f, 0x4301, 0);
    hidpp_device_t *dev = t_open(f);

    T_OK(hidpp_solar_measure(dev, 5, 2));
    T_EQ(T_FUNC(f), 0);
    T_EQ(T_PARAM(f, 0), 5);
    T_EQ(T_PARAM(f, 1), 2);

    T_OK(hidpp_solar_led(dev, HIDPP_SOLAR_LED_GREEN));
    T_EQ(T_FUNC(f), 1);
    T_EQ(T_PARAM(f, 0), 3);
    T_ERR(hidpp_solar_led(dev, 4), HIDPP_EINVAL);

    /* the three events share their layout */
    const uint8_t data[] = { 96, 0x01, 0x3F, 0x06, 0x72, 'G', 'O', 'O', 'D' };

    for (uint8_t func = 0; func < 3; func++) {
        struct hidpp_event e = t_event(f, dev, idx, func, data, sizeof(data));
        T_EQ(e.type, HIDPP_EVENT_SOLAR);
        T_EQ(e.as.solar.kind, func);
        T_EQ(e.as.solar.battery, 96);
        T_EQ(e.as.solar.lux, 319);
    }

    fake_free(f);
    return 0;
}

static int test_solar_unsupported(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = t_new();
    hidpp_device_t *dev = t_open(f);
    T_ERR(hidpp_solar_measure(dev, 1, 1), HIDPP_ENOSYS);
    T_ERR(hidpp_solar_led(dev, 0), HIDPP_ENOSYS);
    T_ERR(hidpp_solar_led(NULL, 0), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

static const pf_test_t solar_tests[] = {
    { test_solar, "solar" },
    { test_solar_unsupported, "unsupported" },
    { 0 },
};

const pf_suite_t suite_solar = { "solar", 1, solar_tests };

/* --------------------------- disable keys ---------------------------- */

static int keys_index, usages_index;

static fake_t *make_disable_keys(void) {
    fake_t *f = t_new();
    fake_add_firmware_info(f);
    keys_index = t_add_canned(f, 0x4521, 0);
    usages_index = t_add_canned(f, 0x4522, 0);
    t_answer(keys_index, 0, (const uint8_t[]) { 0x15 }, 1);
    t_answer(keys_index, 1, (const uint8_t[]) { 0x04 }, 1);
    t_answer(usages_index, 0, (const uint8_t[]) { 32 }, 1);
    return f;
}

static int test_disable_keys_info(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_disable_keys();
    hidpp_device_t *dev = t_open(f);
    struct hidpp_disable_keys_info *info = hidpp_disable_keys_info(dev);
    T_NOT_NULL(info);
    T_TRUE(info->hasKeys && info->hasUsages);
    T_EQ(
        info->keys,
        HIDPP_DISABLE_CAPS_LOCK | HIDPP_DISABLE_SCROLL_LOCK
            | HIDPP_DISABLE_WINDOWS
    );
    T_EQ(info->maxUsages, 32);
    fake_free(f);
    return 0;
}

static int test_disable_keys_mask(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_disable_keys();
    hidpp_device_t *dev = t_open(f);
    T_EQ(hidpp_disable_keys_get(dev), HIDPP_DISABLE_SCROLL_LOCK);

    T_OK(hidpp_disable_keys_set(dev, HIDPP_DISABLE_WINDOWS));
    T_EQ(T_FUNC(f), 2);
    T_EQ(T_PARAM(f, 0), 0x10);
    T_OK(hidpp_disable_keys_set(dev, 0));

    /* Num Lock cannot be disabled on this device */
    T_ERR(hidpp_disable_keys_set(dev, HIDPP_DISABLE_NUM_LOCK), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

static int test_disable_usages(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_disable_keys();
    hidpp_device_t *dev = t_open(f);
    T_NOT_NULL(hidpp_disable_keys_info(dev));

    uint8_t usages[20];
    for (int k = 0; k < 20; k++)
        usages[k] = (uint8_t)(0x04 + k);

    unsigned before = f->requests;
    T_OK(hidpp_disable_usages(dev, usages, 20));
    T_EQ(f->requests - before, 2); /* 16 fit into a request */
    T_EQ(T_FUNC(f), 1);
    T_EQ(T_PARAM(f, 0), 0x04 + 16);
    T_EQ(T_PARAM(f, 3), 0x04 + 19);
    T_EQ(T_PARAM(f, 4), 0); /* end of the list */
    T_EQ(fake_logged(f, before)->raw[4 + 15], 0x04 + 15);

    T_OK(hidpp_enable_usages(dev, usages, 1));
    T_EQ(T_FUNC(f), 2);
    T_EQ(T_PARAM(f, 0), 0x04);
    T_EQ(T_PARAM(f, 1), 0);

    T_OK(hidpp_enable_all_usages(dev));
    T_EQ(T_FUNC(f), 3);

    const uint8_t zero[] = { 0x04, 0x00 };
    T_ERR(hidpp_disable_usages(dev, zero, 2), HIDPP_EINVAL);
    T_ERR(hidpp_disable_usages(dev, NULL, 1), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

static int test_disable_keys_partial(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = t_new();
    t_add_canned(f, 0x4522, 0);
    hidpp_device_t *dev = t_open(f);
    T_FALSE(hidpp_disable_keys_info(dev)->hasKeys);
    T_ERR(hidpp_disable_keys_get(dev), HIDPP_ENOSYS);
    T_ERR(hidpp_disable_keys_set(dev, 1), HIDPP_ENOSYS);
    T_OK(hidpp_enable_all_usages(dev));
    fake_free(f);

    f = t_new();
    t_add_canned(f, 0x4521, 0);
    dev = t_open(f);
    T_FALSE(hidpp_disable_keys_info(dev)->hasUsages);
    T_ERR(hidpp_enable_all_usages(dev), HIDPP_ENOSYS);
    fake_free(f);

    f = t_new();
    dev = t_open(f);
    T_NULL(hidpp_disable_keys_info(dev));
    T_ERR(hidpp_disable_keys_get(dev), HIDPP_ENOSYS);
    fake_free(f);
    return 0;
}

static int test_disable_keys_cache(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = t_from_cache(make_disable_keys, &f);
    struct hidpp_disable_keys_info *info = hidpp_disable_keys_info(dev);
    T_EQ(info->keys, 0x15);
    T_EQ(info->maxUsages, 32);
    T_TRUE(info->hasKeys && info->hasUsages);
    T_EQ(f->requests, 0);
    fake_free(f);
    return 0;
}

static const pf_test_t disable_keys_tests[] = {
    { test_disable_keys_info, "info" },
    { test_disable_keys_mask, "mask" },
    { test_disable_usages, "usages" },
    { test_disable_keys_partial, "partial" },
    { test_disable_keys_cache, "cache" },
    { 0 },
};

const pf_suite_t suite_disable_keys = { "disable_keys", 1, disable_keys_tests };

/* ----------------------------- platform ------------------------------ */

static const uint8_t descriptors[3][8] = {
    { 0, 0, 0x15, 0x00, 0, 0, 0, 0 }, /* Windows, Linux, Android */
    { 1, 1, 0x20, 0x00, 0, 0, 0, 0 }, /* macOS */
    { 1, 2, 0x40, 0x00, 0, 0, 2, 4 }, /* iOS up to 2.4 */
};

static int platform_handler(
    fake_t *f, uint8_t func, const hidpp_packet_t *req, hidpp_packet_t *res
) {
    (void)f;

    switch (func) {
    case 0: /* set platform, OS detection; 2 platforms, 3 descriptors */
        memcpy(res->params, "\x03\x00\x02\x03\x04\x01\x00", 7);
        return 0;
    case 1:
        if (req->params[0] >= 3)
            return HIDPP_ERR_INVALID_ARGUMENT;
        memcpy(res->params, descriptors[req->params[0]], 8);
        return 0;
    case 2:
        res->params[0] = req->params[0] == 0xFF ? 1 : req->params[0];
        memcpy(&res->params[1], "\x01\x01\x02\x00\x01", 5);
        return 0;
    case 3:
        return 0;
    default:
        return HIDPP_ERR_INVALID_FUNCTION_ID;
    }
}

static int platform_index;

static fake_t *make_platform(void) {
    fake_t *f = t_new();
    fake_add_firmware_info(f);
    platform_index = fake_add_feature(f, 0x4531, 1, platform_handler);
    return f;
}

static int test_platform_info(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_platform();
    hidpp_device_t *dev = t_open(f);
    struct hidpp_platform_info *info = hidpp_platform_info(dev);
    T_NOT_NULL(info);
    T_TRUE(info->isMultiPlatform && info->hasOsDetection && info->canSet);
    T_EQ(info->capabilities, 0x0003);
    T_EQ(info->platformCount, 2);
    T_EQ(info->descriptorCount, 3);
    T_EQ(info->hostCount, 4);

    struct hidpp_platform_descriptor d;
    T_OK(hidpp_platform_descriptor(dev, 2, &d));
    T_EQ(d.index, 2);
    T_EQ(d.platform, 1);
    T_EQ(d.osMask, HIDPP_OS_IOS);
    T_EQ(d.toVersion, 2);
    T_EQ(d.toRevision, 4);

    T_OK(hidpp_platform_descriptor(dev, 0, &d));
    T_EQ(d.osMask, HIDPP_OS_WINDOWS | HIDPP_OS_LINUX | HIDPP_OS_ANDROID);
    T_ERR(hidpp_platform_descriptor(dev, 3, &d), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

static int test_platform_hosts(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_platform();
    hidpp_device_t *dev = t_open(f);

    struct hidpp_host_platform hp;
    T_OK(hidpp_platform_get(dev, HIDPP_HOST_CURRENT, &hp));
    T_EQ(T_FUNC(f), 2);
    T_EQ(hp.host, 1);
    T_EQ(hp.status, 1);
    T_EQ(hp.platform, 1);
    T_EQ(hp.source, 2);
    T_EQ(hp.autoDescriptor, 1);

    T_OK(hidpp_platform_get(dev, 3, &hp));
    T_EQ(hp.host, 3);
    T_ERR(hidpp_platform_get(dev, 4, &hp), HIDPP_EINVAL);

    T_OK(hidpp_platform_set(dev, 2, 1));
    T_EQ(T_FUNC(f), 3);
    T_EQ(T_PARAM(f, 0), 2);
    T_EQ(T_PARAM(f, 1), 1);
    T_ERR(hidpp_platform_set(dev, 2, 2), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

static int test_platform_dual(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = t_new();
    int idx = t_add_canned(f, 0x4530, 0);
    t_answer(idx, 0, (const uint8_t[]) { HIDPP_PLATFORM_OTHER }, 1);
    hidpp_device_t *dev = t_open(f);

    struct hidpp_platform_info *info = hidpp_platform_info(dev);
    T_NOT_NULL(info);
    T_FALSE(info->isMultiPlatform);
    T_TRUE(info->canSet);
    T_EQ(info->platformCount, 2);

    struct hidpp_host_platform hp;
    T_OK(hidpp_platform_get(dev, HIDPP_HOST_CURRENT, &hp));
    T_EQ(hp.platform, HIDPP_PLATFORM_OTHER);
    T_ERR(hidpp_platform_get(dev, 0, &hp), HIDPP_EINVAL);

    T_OK(hidpp_platform_set(dev, HIDPP_HOST_CURRENT, HIDPP_PLATFORM_APPLE));
    T_EQ(T_FUNC(f), 1);
    T_EQ(T_PARAM(f, 0), 0);
    T_ERR(hidpp_platform_set(dev, HIDPP_HOST_CURRENT, 2), HIDPP_EINVAL);

    struct hidpp_platform_descriptor d;
    T_ERR(hidpp_platform_descriptor(dev, 0, &d), HIDPP_ENOSYS);

    struct hidpp_event e = t_event(f, dev, idx, 0, (const uint8_t[]) { 1 }, 1);
    T_EQ(e.type, HIDPP_EVENT_PLATFORM);
    T_EQ(e.as.platform.host, HIDPP_HOST_CURRENT);
    T_EQ(e.as.platform.platform, 1);
    fake_free(f);
    return 0;
}

static int test_platform_cannot_set(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = t_new();
    int idx = t_add_canned(f, 0x4531, 0);
    t_answer(idx, 0, (const uint8_t[]) { 0x01, 0x00, 2, 0, 1 }, 5);
    hidpp_device_t *dev = t_open(f);
    T_FALSE(hidpp_platform_info(dev)->canSet);
    T_ERR(hidpp_platform_set(dev, 0, 1), HIDPP_ENOSYS);
    fake_free(f);
    return 0;
}

static int test_platform_event(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_platform();
    hidpp_device_t *dev = t_open(f);
    const uint8_t data[] = { 2, 1, 3 };
    struct hidpp_event e = t_event(f, dev, platform_index, 0, data, 3);
    T_EQ(e.type, HIDPP_EVENT_PLATFORM);
    T_EQ(e.as.platform.host, 2);
    T_EQ(e.as.platform.platform, 1);
    T_EQ(e.as.platform.source, 3);
    fake_free(f);
    return 0;
}

static int test_platform_unsupported_and_cache(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = t_new();
    hidpp_device_t *dev = t_open(f);
    struct hidpp_host_platform hp;
    T_NULL(hidpp_platform_info(dev));
    T_ERR(hidpp_platform_get(dev, 0xFF, &hp), HIDPP_ENOSYS);
    fake_free(f);

    dev = t_from_cache(make_platform, &f);
    struct hidpp_platform_descriptor d;
    T_EQ(hidpp_platform_info(dev)->descriptorCount, 3);
    T_OK(hidpp_platform_descriptor(dev, 1, &d));
    T_EQ(d.osMask, HIDPP_OS_MACOS);
    T_EQ(f->requests, 0);
    fake_free(f);
    return 0;
}

static const pf_test_t platform_tests[] = {
    { test_platform_info, "info" },
    { test_platform_hosts, "hosts" },
    { test_platform_dual, "dual" },
    { test_platform_cannot_set, "cannot_set" },
    { test_platform_event, "event" },
    { test_platform_unsupported_and_cache, "unsupported_and_cache" },
    { 0 },
};

const pf_suite_t suite_platform = { "platform", 1, platform_tests };
