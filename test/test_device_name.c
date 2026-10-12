/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

/*  Device name and type (0x0005). */

#include "test.h"

/* fills `out` with a recognisable name of exactly `len` characters */
static void make_name(char *out, size_t len) {
    for (size_t n = 0; n < len; n++)
        out[n] = (char)('a' + n % 26);
    out[len] = '\0';
}

static int fail_name_chunks_hook(
    hidpp_receiver_t *rcv, hidpp_packet_t *req, hidpp_packet_t *res, void *user
) {
    (void)rcv, (void)user;
    if (res && req && req->feat == FAKE_IDX_DEVICE_NAME
        && HIDPP_MSN(req->func) == 1)
        return HIDPP_EIO;
    return 0;
}

static int test_name(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    T_STR(hidpp_device_name(dev), "MX Master 3S");
    fake_free(f);
    return 0;
}

static int test_type(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    T_EQ(hidpp_device_type(dev), HIDPP_TYPE_MOUSE);
    fake_free(f);
    return 0;
}

static int test_every_device_type(int seed, int i) {
    (void)seed, (void)i;

    for (int t = HIDPP_TYPE_KEYBOARD; t <= HIDPP_TYPE_ADAPTER; t++) {
        fake_t *f = fake_new();
        f->deviceType = (uint8_t)t;
        hidpp_device_t *dev = t_open(f);
        T_EQ(hidpp_device_type(dev), t);
        fake_free(f);
    }
    return 0;
}

static int test_name_lengths(int seed, int i) {
    (void)seed, (void)i;
    static const size_t lengths[] = { 0,  1,  2,  3,  4,   14,  15, 16,
                                      17, 31, 32, 33, 100, 254, 255 };

    for (size_t n = 0; n < sizeof(lengths) / sizeof(lengths[0]); n++) {
        fake_t *f = fake_new();
        make_name(f->name, lengths[n]);
        hidpp_device_t *dev = t_open(f);

        const char *name = hidpp_device_name(dev);
        T_NOT_NULL(name);
        T_EQ(strlen(name), lengths[n]);
        T_STR(name, f->name);
        fake_free(f);
    }
    return 0;
}

static int test_name_with_each_report_size(int seed, int i) {
    (void)seed, (void)i;
    static const int kinds[] = { HIDPP_KIND_SHORT,
                                 HIDPP_KIND_LONG,
                                 HIDPP_KIND_XLONG };

    for (size_t n = 0; n < sizeof(kinds) / sizeof(kinds[0]); n++) {
        fake_t *f = fake_new();
        f->replyKind = kinds[n]; /* 3, 16 or 60 characters per answer */
        make_name(f->name, 100);
        hidpp_device_t *dev = t_open(f);
        T_STR(hidpp_device_name(dev), f->name);
        fake_free(f);
    }
    return 0;
}

static int test_name_requested_in_chunks(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    make_name(f->name, 40);
    hidpp_device_t *dev = t_open(f);
    T_NOT_NULL(hidpp_device_name(dev));

    /* 40 characters in 16 character chunks: offsets 0, 16 and 32 */
    uint8_t offsets[8];
    int count = 0;
    for (unsigned n = 0; n < f->requests; n++) {
        const struct fake_packet *p = fake_logged(f, n);
        if (p->raw[2] == FAKE_IDX_DEVICE_NAME && HIDPP_MSN(p->raw[3]) == 1)
            offsets[count++] = p->raw[4];
    }
    T_EQ(count, 3);
    T_EQ(offsets[0], 0);
    T_EQ(offsets[1], 16);
    T_EQ(offsets[2], 32);
    fake_free(f);
    return 0;
}

static int test_name_is_cached(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    T_NOT_NULL(hidpp_device_name(dev));
    fake_reset_log(f);

    for (int n = 0; n < 10; n++) {
        T_NOT_NULL(hidpp_device_name(dev));
        T_EQ(hidpp_device_type(dev), HIDPP_TYPE_MOUSE);
    }
    T_EQ(f->requests, 0);
    fake_free(f);
    return 0;
}

static int test_name_does_not_touch_firmware_feature(int seed, int i) {
    (void)seed, (void)i;
    /* both features share the generic device layout; make sure that
       querying the name leaves the firmware data alone and vice versa */
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);

    T_STR(hidpp_device_name(dev), "MX Master 3S");
    struct hidpp_firmware_info *fw = hidpp_firmware_info(dev);
    T_NOT_NULL(fw);
    T_EQ(fw->entityCount, 3);
    T_STR(hidpp_device_name(dev), "MX Master 3S");
    T_EQ(hidpp_device_type(dev), HIDPP_TYPE_MOUSE);
    fake_free(f);
    return 0;
}

static int test_name_queries_use_device_name_feature(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    fake_reset_log(f);
    T_NOT_NULL(hidpp_device_name(dev));

    T_TRUE(fake_calls(f, FAKE_IDX_DEVICE_NAME) >= 3); /* type, length, name */
    T_EQ(fake_calls(f, FAKE_IDX_FIRMWARE), 0);
    T_EQ(fake_calls(f, FAKE_IDX_NICKNAME), 0);
    fake_free(f);
    return 0;
}

static int test_unsupported_feature(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    hidpp_device_t *dev = t_open(f);

    for (int n = 0; n < 3; n++) {
        T_NULL(hidpp_device_name(dev));
        T_ERR(hidpp_device_type(dev), HIDPP_ENOSYS);
    }
    fake_free(f);
    return 0;
}

static int test_type_first_on_unsupported_device(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    hidpp_device_t *dev = t_open(f);
    /* the very first call must already report "not supported" */
    T_ERR(hidpp_device_type(dev), HIDPP_ENOSYS);
    fake_free(f);
    return 0;
}

static int test_failing_name_chunks(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_set_protocol_hook(f->rcv, fail_name_chunks_hook, NULL);
    hidpp_device_t *dev = t_open(f);

    const char *name = hidpp_device_name(dev);
    /* either nothing or an empty string, never garbage */
    T_TRUE(name == NULL || name[0] == '\0');
    fake_free(f);
    return 0;
}

static int test_null_device(int seed, int i) {
    (void)seed, (void)i;
    T_NULL(hidpp_device_name(NULL));
    T_ERR(hidpp_device_type(NULL), HIDPP_EINVAL);
    return 0;
}

static int test_name_after_cache_clear(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    T_STR(hidpp_device_name(dev), "MX Master 3S");

    strcpy(f->name, "Renamed");
    hidpp_cache_clear(dev);
    T_STR(hidpp_device_name(dev), "Renamed");
    fake_free(f);
    return 0;
}

static const pf_test_t device_name_tests[] = {
    { test_name, "name" },
    { test_type, "type" },
    { test_every_device_type, "every_device_type" },
    { test_name_lengths, "name_lengths" },
    { test_name_with_each_report_size, "name_with_each_report_size" },
    { test_name_requested_in_chunks, "name_requested_in_chunks" },
    { test_name_is_cached, "name_is_cached" },
    { test_name_does_not_touch_firmware_feature, "independent_of_firmware" },
    { test_name_queries_use_device_name_feature, "uses_device_name_feature" },
    { test_unsupported_feature, "unsupported_feature" },
    { test_type_first_on_unsupported_device, "type_first_on_unsupported" },
    { test_failing_name_chunks, "failing_name_chunks" },
    { test_null_device, "null_device" },
    { test_name_after_cache_clear, "name_after_cache_clear" },
    { 0 },
};

const pf_suite_t suite_device_name = { "device_name", 1, device_name_tests };
