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
#include <string.h> /* IWYU pragma: keep */

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
extern const pf_suite_t suite_hosts;
extern const pf_suite_t suite_backlight;
extern const pf_suite_t suite_illumination;
extern const pf_suite_t suite_action;
extern const pf_suite_t suite_retry;
extern const pf_suite_t suite_scrolling;
extern const pf_suite_t suite_smartshift;
extern const pf_suite_t suite_hires_wheel;
extern const pf_suite_t suite_thumbwheel;
extern const pf_suite_t suite_crown;
extern const pf_suite_t suite_pointer;
extern const pf_suite_t suite_dpi;
extern const pf_suite_t suite_fn_inversion;
extern const pf_suite_t suite_solar;
extern const pf_suite_t suite_disable_keys;
extern const pf_suite_t suite_platform;
extern const pf_suite_t suite_touchpad;
extern const pf_suite_t suite_touch_mouse;
extern const pf_suite_t suite_brightness;
extern const pf_suite_t suite_report_rate;
extern const pf_suite_t suite_led;
extern const pf_suite_t suite_perkey;
extern const pf_suite_t suite_mode_status;
extern const pf_suite_t suite_sidetone;
extern const pf_suite_t suite_equalizer;
extern const pf_suite_t suite_events;

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

/*  Canned answers: features added with t_add_canned() answer every
    function with the parameters (or HID++ error) stored for their index
    and function, whatever the request was. */
struct t_canned {
    uint8_t answers[FAKE_MAX_FEATURES + 1][16][HIDPP_LEN_LONG - 4];
    int errors[FAKE_MAX_FEATURES + 1][16];
};

static inline struct t_canned *t_canned(void) {
    static struct t_canned canned;
    return &canned;
}

static inline int t_canned_handler(
    fake_t *f, uint8_t func, const hidpp_packet_t *req, hidpp_packet_t *res
) {
    struct t_canned *c = t_canned();
    (void)f;

    if (func >= 16 || req->feat > FAKE_MAX_FEATURES)
        return HIDPP_ERR_INVALID_FUNCTION_ID;

    if (c->errors[req->feat][func])
        return c->errors[req->feat][func];

    memcpy(res->params, c->answers[req->feat][func], HIDPP_LEN_LONG - 4);
    return 0;
}

/* A bare device whose canned answers are all zero. */
static inline fake_t *t_new(void) {
    memset(t_canned(), 0, sizeof(struct t_canned));
    return fake_new_bare();
}

static inline int t_add_canned(fake_t *f, uint16_t id, uint8_t version) {
    return fake_add_feature(f, id, version, t_canned_handler);
}

/* Sets the answer of function `func` of the feature at `index`. */
static inline void t_answer(
    int index, uint8_t func, const uint8_t *data, size_t n
) {
    uint8_t *answer = t_canned()->answers[index][func];
    memset(answer, 0, HIDPP_LEN_LONG - 4);
    memcpy(answer, data, n);
}

/* Makes function `func` of the feature at `index` fail with `code`. */
static inline void t_refuse(int index, uint8_t func, int code) {
    t_canned()->errors[index][func] = code;
}

/* Parameter `n` and function number of the last request. */
#define T_PARAM(f, n) (fake_last(f)->raw[4 + (n)])
#define T_FUNC(f) (fake_last(f)->raw[3] >> 4)

/*  Collects and saves the cache of a device created by `make` (which needs
    the firmware feature for a cache id), then loads it into another device
    from `make`. The log of the returned device is cleared. */
static inline hidpp_device_t *t_from_cache(
    fake_t *(*make)(void), fake_t **out
) {
    static char buffer[128 * 1024];
    fake_t *src = make();
    hidpp_device_t *sdev = t_open(src);
    hidpp_cache_collect(sdev);

    size_t size = sizeof(buffer);
    T_OK(hidpp_cache_save(sdev, buffer, &size));
    fake_free(src);

    fake_t *f = make();
    hidpp_device_t *dev = t_open(f);
    T_OK(hidpp_cache_load(dev, buffer, size));
    fake_reset_log(f);
    *out = f;
    return dev;
}

/* Queues a notification from the feature at `index` and polls it. */
static inline struct hidpp_event t_event(
    fake_t *f,
    hidpp_device_t *dev,
    int index,
    uint8_t func,
    const uint8_t *data,
    size_t n
) {
    uint16_t ids[FAKE_MAX_FEATURES + 1];
    struct hidpp_event e;

    /* resolving the feature id must not need a request later */
    T_TRUE(hidpp_feature_list(dev, ids, FAKE_MAX_FEATURES + 1) > 0);
    memset(&e, 0xCD, sizeof(e));
    fake_notify(f, (uint8_t)index, func, data, n);
    T_OK(hidpp_device_poll(dev, &e));
    return e;
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
