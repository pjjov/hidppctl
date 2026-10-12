/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

/*  Device friendly name / nickname (0x0007). */

#include "test.h"

static void make_name(char *out, size_t len) {
    for (size_t n = 0; n < len; n++)
        out[n] = (char)('A' + n % 26);
    out[len] = '\0';
}

/* counts requests which carry function `func` for the nickname feature */
static unsigned count_func(const fake_t *f, uint8_t func) {
    unsigned count = 0;

    for (unsigned n = 0; n < f->requests; n++) {
        const struct fake_packet *p = fake_logged(f, n);
        if (p->raw[2] == FAKE_IDX_NICKNAME && HIDPP_MSN(p->raw[3]) == func)
            count++;
    }
    return count;
}

/* ------------------------------ reading ------------------------------ */

static int test_get(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    T_STR(hidpp_nickname_get(dev), "Desk mouse");
    fake_free(f);
    return 0;
}

static int test_default(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    T_STR(hidpp_nickname_default(dev), "MX Master 3S");
    fake_free(f);
    return 0;
}

static int test_get_is_not_empty_on_first_call(int seed, int i) {
    (void)seed, (void)i;
    /* the stored name length used to be reset to 0 during initialisation,
       which made every nickname read as an empty string */
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    const char *name = hidpp_nickname_get(dev);
    T_NOT_NULL(name);
    T_TRUE(name[0] != '\0');
    fake_free(f);
    return 0;
}

static int test_get_various_lengths(int seed, int i) {
    (void)seed, (void)i;
    static const size_t lengths[] = { 0, 1, 14, 15, 16, 29, 30, 31, 100, 254 };

    for (size_t n = 0; n < sizeof(lengths) / sizeof(lengths[0]); n++) {
        fake_t *f = fake_new();
        f->nickMax = 255;
        make_name(f->nick, lengths[n]);
        make_name(f->nickDefault, lengths[n] / 2);
        hidpp_device_t *dev = t_open(f);

        T_STR(hidpp_nickname_get(dev), f->nick);
        T_STR(hidpp_nickname_default(dev), f->nickDefault);
        fake_free(f);
    }
    return 0;
}

static int test_get_utf8_bytes_preserved(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    strcpy(f->nick, "Мој миш");
    hidpp_device_t *dev = t_open(f);
    T_STR(hidpp_nickname_get(dev), "Мој миш");
    fake_free(f);
    return 0;
}

static int test_get_is_cached_for_a_while(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    T_NOT_NULL(hidpp_nickname_get(dev));
    fake_reset_log(f);

    for (int n = 0; n < 10; n++)
        T_STR(hidpp_nickname_get(dev), "Desk mouse");
    T_EQ(f->requests, 0);
    T_STR(hidpp_nickname_default(dev), "MX Master 3S");
    T_EQ(f->requests, 0);
    fake_free(f);
    return 0;
}

static int test_get_reads_name_in_chunks(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    f->nickMax = 255;
    make_name(f->nick, 40);
    hidpp_device_t *dev = t_open(f);
    fake_reset_log(f);
    T_STR(hidpp_nickname_get(dev), f->nick);

    uint8_t offsets[8];
    int count = 0;
    for (unsigned n = 0; n < f->requests; n++) {
        const struct fake_packet *p = fake_logged(f, n);
        if (p->raw[2] == FAKE_IDX_NICKNAME && HIDPP_MSN(p->raw[3]) == 1)
            offsets[count++] = p->raw[4];
    }
    T_EQ(count, 3); /* 15 characters per report */
    T_EQ(offsets[0], 0);
    T_EQ(offsets[1], 15);
    T_EQ(offsets[2], 30);
    fake_free(f);
    return 0;
}

static int test_cache_clear_rereads_name(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    T_STR(hidpp_nickname_get(dev), "Desk mouse");

    strcpy(f->nick, "Renamed elsewhere");
    hidpp_cache_clear(dev);
    T_STR(hidpp_nickname_get(dev), "Renamed elsewhere");
    fake_free(f);
    return 0;
}

/* ------------------------------ writing ------------------------------ */

static int test_set(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    T_OK(hidpp_nickname_set(dev, "Office"));
    T_STR(f->nick, "Office");
    T_STR(hidpp_nickname_get(dev), "Office");
    fake_free(f);
    return 0;
}

static int test_set_uses_long_reports(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    fake_reset_log(f);
    T_OK(hidpp_nickname_set(dev, "Office"));

    T_TRUE(count_func(f, 3) >= 1);
    for (unsigned n = 0; n < f->requests; n++) {
        const struct fake_packet *p = fake_logged(f, n);
        if (p->raw[2] == FAKE_IDX_NICKNAME && HIDPP_MSN(p->raw[3]) == 3) {
            /* a short report has room for 3 parameters, not 1 + 15 */
            T_EQ(p->raw[0], HIDPP_KIND_LONG);
            T_EQ(p->len, HIDPP_LEN_LONG);
        }
    }
    fake_free(f);
    return 0;
}

static int test_set_around_chunk_boundaries(int seed, int i) {
    (void)seed, (void)i;

    /* the terminator is written as well, so these straddle the 15 byte
       chunk size: 14 -> exactly one chunk, 15 -> two chunks (NUL alone) */
    for (size_t len = 0; len <= 60; len++) {
        fake_t *f = fake_new();
        f->nickMax = 120;
        hidpp_device_t *dev = t_open(f);

        char name[128];
        make_name(name, len);
        T_OK(hidpp_nickname_set(dev, name));
        T_STR(f->nick, name);
        T_STR(hidpp_nickname_get(dev), name);
        fake_free(f);
    }
    return 0;
}

static int test_set_chunk_count(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    f->nickMax = 120;
    hidpp_device_t *dev = t_open(f);

    char name[64];
    make_name(name, 40); /* 41 bytes with the terminator */
    fake_reset_log(f);
    T_OK(hidpp_nickname_set(dev, name));
    T_EQ(count_func(f, 3), 3);
    fake_free(f);
    return 0;
}

static int test_set_shorter_name_replaces_longer(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    f->nickMax = 120;
    hidpp_device_t *dev = t_open(f);

    T_OK(hidpp_nickname_set(dev, "A rather long nickname indeed"));
    T_OK(hidpp_nickname_set(dev, "Hi"));
    T_STR(f->nick, "Hi"); /* no leftovers from the previous name */
    T_STR(hidpp_nickname_get(dev), "Hi");
    fake_free(f);
    return 0;
}

static int test_set_chunks_do_not_leak_previous_chunk(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    f->nickMax = 120;
    hidpp_device_t *dev = t_open(f);

    /* 20 characters: the second chunk holds 5 characters + NUL and
       must be zero padded, not filled with the tail of the first one */
    T_OK(hidpp_nickname_set(dev, "ABCDEFGHIJKLMNOPQRST"));
    T_STR(f->nick, "ABCDEFGHIJKLMNOPQRST");
    T_EQ(f->nick[21], 0);
    T_EQ(f->nick[25], 0);
    fake_free(f);
    return 0;
}

static int test_set_empty_name(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    T_OK(hidpp_nickname_set(dev, ""));
    T_STR(f->nick, "");
    T_STR(hidpp_nickname_get(dev), "");
    fake_free(f);
    return 0;
}

static int test_set_maximum_length(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new(); /* maximum is 24 including the terminator */
    hidpp_device_t *dev = t_open(f);

    char name[64];
    make_name(name, 23);
    T_OK(hidpp_nickname_set(dev, name));
    T_STR(f->nick, name);

    make_name(name, 24);
    T_ERR(hidpp_nickname_set(dev, name), HIDPP_ENOMEM);
    make_name(name, 50);
    T_ERR(hidpp_nickname_set(dev, name), HIDPP_ENOMEM);
    fake_free(f);
    return 0;
}

static int test_set_too_long_leaves_device_untouched(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    T_NOT_NULL(hidpp_nickname_get(dev));
    fake_reset_log(f);

    char name[64];
    make_name(name, 40);
    T_ERR(hidpp_nickname_set(dev, name), HIDPP_ENOMEM);
    T_EQ(count_func(f, 3), 0);
    T_STR(f->nick, "Desk mouse");
    T_STR(hidpp_nickname_get(dev), "Desk mouse");
    fake_free(f);
    return 0;
}

static int test_set_not_possible_when_max_is_zero(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    f->nickMax = 0;
    hidpp_device_t *dev = t_open(f);
    T_ERR(hidpp_nickname_set(dev, "x"), HIDPP_ENOMEM);
    T_ERR(hidpp_nickname_set(dev, ""), HIDPP_ENOMEM);
    fake_free(f);
    return 0;
}

static int test_set_utf8(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    T_OK(hidpp_nickname_set(dev, "Миш"));
    T_STR(f->nick, "Миш");
    T_STR(hidpp_nickname_get(dev), "Миш");
    fake_free(f);
    return 0;
}

static int test_set_device_error(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    T_NOT_NULL(hidpp_nickname_get(dev));

    f->nickMax = 3; /* the device now rejects chunks beyond its limit */
    f->nick[0] = 'x';
    char name[32];
    make_name(name, 20);
    T_TRUE(hidpp_nickname_set(dev, name) < 0);
    fake_free(f);
    return 0;
}

static int test_set_invalid_arguments(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    T_ERR(hidpp_nickname_set(NULL, "x"), HIDPP_EINVAL);
    T_ERR(hidpp_nickname_set(dev, NULL), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

/* ------------------------------ reset -------------------------------- */

static int test_reset(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    T_OK(hidpp_nickname_reset(dev));
    T_STR(f->nick, "MX Master 3S");
    fake_free(f);
    return 0;
}

static int test_reset_is_visible_through_get(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    T_STR(hidpp_nickname_get(dev), "Desk mouse");
    T_OK(hidpp_nickname_reset(dev));
    T_STR(hidpp_nickname_get(dev), "MX Master 3S");
    fake_free(f);
    return 0;
}

static int test_reset_uses_function_4(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    fake_reset_log(f);
    T_OK(hidpp_nickname_reset(dev));
    T_EQ(count_func(f, 4), 1);
    fake_free(f);
    return 0;
}

static int test_default_survives_set_and_reset(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    T_OK(hidpp_nickname_set(dev, "Changed"));
    T_STR(hidpp_nickname_default(dev), "MX Master 3S");
    T_OK(hidpp_nickname_reset(dev));
    T_STR(hidpp_nickname_default(dev), "MX Master 3S");
    fake_free(f);
    return 0;
}

static int test_set_after_reset(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    T_OK(hidpp_nickname_reset(dev));
    T_OK(hidpp_nickname_set(dev, "Again"));
    T_STR(hidpp_nickname_get(dev), "Again");
    fake_free(f);
    return 0;
}

/* ---------------------------- unsupported ---------------------------- */

static int test_unsupported_feature(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    hidpp_device_t *dev = t_open(f);

    for (int n = 0; n < 3; n++) {
        T_NULL(hidpp_nickname_get(dev));
        T_NULL(hidpp_nickname_default(dev));
        T_ERR(hidpp_nickname_set(dev, "x"), HIDPP_ENOSYS);
        T_ERR(hidpp_nickname_reset(dev), HIDPP_ENOSYS);
    }
    fake_free(f);
    return 0;
}

static int test_set_first_on_unsupported_device(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    hidpp_device_t *dev = t_open(f);
    T_ERR(hidpp_nickname_set(dev, "x"), HIDPP_ENOSYS);
    fake_free(f);
    return 0;
}

static int test_null_device(int seed, int i) {
    (void)seed, (void)i;
    T_NULL(hidpp_nickname_get(NULL));
    T_NULL(hidpp_nickname_default(NULL));
    T_ERR(hidpp_nickname_reset(NULL), HIDPP_EINVAL);
    return 0;
}

static int test_device_without_answers(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    f->silent = 1;

    const char *name = hidpp_nickname_get(dev);
    T_TRUE(name == NULL || name[0] == '\0');
    fake_free(f);
    return 0;
}

static const pf_test_t nickname_tests[] = {
    { test_get, "get" },
    { test_default, "default" },
    { test_get_is_not_empty_on_first_call, "get_not_empty_on_first_call" },
    { test_get_various_lengths, "get_various_lengths" },
    { test_get_utf8_bytes_preserved, "get_utf8_preserved" },
    { test_get_is_cached_for_a_while, "get_is_cached" },
    { test_get_reads_name_in_chunks, "get_reads_in_chunks" },
    { test_cache_clear_rereads_name, "cache_clear_rereads" },
    { test_set, "set" },
    { test_set_uses_long_reports, "set_uses_long_reports" },
    { test_set_around_chunk_boundaries, "set_around_chunk_boundaries" },
    { test_set_chunk_count, "set_chunk_count" },
    { test_set_shorter_name_replaces_longer, "set_shorter_replaces_longer" },
    { test_set_chunks_do_not_leak_previous_chunk, "set_no_stale_chunk_bytes" },
    { test_set_empty_name, "set_empty_name" },
    { test_set_maximum_length, "set_maximum_length" },
    { test_set_too_long_leaves_device_untouched, "set_too_long_untouched" },
    { test_set_not_possible_when_max_is_zero, "set_max_zero" },
    { test_set_utf8, "set_utf8" },
    { test_set_device_error, "set_device_error" },
    { test_set_invalid_arguments, "set_invalid_arguments" },
    { test_reset, "reset" },
    { test_reset_is_visible_through_get, "reset_visible_through_get" },
    { test_reset_uses_function_4, "reset_uses_function_4" },
    { test_default_survives_set_and_reset, "default_survives_set_reset" },
    { test_set_after_reset, "set_after_reset" },
    { test_unsupported_feature, "unsupported_feature" },
    { test_set_first_on_unsupported_device, "set_first_on_unsupported" },
    { test_null_device, "null_device" },
    { test_device_without_answers, "device_without_answers" },
    { 0 },
};

const pf_suite_t suite_nickname = { "nickname", 1, nickname_tests };
