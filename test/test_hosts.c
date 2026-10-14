/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

/*  Change host (0x1814) and hosts information (0x1815). */

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
    struct hidpp_host_info *info = hidpp_host_info(dev);
    T_NOT_NULL(info);
    T_TRUE(info->initialized);
    T_EQ(info->count, 3);
    T_EQ(info->capabilities, 0x01FF);
    T_TRUE(info->canSwitch);
    T_TRUE(info->hasHostInfo);
    T_TRUE(info->canGetName && info->canSetName && info->canMove);
    T_TRUE(info->canDelete && info->hasOsVersion);
    T_TRUE(info->hasEQuadDescriptor && info->hasUSBDescriptor);
    T_TRUE(info->hasBTDescriptor && info->hasBLEDescriptor);
    fake_free(f);
    return 0;
}

static int test_capability_bits(int seed, int i) {
    (void)seed, (void)i;

    for (int bit = 0; bit < 9; bit++) {
        fake_t *f = fake_new_all();
        f->hiCaps = (uint16_t)(1u << bit);
        hidpp_device_t *dev = t_open(f);
        struct hidpp_host_info *info = hidpp_host_info(dev);
        T_NOT_NULL(info);
        int set = info->canGetName + info->canSetName + info->canMove
            + info->canDelete + info->hasOsVersion + info->hasEQuadDescriptor
            + info->hasUSBDescriptor + info->hasBTDescriptor
            + info->hasBLEDescriptor;
        T_EQ(set, 1);
        T_EQ(info->canGetName, bit == 0);
        T_EQ(info->canSetName, bit == 1);
        T_EQ(info->canMove, bit == 2);
        T_EQ(info->canDelete, bit == 3);
        T_EQ(info->hasOsVersion, bit == 4);
        T_EQ(info->hasBLEDescriptor, bit == 8);
        fake_free(f);
    }
    return 0;
}

static int test_capability_mask_byte_order(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_all();
    f->hiCaps = 0x0100; /* only BLE descriptor: second byte, bit 0 */
    hidpp_device_t *dev = t_open(f);
    struct hidpp_host_info *info = hidpp_host_info(dev);
    T_NOT_NULL(info);
    T_EQ(info->capabilities, 0x0100);
    T_FALSE(info->canGetName);
    T_TRUE(info->hasBLEDescriptor);
    fake_free(f);
    return 0;
}

static int test_change_host_only(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    fake_add_change_host(f);
    hidpp_device_t *dev = t_open(f);
    struct hidpp_host_info *info = hidpp_host_info(dev);
    T_NOT_NULL(info);
    T_EQ(info->count, 3);
    T_TRUE(info->canSwitch);
    T_FALSE(info->hasHostInfo);
    T_FALSE(info->canGetName);
    T_EQ(hidpp_host_current(dev), 1);
    char name[16];
    T_ERR(hidpp_host_name(dev, 0, name, sizeof(name)), HIDPP_ENOSYS);
    struct hidpp_host_status st;
    T_ERR(hidpp_host_status(dev, 0, &st), HIDPP_ENOSYS);
    T_EQ(hidpp_host_cookie(dev, 0), 0x11);
    fake_free(f);
    return 0;
}

static int test_hosts_info_only(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    fake_add_hosts_info(f);
    f->chCurrent = 1;
    hidpp_device_t *dev = t_open(f);
    struct hidpp_host_info *info = hidpp_host_info(dev);
    T_NOT_NULL(info);
    T_FALSE(info->canSwitch);
    T_EQ(hidpp_host_current(dev), 1);
    T_ERR(hidpp_host_switch(dev, 0), HIDPP_ENOSYS);
    T_ERR(hidpp_host_cookie(dev, 0), HIDPP_ENOSYS);
    fake_free(f);
    return 0;
}

static int test_unsupported(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    T_NULL(hidpp_host_info(dev));
    T_ERR(hidpp_host_current(dev), HIDPP_ENOSYS);
    T_ERR(hidpp_host_switch(dev, 0), HIDPP_ENOSYS);
    T_ERR(hidpp_host_cookie(dev, 0), HIDPP_ENOSYS);
    fake_free(f);
    return 0;
}

static int test_null_arguments(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_all(&f);
    char name[8];
    struct hidpp_host_status st;
    struct hidpp_host_os os = { 0 };
    struct hidpp_host_descriptor d;

    T_NULL(hidpp_host_info(NULL));
    T_ERR(hidpp_host_current(NULL), HIDPP_EINVAL);
    T_ERR(hidpp_host_switch(NULL, 0), HIDPP_EINVAL);
    T_ERR(hidpp_host_status(dev, 0, NULL), HIDPP_EINVAL);
    T_ERR(hidpp_host_name(dev, 0, NULL, 4), HIDPP_EINVAL);
    T_ERR(hidpp_host_name(dev, 0, name, 0), HIDPP_EINVAL);
    T_ERR(hidpp_host_set_name(dev, 0, NULL), HIDPP_EINVAL);
    T_ERR(hidpp_host_os(dev, 0, NULL), HIDPP_EINVAL);
    T_ERR(hidpp_host_set_os(dev, 0, NULL), HIDPP_EINVAL);
    T_ERR(hidpp_host_descriptor(dev, 0, 0, NULL), HIDPP_EINVAL);
    T_ERR(hidpp_host_status(NULL, 0, &st), HIDPP_EINVAL);
    T_ERR(hidpp_host_os(NULL, 0, &os), HIDPP_EINVAL);
    T_ERR(hidpp_host_descriptor(NULL, 0, 0, &d), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

static int test_current_is_live(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_all(&f);
    T_EQ(hidpp_host_current(dev), 1);
    f->chCurrent = 2;
    T_EQ(hidpp_host_current(dev), 2);
    fake_free(f);
    return 0;
}

static int test_switch_sends_without_waiting(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_all(&f);
    T_NOT_NULL(hidpp_host_info(dev)); /* initialise before counting */
    fake_reset_log(f);

    T_OK(hidpp_host_switch(dev, 2));
    T_EQ(f->chSwitched, 2);
    T_EQ(f->requests, 1);

    const struct fake_packet *p = fake_last(f);
    T_EQ(p->raw[2], FAKE_IDX_CHANGE_HOST);
    T_EQ(p->raw[3] >> 4, 1);
    T_EQ(p->raw[4], 2);
    fake_free(f);
    return 0;
}

static int test_switch_rejects_bad_hosts(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_all(&f);
    T_NOT_NULL(hidpp_host_info(dev));
    fake_reset_log(f);
    T_ERR(hidpp_host_switch(dev, 3), HIDPP_EINVAL);
    T_ERR(hidpp_host_switch(dev, 0xFF), HIDPP_EINVAL);
    T_EQ(f->requests, 0);
    T_EQ(f->chSwitched, -1);
    fake_free(f);
    return 0;
}

static int test_status(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_all(&f);
    struct hidpp_host_status st;

    T_OK(hidpp_host_status(dev, 0, &st));
    T_EQ(st.index, 0);
    T_TRUE(st.isPaired);
    T_EQ(st.busType, HIDPP_HOST_BUS_BT);
    T_STR(st.busName, "Bluetooth");
    T_EQ(st.numPages, 2);
    T_EQ(st.nameLength, 7);
    T_EQ(st.nameMaxLength, 30);

    T_OK(hidpp_host_status(dev, 2, &st));
    T_FALSE(st.isPaired);
    T_EQ(st.busType, HIDPP_HOST_BUS_NONE);
    T_EQ(st.nameLength, 0);

    T_OK(hidpp_host_status(dev, HIDPP_HOST_CURRENT, &st));
    T_EQ(st.index, 1);
    T_EQ(st.busType, HIDPP_HOST_BUS_BLE);
    T_ERR(hidpp_host_status(dev, 3, &st), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

static int test_name(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_all(&f);
    char name[64];

    T_EQ(hidpp_host_name(dev, 0, name, sizeof(name)), 7);
    T_STR(name, "desktop");

    /* longer than one 14 byte chunk */
    T_EQ(hidpp_host_name(dev, 1, name, sizeof(name)), 23);
    T_STR(name, "Laptop with a long name");
    T_EQ(hidpp_host_name(dev, HIDPP_HOST_CURRENT, name, sizeof(name)), 23);

    T_EQ(hidpp_host_name(dev, 2, name, sizeof(name)), 0);
    T_STR(name, "");
    fake_free(f);
    return 0;
}

static int test_name_is_truncated_to_the_buffer(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_all(&f);
    char name[6];
    memset(name, 'x', sizeof(name));

    T_EQ(hidpp_host_name(dev, 1, name, sizeof(name)), 5);
    T_STR(name, "Lapto");

    char one[1] = { 'x' };
    T_EQ(hidpp_host_name(dev, 1, one, 1), 0);
    T_EQ(one[0], 0);
    fake_free(f);
    return 0;
}

static int test_name_needs_capability(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_all();
    f->hiCaps &= (uint16_t)~1u;
    hidpp_device_t *dev = t_open(f);
    char name[16];
    T_ERR(hidpp_host_name(dev, 0, name, sizeof(name)), HIDPP_ENOSYS);
    T_STR(name, "");
    fake_free(f);
    return 0;
}

static int test_set_name(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_all(&f);
    char name[64];

    T_OK(hidpp_host_set_name(dev, 0, "Workstation with a long name"));
    T_STR(f->hosts[0].name, "Workstation with a long name");
    T_EQ(hidpp_host_name(dev, 0, name, sizeof(name)), 28);

    /* a shorter name replaces the longer one */
    T_OK(hidpp_host_set_name(dev, 0, "ab"));
    T_STR(f->hosts[0].name, "ab");

    /* exactly one chunk, and exactly two */
    T_OK(hidpp_host_set_name(dev, 0, "12345678901234"));
    T_STR(f->hosts[0].name, "12345678901234");
    T_OK(hidpp_host_set_name(dev, 0, "1234567890123456789012345678"));
    T_STR(f->hosts[0].name, "1234567890123456789012345678");

    T_OK(hidpp_host_set_name(dev, HIDPP_HOST_CURRENT, "current"));
    T_STR(f->hosts[1].name, "current");
    fake_free(f);
    return 0;
}

static int test_set_empty_name_clears(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_all(&f);
    T_OK(hidpp_host_set_name(dev, 0, ""));
    T_STR(f->hosts[0].name, "");
    fake_free(f);
    return 0;
}

static int test_set_name_wire_format(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_all(&f);
    T_EQ(hidpp_host_set_name(dev, 0, "ABCDEFGHIJKLMNOPQ"), 0);
    fake_reset_log(f);
    T_OK(hidpp_host_set_name(dev, 0, "ABCDEFGHIJKLMNOPQ"));

    /* status request, then the two chunks */
    const struct fake_packet *a = fake_logged(f, 1);
    const struct fake_packet *b = fake_logged(f, 2);
    T_NOT_NULL(a);
    T_NOT_NULL(b);
    T_EQ(a->raw[0], HIDPP_KIND_LONG);
    T_EQ(a->raw[3] >> 4, 4);
    T_EQ(a->raw[4], 0);
    T_EQ(a->raw[5], 0);
    T_MEM(&a->raw[6], "ABCDEFGHIJKLMN", 14);
    T_EQ(b->raw[5], 14);
    T_MEM(&b->raw[6], "OPQ\0\0\0", 6);
    fake_free(f);
    return 0;
}

static int test_set_name_rejects_bad_names(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_all(&f);
    T_NOT_NULL(hidpp_host_info(dev));
    char big[300];
    memset(big, 'a', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';
    fake_reset_log(f);

    T_ERR(
        hidpp_host_set_name(dev, 0, "1234567890123456789012345678901"),
        HIDPP_ENOMEM
    );
    T_ERR(hidpp_host_set_name(dev, 0, big), HIDPP_ENOMEM);
    T_ERR(hidpp_host_set_name(dev, 0, "caf\xC3\xA9"), HIDPP_EINVAL);
    T_ERR(hidpp_host_set_name(dev, 3, "x"), HIDPP_EINVAL);
    T_STR(f->hosts[0].name, "desktop");
    fake_free(f);
    return 0;
}

static int test_move(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_all(&f);
    T_NOT_NULL(hidpp_host_info(dev));
    T_OK(hidpp_host_move(dev, 0, 1));
    T_STR(f->hosts[1].name, "desktop");
    T_STR(f->hosts[0].name, "Laptop with a long name");

    fake_reset_log(f);
    T_ERR(hidpp_host_move(dev, 0, 3), HIDPP_EINVAL);
    T_ERR(hidpp_host_move(dev, 3, 0), HIDPP_EINVAL);
    T_ERR(hidpp_host_move(dev, 0xFF, 0), HIDPP_EINVAL);
    T_EQ(f->requests, 0);
    fake_free(f);
    return 0;
}

static int test_delete(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_all(&f);
    T_OK(hidpp_host_delete(dev, 0));
    struct hidpp_host_status st;
    T_OK(hidpp_host_status(dev, 0, &st));
    T_FALSE(st.isPaired);
    T_ERR(hidpp_host_delete(dev, 3), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

static int test_management_needs_capabilities(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_all();
    f->hiCaps = 0;
    hidpp_device_t *dev = t_open(f);
    struct hidpp_host_os os = { 0 };
    struct hidpp_host_descriptor d;
    T_ERR(hidpp_host_set_name(dev, 0, "x"), HIDPP_ENOSYS);
    T_ERR(hidpp_host_move(dev, 0, 1), HIDPP_ENOSYS);
    T_ERR(hidpp_host_delete(dev, 0), HIDPP_ENOSYS);
    T_ERR(hidpp_host_os(dev, 0, &os), HIDPP_ENOSYS);
    T_ERR(hidpp_host_set_os(dev, 0, &os), HIDPP_ENOSYS);
    T_ERR(hidpp_host_descriptor(dev, 0, 0, &d), HIDPP_ENOSYS);
    fake_free(f);
    return 0;
}

static int test_os(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_all(&f);
    struct hidpp_host_os os;

    T_OK(hidpp_host_os(dev, 0, &os));
    T_EQ(os.type, 1);
    T_EQ(os.version, 10);
    T_EQ(os.revision, 0x000B);
    T_EQ(os.build, 0x4A61);

    struct hidpp_host_os set = {
        .type = 3, .version = 6, .revision = 0x1234, .build = 0xABCD
    };
    T_OK(hidpp_host_set_os(dev, 1, &set));
    T_MEM(f->hosts[1].os, "\x03\x06\x12\x34\xAB\xCD", 6);

    const struct fake_packet *p = fake_last(f);
    T_EQ(p->raw[0], HIDPP_KIND_LONG);
    T_OK(hidpp_host_os(dev, 1, &os));
    T_EQ(os.revision, 0x1234);
    T_EQ(os.build, 0xABCD);
    T_ERR(hidpp_host_os(dev, 5, &os), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

static int test_descriptor(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_all(&f);
    struct hidpp_host_descriptor d;

    T_OK(hidpp_host_descriptor(dev, 0, 1, &d));
    T_EQ(d.index, 0);
    T_EQ(d.busType, HIDPP_HOST_BUS_BT);
    T_EQ(d.data[0], 0xB0);
    T_OK(hidpp_host_descriptor(dev, 1, 0, &d));
    T_EQ(d.busType, HIDPP_HOST_BUS_BLE);
    T_EQ(d.data[0], 0xA1);
    T_ERR(hidpp_host_descriptor(dev, 3, 0, &d), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

static int test_cookies(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_all(&f);

    T_EQ(hidpp_host_cookie(dev, 0), 0x11);
    T_EQ(hidpp_host_cookie(dev, 1), 0x22);
    T_EQ(hidpp_host_cookie(dev, 2), 0);
    T_EQ(hidpp_host_cookie(dev, HIDPP_HOST_CURRENT), 0x22);

    T_OK(hidpp_host_set_cookie(dev, 2, 0xFE));
    T_EQ(f->hosts[2].cookie, 0xFE);
    T_OK(hidpp_host_set_cookie(dev, HIDPP_HOST_CURRENT, 0x07));
    T_EQ(f->hosts[1].cookie, 0x07);
    T_EQ(hidpp_host_cookie(dev, 1), 0x07);

    T_ERR(hidpp_host_cookie(dev, 3), HIDPP_EINVAL);
    T_ERR(hidpp_host_set_cookie(dev, 3, 1), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

static int test_device_failure(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_all(&f);
    T_NOT_NULL(hidpp_host_info(dev));
    f->silent = 1;
    struct hidpp_host_status st;
    char name[8];
    T_TRUE(hidpp_host_current(dev) < 0);
    T_TRUE(hidpp_host_status(dev, 0, &st) < 0);
    T_TRUE(hidpp_host_name(dev, 0, name, sizeof(name)) < 0);
    T_TRUE(hidpp_host_cookie(dev, 0) < 0);
    T_TRUE(hidpp_host_set_name(dev, 0, "x") < 0);
    fake_free(f);
    return 0;
}

static int test_info_is_cached(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_all(&f);
    T_NOT_NULL(hidpp_host_info(dev));
    fake_reset_log(f);
    for (int n = 0; n < 5; n++)
        T_NOT_NULL(hidpp_host_info(dev));
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

    struct hidpp_host_info *info = hidpp_host_info(dev);
    T_NOT_NULL(info);
    T_EQ(info->count, 3);
    T_EQ(info->capabilities, 0x01FF);
    T_TRUE(info->canSwitch && info->hasHostInfo && info->canSetName);
    T_EQ(f->requests, 0);

    /* live data still comes from the device */
    T_EQ(hidpp_host_current(dev), 1);
    T_EQ(f->requests, 1);
    fake_free(f);
    return 0;
}

static int test_cache_clear_refreshes(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_all(&f);
    T_EQ(hidpp_host_info(dev)->count, 3);
    f->hostCount = 2;
    T_EQ(hidpp_host_info(dev)->count, 3);
    hidpp_cache_clear(dev);
    T_EQ(hidpp_host_info(dev)->count, 2);
    fake_free(f);
    return 0;
}

static int test_garbage_responses(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_all(&f);
    f->garbage = 1;
    f->garbageState = (uint32_t)seed | 1;

    char name[300];
    struct hidpp_host_status st;
    struct hidpp_host_os os;
    struct hidpp_host_descriptor d;

    for (int n = 0; n < 20; n++) {
        (void)hidpp_host_info(dev);
        (void)hidpp_host_current(dev);
        (void)hidpp_host_status(dev, (uint8_t)(n % 4), &st);
        (void)hidpp_host_name(dev, (uint8_t)(n % 4), name, sizeof(name));
        (void)hidpp_host_set_name(dev, (uint8_t)(n % 3), "abc");
        (void)hidpp_host_os(dev, (uint8_t)(n % 4), &os);
        (void)hidpp_host_descriptor(dev, (uint8_t)(n % 4), 0, &d);
        (void)hidpp_host_cookie(dev, (uint8_t)(n % 4));
        hidpp_cache_clear(dev);
    }
    return 0;
}

static const pf_test_t hosts_tests[] = {
    { test_info, "info" },
    { test_capability_bits, "capability_bits" },
    { test_capability_mask_byte_order, "capability_mask_byte_order" },
    { test_change_host_only, "change_host_only" },
    { test_hosts_info_only, "hosts_info_only" },
    { test_unsupported, "unsupported" },
    { test_null_arguments, "null_arguments" },
    { test_current_is_live, "current_is_live" },
    { test_switch_sends_without_waiting, "switch_sends_without_waiting" },
    { test_switch_rejects_bad_hosts, "switch_rejects_bad_hosts" },
    { test_status, "status" },
    { test_name, "name" },
    { test_name_is_truncated_to_the_buffer, "name_truncated" },
    { test_name_needs_capability, "name_needs_capability" },
    { test_set_name, "set_name" },
    { test_set_empty_name_clears, "set_empty_name" },
    { test_set_name_wire_format, "set_name_wire_format" },
    { test_set_name_rejects_bad_names, "set_name_rejects_bad_names" },
    { test_move, "move" },
    { test_delete, "delete" },
    { test_management_needs_capabilities, "management_needs_capabilities" },
    { test_os, "os" },
    { test_descriptor, "descriptor" },
    { test_cookies, "cookies" },
    { test_device_failure, "device_failure" },
    { test_info_is_cached, "info_is_cached" },
    { test_cache_roundtrip, "cache_roundtrip" },
    { test_cache_clear_refreshes, "cache_clear_refreshes" },
    { test_garbage_responses, "garbage_responses" },
    { 0 },
};

const pf_suite_t suite_hosts = { "hosts", 1, hosts_tests };
