/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

/*  Shared helpers for the test suites.

    Assertions abort the (forked) test with a message that contains both
    the failed expression and the compared values, so a failing test can
    be diagnosed from the report alone. Unlike `pf_assert` they stay active
    when the project is compiled with NDEBUG.
*/
#ifndef HIDPP_TEST_H
#define HIDPP_TEST_H

#include <hidpp.h>
#include <pf_test.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fake.h"

static inline void t_fail(const char *file, int line, const char *fmt, ...) {
    va_list ap;
    fprintf(stderr, "    %s:%d: ", file, line);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    fflush(stderr);
    abort();
}

#define T_TRUE(x)                                               \
    do {                                                        \
        if (!(x))                                               \
            t_fail(__FILE__, __LINE__, "`%s` is not true", #x); \
    } while (0)

#define T_FALSE(x)                                               \
    do {                                                         \
        if ((x))                                                 \
            t_fail(__FILE__, __LINE__, "`%s` is not false", #x); \
    } while (0)

#define T_NULL(x)                                               \
    do {                                                        \
        if ((x) != NULL)                                        \
            t_fail(__FILE__, __LINE__, "`%s` is not NULL", #x); \
    } while (0)

#define T_NOT_NULL(x)                                       \
    do {                                                    \
        if ((x) == NULL)                                    \
            t_fail(__FILE__, __LINE__, "`%s` is NULL", #x); \
    } while (0)

/* Compares integers, printing both sides on failure. */
#define T_EQ(a, b)                                                     \
    do {                                                               \
        long long t_a_ = (long long)(a), t_b_ = (long long)(b);        \
        if (t_a_ != t_b_)                                              \
            t_fail(                                                    \
                __FILE__,                                              \
                __LINE__,                                              \
                "`%s` == `%s` failed: %lld (0x%llx) != %lld (0x%llx)", \
                #a,                                                    \
                #b,                                                    \
                t_a_,                                                  \
                (unsigned long long)t_a_,                              \
                t_b_,                                                  \
                (unsigned long long)t_b_                               \
            );                                                         \
    } while (0)

#define T_NE(a, b)                                              \
    do {                                                        \
        long long t_a_ = (long long)(a), t_b_ = (long long)(b); \
        if (t_a_ == t_b_)                                       \
            t_fail(                                             \
                __FILE__,                                       \
                __LINE__,                                       \
                "`%s` != `%s` failed: both are %lld",           \
                #a,                                             \
                #b,                                             \
                t_a_                                            \
            );                                                  \
    } while (0)

/* Expects a successful library call. */
#define T_OK(x) T_EQ((x), HIDPP_OK)

/* Expects a library call to fail with exactly the error code `code`. */
#define T_ERR(x, code) T_EQ((x), (code))

#define T_STR(a, b)                                      \
    do {                                                 \
        const char *t_a_ = (a), *t_b_ = (b);             \
        if (!t_a_ || !t_b_ || strcmp(t_a_, t_b_) != 0)   \
            t_fail(                                      \
                __FILE__,                                \
                __LINE__,                                \
                "`%s` == `%s` failed: \"%s\" != \"%s\"", \
                #a,                                      \
                #b,                                      \
                t_a_ ? t_a_ : "(null)",                  \
                t_b_ ? t_b_ : "(null)"                   \
            );                                           \
    } while (0)

#define T_MEM(a, b, n)                      \
    do {                                    \
        if (memcmp((a), (b), (n)) != 0)     \
            t_fail(                         \
                __FILE__,                   \
                __LINE__,                   \
                "`%s` != `%s` (%zu bytes)", \
                #a,                         \
                #b,                         \
                (size_t)(n)                 \
            );                              \
    } while (0)

/*  Every suite file exports one `pf_suite_t`; main.c lists them all. */
extern const pf_suite_t suite_packet;
extern const pf_suite_t suite_names;
extern const pf_suite_t suite_receiver;
extern const pf_suite_t suite_hook;
extern const pf_suite_t suite_device;
extern const pf_suite_t suite_root;
extern const pf_suite_t suite_firmware;
extern const pf_suite_t suite_device_name;
extern const pf_suite_t suite_nickname;
extern const pf_suite_t suite_battery;
extern const pf_suite_t suite_keymap;
extern const pf_suite_t suite_poll;
extern const pf_suite_t suite_cache;
extern const pf_suite_t suite_robustness;

/* Opens the fake device and fails the test if the library refuses. */
static inline hidpp_device_t *t_open(fake_t *f) {
    hidpp_device_t *dev = fake_open(f);
    T_NOT_NULL(dev);
    return dev;
}

/* A feature handler which accepts every function and answers with zeros. */
static inline int t_null_handler(
    fake_t *f, uint8_t func, const hidpp_packet_t *req, hidpp_packet_t *res
) {
    (void)f, (void)func, (void)req, (void)res;
    return 0;
}

/*  Small deterministic PRNG (xorshift32) so fuzz-like tests are
    reproducible from the seed that pf_test prints. */
static inline uint32_t t_rand(uint32_t *state) {
    uint32_t x = *state ? *state : 0x9E3779B9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *state = x;
}

#endif /* HIDPP_TEST_H */
