/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

/*  Randomised tests. A misbehaving device, a corrupted cache or a noisy
    receiver must never crash the library or corrupt memory. Every test is
    repeated with different data; a failure is reproduced with the seed the
    runner printed (`hidpp-tests --seed N robustness`). */

#include "test.h"

/* Touches every public getter of a device and ignores the results. */
static void exercise(hidpp_device_t *dev) {
    uint16_t ids[300];
    uint16_t controls[300];
    struct hidpp_device_info info;
    struct hidpp_event e;

    (void)hidpp_device_info(dev, &info);
    (void)hidpp_device_type(dev);
    (void)hidpp_device_name(dev);
    (void)hidpp_nickname_get(dev);
    (void)hidpp_nickname_default(dev);
    (void)hidpp_serial_number(dev);
    (void)hidpp_battery_info(dev);
    (void)hidpp_battery_status(dev);
    (void)hidpp_feature_list(dev, ids, 300);
    (void)hidpp_feature_info(dev, 0x1000);
    (void)hidpp_feature_info(dev, 0x1B04);
    (void)hidpp_feature_index(dev, 0x0007);
    (void)hidpp_feature_id(dev, 200);
    (void)hidpp_ping(dev, 0x42);
    (void)hidpp_cache_id(dev);

    struct hidpp_firmware_info *fw = hidpp_firmware_info(dev);
    for (int n = 0; fw && n < 256; n++)
        (void)hidpp_firmware_entity(dev, (uint8_t)n);

    hidpp_keymap_t *map = hidpp_keymap(dev);
    if (map) {
        size_t count = hidpp_keymap_list(map, controls, 300);
        for (size_t n = 0; n < count; n++) {
            struct hidpp_keymap_state st;
            (void)hidpp_keymap_info(map, controls[n]);
            (void)hidpp_keymap_state(map, controls[n], &st);
        }
        for (int n = 0; n < 256; n++)
            (void)hidpp_keymap_id(map, (uint8_t)n);
    }

    (void)hidpp_device_poll(dev, &e);
}

/* ------------------------- misbehaving devices ----------------------- */

static int test_garbage_responses(int seed, int i) {
    uint32_t rng = (uint32_t)seed * 31u + (uint32_t)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);

    f->garbage = 1;
    f->garbageState = t_rand(&rng);
    exercise(dev);
    exercise(dev);

    /* clearing the cache makes everything be queried again */
    hidpp_cache_clear(dev);
    f->garbageState = t_rand(&rng);
    exercise(dev);
    fake_free(f);
    return 0;
}

static int test_garbage_from_the_first_request(int seed, int i) {
    uint32_t rng = (uint32_t)seed * 17u + (uint32_t)i;
    fake_t *f = fake_new();
    f->garbage = 1;
    f->garbageState = t_rand(&rng);

    /* opening may legitimately fail, but must not crash */
    hidpp_device_t *dev = fake_open(f);
    if (dev)
        exercise(dev);
    fake_free(f);
    return 0;
}

static int test_garbage_with_every_report_size(int seed, int i) {
    uint32_t rng = (uint32_t)seed * 7u + (uint32_t)i;
    static const int kinds[] = { HIDPP_KIND_SHORT,
                                 HIDPP_KIND_LONG,
                                 HIDPP_KIND_XLONG };

    for (size_t n = 0; n < 3; n++) {
        fake_t *f = fake_new();
        hidpp_device_t *dev = t_open(f);
        f->replyKind = kinds[n];
        f->garbage = 1;
        f->garbageState = t_rand(&rng);
        exercise(dev);
        fake_free(f);
    }
    return 0;
}

static int test_flaky_device(int seed, int i) {
    uint32_t rng = (uint32_t)seed * 13u + (uint32_t)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);

    for (int round = 0; round < 40; round++) {
        f->drop = (int)(t_rand(&rng) % 4);
        f->noise = (int)(t_rand(&rng) % 32);
        f->swidOffset = (int)(t_rand(&rng) % 3 == 0);
        f->errorKind = (t_rand(&rng) & 1) ? HIDPP_KIND_LONG : HIDPP_KIND_SHORT;
        if (t_rand(&rng) % 5 == 0)
            hidpp_cache_clear(dev);
        exercise(dev);
        fake_flush(f);
    }
    fake_free(f);
    return 0;
}

static int test_random_features_and_counts(int seed, int i) {
    uint32_t rng = (uint32_t)seed * 5u + (uint32_t)i;
    fake_t *f = fake_new();
    f->featureSetCountOverride = (int)(t_rand(&rng) % 256);
    f->keymapCount = (uint8_t)(t_rand(&rng) % (FAKE_MAX_CONTROLS + 1));
    f->fwEntityCount = (uint8_t)(t_rand(&rng) % 256);

    hidpp_device_t *dev = fake_open(f);
    if (dev)
        exercise(dev);
    fake_free(f);
    return 0;
}

static int test_random_names(int seed, int i) {
    uint32_t rng = (uint32_t)seed * 3u + (uint32_t)i;
    fake_t *f = fake_new();

    /* names with arbitrary bytes, but without terminators in the middle */
    size_t len = t_rand(&rng) % FAKE_NAME_MAX;
    for (size_t n = 0; n < len; n++)
        f->name[n] = (char)(1 + t_rand(&rng) % 255);
    f->name[len] = '\0';
    len = t_rand(&rng) % FAKE_NAME_MAX;
    for (size_t n = 0; n < len; n++)
        f->nick[n] = (char)(1 + t_rand(&rng) % 255);
    f->nick[len] = '\0';

    hidpp_device_t *dev = t_open(f);
    T_STR(hidpp_device_name(dev), f->name);
    T_STR(hidpp_nickname_get(dev), f->nick);
    fake_free(f);
    return 0;
}

/* ------------------------- noisy transport --------------------------- */

static int test_random_reports_in_the_stream(int seed, int i) {
    uint32_t rng = (uint32_t)seed * 11u + (uint32_t)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);

    for (int n = 0; n < 200; n++) {
        uint8_t raw[HIDPP_LEN_XLONG];
        size_t len = 1 + t_rand(&rng) % HIDPP_LEN_XLONG;
        for (size_t k = 0; k < len; k++)
            raw[k] = (uint8_t)t_rand(&rng);
        fake_queue_raw(f, raw, len);
    }

    /* requests and polling both have to cope */
    (void)hidpp_ping(dev, 1);
    struct hidpp_event e;
    for (int n = 0; n < 50; n++)
        (void)hidpp_device_poll(dev, &e);
    (void)hidpp_poll(f->rcv, &e);

    fake_flush(f);
    T_OK(hidpp_ping(dev, 2)); /* and recover afterwards */
    fake_free(f);
    return 0;
}

static int test_random_notifications_are_decoded(int seed, int i) {
    uint32_t rng = (uint32_t)seed * 19u + (uint32_t)i;
    fake_t *f = fake_new();
    fake_add_feature(f, 0x2121, 0, t_null_handler);
    fake_add_feature(f, 0x2130, 0, t_null_handler);
    fake_add_feature(f, 0x6100, 0, t_null_handler);
    fake_add_feature(f, 0x6110, 0, t_null_handler);
    hidpp_device_t *dev = t_open(f);

    uint16_t ids[32];
    hidpp_feature_list(dev, ids, 32); /* warm the cache */

    for (int n = 0; n < 300; n++) {
        uint8_t data[16];
        for (int k = 0; k < 16; k++)
            data[k] = (uint8_t)t_rand(&rng);
        fake_notify(
            f,
            (uint8_t)(t_rand(&rng) % 14),
            (uint8_t)(t_rand(&rng) % 16),
            data,
            16
        );
    }

    for (int n = 0; n < 300; n++) {
        struct hidpp_event e;
        T_OK(hidpp_device_poll(dev, &e));
        T_TRUE(e.type > HIDPP_EVENT_NONE && e.type < HIDPP__EVENT_MAX);
    }
    fake_free(f);
    return 0;
}

/* ---------------------------- random caches -------------------------- */

static int test_random_cache_buffers(int seed, int i) {
    uint32_t rng = (uint32_t)seed * 23u + (uint32_t)i;
    static uint8_t buf[64 * 1024];

    for (int round = 0; round < 20; round++) {
        size_t size = 1 + t_rand(&rng) % sizeof(buf);
        for (size_t n = 0; n < size; n++)
            buf[n] = (uint8_t)t_rand(&rng);

        fake_t *f = fake_new();
        hidpp_device_t *dev = t_open(f);
        T_TRUE(hidpp_cache_load(dev, buf, size) != HIDPP_OK);
        exercise(dev);
        fake_free(f);
    }
    return 0;
}

static int test_corrupted_valid_caches(int seed, int i) {
    uint32_t rng = (uint32_t)seed * 29u + (uint32_t)i;
    static uint8_t good[128 * 1024], buf[128 * 1024];

    fake_t *src = fake_new();
    hidpp_device_t *sdev = t_open(src);
    hidpp_cache_collect(sdev);
    size_t size = sizeof(good);
    T_OK(hidpp_cache_save(sdev, good, &size));
    fake_free(src);

    for (int round = 0; round < 30; round++) {
        memcpy(buf, good, size);

        /* corrupt the payload only, so the header stays valid */
        int flips = 1 + (int)(t_rand(&rng) % 40);
        for (int k = 0; k < flips; k++)
            buf[32 + t_rand(&rng) % (size - 32)] = (uint8_t)t_rand(&rng);

        fake_t *f = fake_new();
        hidpp_device_t *dev = t_open(f);
        (void)hidpp_cache_load(dev, buf, size);
        exercise(dev); /* garbage answers are fine, crashes are not */
        fake_free(f);
    }
    return 0;
}

/* ------------------------ random API sequences ------------------------ */

static int test_random_api_sequences(int seed, int i) {
    uint32_t rng = (uint32_t)seed * 37u + (uint32_t)i;
    fake_t *f = fake_new();
    f->nickMax = 40;
    hidpp_device_t *dev = t_open(f);
    static uint8_t cache[128 * 1024];

    for (int step = 0; step < 300; step++) {
        switch (t_rand(&rng) % 14) {
        case 0:
            (void)hidpp_device_name(dev);
            break;
        case 1: {
            char name[48];
            size_t len = t_rand(&rng) % 45;
            for (size_t n = 0; n < len; n++)
                name[n] = (char)('a' + t_rand(&rng) % 26);
            name[len] = '\0';
            int rc = hidpp_nickname_set(dev, name);
            T_TRUE(rc == HIDPP_OK || rc == HIDPP_ENOMEM);
            if (rc == HIDPP_OK)
                T_STR(f->nick, name);
            break;
        }
        case 2:
            (void)hidpp_nickname_reset(dev);
            break;
        case 3:
            (void)hidpp_battery_status(dev);
            break;
        case 4: {
            hidpp_keymap_t *map = hidpp_keymap(dev);
            if (map) {
                uint16_t cid = 0x0050 + (uint16_t)(t_rand(&rng) % 4);
                (void)hidpp_keymap_divert(map, cid, (int)(t_rand(&rng) & 1));
            }
            break;
        }
        case 5: {
            hidpp_keymap_t *map = hidpp_keymap(dev);
            if (map) {
                uint16_t cid = 0x0050 + (uint16_t)(t_rand(&rng) % 4);
                (void)hidpp_keymap_remap(
                    map, cid, 0x0050 + (uint16_t)(t_rand(&rng) % 4)
                );
            }
            break;
        }
        case 6:
            hidpp_cache_clear(dev);
            break;
        case 7:
            hidpp_cache_collect(dev);
            break;
        case 8: {
            size_t size = sizeof(cache);
            if (hidpp_cache_save(dev, cache, &size) == HIDPP_OK)
                T_OK(hidpp_cache_load(dev, cache, size));
            break;
        }
        case 9:
            T_OK(hidpp_ping(dev, (uint8_t)t_rand(&rng)));
            break;
        case 10:
            f->drop = (int)(t_rand(&rng) % 2);
            break;
        case 11:
            f->noise = (int)(t_rand(&rng) % 32);
            break;
        case 12: {
            struct hidpp_event e;
            fake_notify(f, 5, 0, NULL, 0);
            (void)hidpp_device_poll(dev, &e);
            break;
        }
        default:
            (void)hidpp_firmware_entity(dev, (uint8_t)(t_rand(&rng) % 5));
            break;
        }
        f->drop = 0;
        fake_flush(f);
    }
    fake_free(f);
    return 0;
}

static const pf_test_t robustness_tests[] = {
    { test_garbage_responses, "garbage_responses" },
    { test_garbage_from_the_first_request, "garbage_from_first_request" },
    { test_garbage_with_every_report_size, "garbage_every_report_size" },
    { test_flaky_device, "flaky_device" },
    { test_random_features_and_counts, "random_features_and_counts" },
    { test_random_names, "random_names" },
    { test_random_reports_in_the_stream, "random_reports_in_stream" },
    { test_random_notifications_are_decoded, "random_notifications" },
    { test_random_cache_buffers, "random_cache_buffers" },
    { test_corrupted_valid_caches, "corrupted_valid_caches" },
    { test_random_api_sequences, "random_api_sequences" },
    { 0 },
};

const pf_suite_t suite_robustness = { "robustness", 25, robustness_tests };
