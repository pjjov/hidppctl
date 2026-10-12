/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

/*  Packet construction, bit helper macros, error strings and version. */

#include "test.h"

/* ---------------------------- macros -------------------------------- */

static int test_msb_lsb(int seed, int i) {
    (void)seed;
    (void)i;
    T_EQ(HIDPP_MSB(0x1234), 0x12);
    T_EQ(HIDPP_LSB(0x1234), 0x34);
    T_EQ(HIDPP_MSB(0xFFFF), 0xFF);
    T_EQ(HIDPP_LSB(0x0000), 0x00);
    T_EQ(HIDPP_MSB(0x00FF), 0x00);
    T_EQ(HIDPP_LSB(0xFF00), 0x00);
    return 0;
}

static int test_word_roundtrip(int seed, int i) {
    uint32_t rng = (uint32_t)seed + (uint32_t)i;

    for (int n = 0; n < 256; n++) {
        uint16_t w = (uint16_t)t_rand(&rng);
        T_EQ(HIDPP_WORD(HIDPP_MSB(w), HIDPP_LSB(w)), w);
    }
    return 0;
}

static int test_word_is_big_endian(int seed, int i) {
    (void)seed;
    (void)i;
    T_EQ(HIDPP_WORD(0x12, 0x34), 0x1234);
    T_EQ(HIDPP_WORD(0xFF, 0x00), 0xFF00);
    T_EQ(HIDPP_WORD(0x00, 0xFF), 0x00FF);
    return 0;
}

static int test_dword(int seed, int i) {
    (void)seed;
    (void)i;
    T_EQ(HIDPP_DWORD(0x12, 0x34, 0x56, 0x78), 0x12345678);
    T_EQ((uint32_t)HIDPP_DWORD(0xFF, 0xFF, 0xFF, 0xFF), 0xFFFFFFFFu);
    T_EQ((uint32_t)HIDPP_DWORD(0x80, 0x00, 0x00, 0x00), 0x80000000u);
    T_EQ(HIDPP_DWORD(0, 0, 0, 1), 1);
    return 0;
}

static int test_qword(int seed, int i) {
    (void)seed;
    (void)i;
    uint64_t v = HIDPP_QWORD(1, 2, 3, 4, 5, 6, 7, 8);
    T_TRUE(v == 0x0102030405060708ull);
    v = HIDPP_QWORD(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);
    T_TRUE(v == 0xFFFFFFFFFFFFFFFFull);
    v = HIDPP_QWORD(0x80, 0, 0, 0, 0, 0, 0, 0);
    T_TRUE(v == 0x8000000000000000ull);
    return 0;
}

static int test_nibbles(int seed, int i) {
    (void)seed;
    (void)i;
    T_EQ(HIDPP_MSN(0xAB), 0xA);
    T_EQ(HIDPP_LSN(0xAB), 0xB);
    T_EQ(HIDPP_BYTE(0xA, 0xB), 0xAB);
    T_EQ(HIDPP_BYTE(0x1, 0xFF), 0x1F); /* low nibble is masked */
    T_EQ(HIDPP_MSN(HIDPP_BYTE(7, 9)), 7);
    T_EQ(HIDPP_LSN(HIDPP_BYTE(7, 9)), 9);
    return 0;
}

static int test_report_constants(int seed, int i) {
    (void)seed;
    (void)i;
    T_EQ(HIDPP_KIND_SHORT, 0x10);
    T_EQ(HIDPP_KIND_LONG, 0x11);
    T_EQ(HIDPP_KIND_XLONG, 0x12);
    T_EQ(HIDPP_LEN_SHORT, 7);
    T_EQ(HIDPP_LEN_LONG, 20);
    T_EQ(HIDPP_LEN_XLONG, 64);
    T_EQ(sizeof(hidpp_packet_t), HIDPP_LEN_XLONG);
    return 0;
}

static int test_error_codes_are_negative(int seed, int i) {
    (void)seed;
    (void)i;
    T_EQ(HIDPP_OK, 0);
    T_TRUE(HIDPP_ENOENT < 0);
    T_TRUE(HIDPP_EINTR < 0);
    T_TRUE(HIDPP_EIO < 0);
    T_TRUE(HIDPP_EAGAIN < 0);
    T_TRUE(HIDPP_ENOMEM < 0);
    T_TRUE(HIDPP_EEXIST < 0);
    T_TRUE(HIDPP_EINVAL < 0);
    T_TRUE(HIDPP_ENOSYS < 0);
    T_TRUE(HIDPP_ENODATA < 0);
    T_TRUE(HIDPP_ETIMEDOUT < 0);
    T_EQ(HIDPP_HOOK_SKIP_IO, -192);
    return 0;
}

/* ---------------------------- hidpp_make ----------------------------- */

static int test_make_header_fields(int seed, int i) {
    (void)seed;
    (void)i;
    hidpp_packet_t p;
    memset(&p, 0xCC, sizeof(p));

    T_OK(hidpp_make(&p, 0x03, 0x07, 0x2E, NULL, 0));
    T_EQ(p.kind, HIDPP_KIND_SHORT);
    T_EQ(p.device, 0x03);
    T_EQ(p.feat, 0x07);
    T_EQ(p.func, 0x2E);
    return 0;
}

static int test_make_kind_by_length(int seed, int i) {
    (void)seed;
    (void)i;
    uint8_t data[HIDPP_LEN_XLONG - 4];
    memset(data, 0x5A, sizeof(data));
    hidpp_packet_t p;

    /* parameter bytes available: short 3, long 16, very long 60 */
    for (size_t n = 0; n <= 3; n++) {
        T_OK(hidpp_make(&p, 1, 0, 0, data, n));
        T_EQ(p.kind, HIDPP_KIND_SHORT);
    }
    for (size_t n = 4; n <= 16; n++) {
        T_OK(hidpp_make(&p, 1, 0, 0, data, n));
        T_EQ(p.kind, HIDPP_KIND_LONG);
    }
    for (size_t n = 17; n <= 60; n++) {
        T_OK(hidpp_make(&p, 1, 0, 0, data, n));
        T_EQ(p.kind, HIDPP_KIND_XLONG);
    }
    return 0;
}

static int test_make_too_long(int seed, int i) {
    (void)seed;
    (void)i;
    uint8_t data[128] = { 0 };
    hidpp_packet_t p;
    p.kind = 0xEE;

    T_ERR(hidpp_make(&p, 1, 0, 0, data, 61), HIDPP_EINVAL);
    T_ERR(hidpp_make(&p, 1, 0, 0, data, 64), HIDPP_EINVAL);
    T_ERR(hidpp_make(&p, 1, 0, 0, data, 128), HIDPP_EINVAL);
    T_EQ(p.kind, 0xEE); /* untouched on failure */
    return 0;
}

static int test_make_copies_data(int seed, int i) {
    (void)seed;
    (void)i;
    uint8_t data[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    hidpp_packet_t p;
    memset(&p, 0, sizeof(p));

    T_OK(hidpp_make(&p, 1, 2, 3, data, sizeof(data)));
    T_MEM(p.params, data, sizeof(data));
    T_EQ(p.params[10], 0);
    return 0;
}

static int test_make_null_data(int seed, int i) {
    (void)seed;
    (void)i;
    hidpp_packet_t p;
    memset(&p, 0x77, sizeof(p));

    /* a length without data must not read from NULL */
    T_OK(hidpp_make(&p, 1, 2, 3, NULL, 3));
    T_EQ(p.kind, HIDPP_KIND_SHORT);
    T_EQ(p.params[0], 0x77);
    return 0;
}

static int test_make_exact_max(int seed, int i) {
    (void)seed;
    (void)i;
    uint8_t data[HIDPP_LEN_XLONG - 4];
    for (size_t n = 0; n < sizeof(data); n++)
        data[n] = (uint8_t)n;

    hidpp_packet_t p;
    memset(&p, 0, sizeof(p));
    T_OK(hidpp_make(&p, 0xFF, 0xAA, 0xBB, data, sizeof(data)));
    T_MEM(p.params, data, sizeof(data));
    T_EQ(p.device, 0xFF);
    return 0;
}

/* --------------------------- error strings --------------------------- */

static int test_error_str_known(int seed, int i) {
    (void)seed;
    (void)i;
    T_STR(hidpp_error_str(HIDPP_OK), "Success");
    T_STR(hidpp_error_str(HIDPP_ENOENT), "No such entry");
    T_STR(hidpp_error_str(HIDPP_EINTR), "Interrupted");
    T_STR(hidpp_error_str(HIDPP_EIO), "I/O error");
    T_STR(hidpp_error_str(HIDPP_EAGAIN), "Resource temporarily unavailable");
    T_STR(hidpp_error_str(HIDPP_ENOMEM), "Out of memory");
    T_STR(hidpp_error_str(HIDPP_EEXIST), "Already exists");
    T_STR(hidpp_error_str(HIDPP_EINVAL), "Invalid argument");
    T_STR(hidpp_error_str(HIDPP_ENOSYS), "Not supported");
    T_STR(hidpp_error_str(HIDPP_ENODATA), "No data available");
    T_STR(hidpp_error_str(HIDPP_ETIMEDOUT), "Operation timed out");
    return 0;
}

static int test_error_str_unknown(int seed, int i) {
    (void)seed;
    (void)i;
    T_STR(hidpp_error_str(12345), "Unknown error");
    T_STR(hidpp_error_str(-12345), "Unknown error");
    T_STR(hidpp_error_str(1), "Unknown error");
    return 0;
}

static int test_error_str_never_null(int seed, int i) {
    uint32_t rng = (uint32_t)seed + (uint32_t)i;

    for (int n = 0; n < 1000; n++)
        T_NOT_NULL(hidpp_error_str((int)t_rand(&rng)));
    return 0;
}

/* ----------------------------- version ------------------------------- */

static int test_version_string(int seed, int i) {
    (void)seed;
    (void)i;
    T_STR(hidpp_version(), HIDPP_VERSION_STRING);

    char expected[32];
    snprintf(
        expected,
        sizeof(expected),
        "%d.%d.%d",
        HIDPP_VERSION_MAJOR,
        HIDPP_VERSION_MINOR,
        HIDPP_VERSION_PATCH
    );
    T_STR(HIDPP_VERSION_STRING, expected);
    return 0;
}

static int test_version_number(int seed, int i) {
    (void)seed;
    (void)i;
    T_EQ(HIDPP_MAKE_VERSION(1, 2, 3), 1002003);
    T_EQ(HIDPP_MAKE_VERSION(0, 0, 0), 0);
    T_TRUE(HIDPP_MAKE_VERSION(1, 0, 0) > HIDPP_MAKE_VERSION(0, 999, 999));
    T_TRUE(HIDPP_VERSION >= HIDPP_MAKE_VERSION(0, 1, 0));
    return 0;
}

static const pf_test_t packet_tests[] = {
    { test_msb_lsb, "msb_lsb" },
    { test_word_roundtrip, "word_roundtrip" },
    { test_word_is_big_endian, "word_is_big_endian" },
    { test_dword, "dword" },
    { test_qword, "qword" },
    { test_nibbles, "nibbles" },
    { test_report_constants, "report_constants" },
    { test_error_codes_are_negative, "error_codes_are_negative" },
    { test_make_header_fields, "make_header_fields" },
    { test_make_kind_by_length, "make_kind_by_length" },
    { test_make_too_long, "make_too_long" },
    { test_make_copies_data, "make_copies_data" },
    { test_make_null_data, "make_null_data" },
    { test_make_exact_max, "make_exact_max" },
    { test_error_str_known, "error_str_known" },
    { test_error_str_unknown, "error_str_unknown" },
    { test_error_str_never_null, "error_str_never_null" },
    { test_version_string, "version_string" },
    { test_version_number, "version_number" },
    { 0 },
};

const pf_suite_t suite_packet = { "packet", 4, packet_tests };
