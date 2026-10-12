/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

/*  Cache identification, serialisation and invalidation. */

#include "test.h"

#define CACHE_CAPACITY (128 * 1024)

/* Everything a cache should be able to answer without asking the device. */
static void read_everything(hidpp_device_t *dev) {
    uint16_t ids[32];
    uint16_t controls[32];

    T_TRUE(hidpp_feature_list(dev, ids, 32) > 0);
    T_EQ(hidpp_feature_index(dev, 0x1000), FAKE_IDX_BATTERY);
    T_NOT_NULL(hidpp_feature_info(dev, 0x1B04));
    T_STR(hidpp_device_name(dev), "MX Master 3S");
    T_EQ(hidpp_device_type(dev), HIDPP_TYPE_MOUSE);
    T_STR(hidpp_nickname_default(dev), "MX Master 3S");
    T_STR(hidpp_serial_number(dev), "ABCDEF123456");

    struct hidpp_firmware_info *fw = hidpp_firmware_info(dev);
    T_NOT_NULL(fw);
    T_EQ(fw->entityCount, 3);
    T_EQ(fw->unitId, 0xDEADBEEF);
    T_NOT_NULL(hidpp_firmware_entity(dev, 0));

    struct hidpp_battery_info *bat = hidpp_battery_info(dev);
    T_NOT_NULL(bat);
    T_EQ(bat->nominalLife, 3000);
    T_EQ(bat->criticalLevel, 5);

    hidpp_keymap_t *map = hidpp_keymap(dev);
    T_NOT_NULL(map);
    T_EQ(hidpp_keymap_list(map, controls, 32), 5);
    struct hidpp_keymap_info *info = hidpp_keymap_info(map, 0x00C3);
    T_NOT_NULL(info);
    T_EQ(info->group, 1);
    T_TRUE(info->isDivertable);
    T_STR(info->name, "Mouse_Gesture_Button");
}

/* a device with everything collected and a cache saved from it */
static size_t make_cache(void *buffer, size_t capacity) {
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    hidpp_cache_collect(dev);

    size_t size = capacity;
    T_OK(hidpp_cache_save(dev, buffer, &size));
    fake_free(f);
    return size;
}

/* ------------------------------- id ---------------------------------- */

static int test_id(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    /* prefix, number, revision and build of the main application */
    uint64_t expected = HIDPP_QWORD(0, 'R', 'B', 'M', 0x12, 0x34, 0x05, 0x67);
    T_TRUE(hidpp_cache_id(dev) == expected);
    fake_free(f);
    return 0;
}

static int test_id_is_stable(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    uint64_t id = hidpp_cache_id(dev);
    T_TRUE(id != 0);
    for (int n = 0; n < 5; n++)
        T_TRUE(hidpp_cache_id(dev) == id);
    fake_free(f);
    return 0;
}

static int test_id_changes_with_firmware(int seed, int i) {
    (void)seed, (void)i;
    fake_t *a = fake_new(), *b = fake_new(), *c = fake_new(), *d = fake_new();
    b->fwEntities[0].build++;
    c->fwEntities[0].revision++;
    d->fwEntities[0].prefix[0] = 'X';

    uint64_t ia = hidpp_cache_id(t_open(a));
    uint64_t ib = hidpp_cache_id(t_open(b));
    uint64_t ic = hidpp_cache_id(t_open(c));
    uint64_t id = hidpp_cache_id(t_open(d));
    T_TRUE(ia != ib && ia != ic && ia != id);
    T_TRUE(ib != ic && ib != id && ic != id);
    fake_free(a);
    fake_free(b);
    fake_free(c);
    fake_free(d);
    return 0;
}

static int test_id_ignores_other_entities(int seed, int i) {
    (void)seed, (void)i;
    fake_t *a = fake_new(), *b = fake_new();
    b->fwEntities[1].build = 0x7777;
    T_TRUE(hidpp_cache_id(t_open(a)) == hidpp_cache_id(t_open(b)));
    fake_free(a);
    fake_free(b);
    return 0;
}

static int test_id_without_firmware_feature(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    hidpp_device_t *dev = t_open(f);
    T_TRUE(hidpp_cache_id(dev) == 0);
    fake_free(f);
    return 0;
}

static int test_id_unresponsive_device(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    f->silent = 1;
    T_TRUE(hidpp_cache_id(dev) == 0);
    fake_free(f);
    return 0;
}

static int test_id_null_device(int seed, int i) {
    (void)seed, (void)i;
    T_TRUE(hidpp_cache_id(NULL) == 0);
    return 0;
}

static int test_id_bypasses_cached_indexes(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    hidpp_cache_collect(dev);
    fake_reset_log(f);

    /* the id must reflect the device, not the (possibly stale) cache */
    T_TRUE(hidpp_cache_id(dev) != 0);
    T_TRUE(f->requests >= 2);
    fake_free(f);
    return 0;
}

/* ------------------------------ save --------------------------------- */

static int test_save_reports_size(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    hidpp_cache_collect(dev);

    static uint8_t buf[CACHE_CAPACITY];
    size_t size = sizeof(buf);
    T_OK(hidpp_cache_save(dev, buf, &size));
    T_TRUE(size > 32);
    T_TRUE(size < sizeof(buf));
    fake_free(f);
    return 0;
}

static int test_save_into_exactly_sized_buffer(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    hidpp_cache_collect(dev);

    /* saving into a buffer which is too small reports the required size */
    uint8_t tiny[16];
    size_t need = sizeof(tiny);
    T_ERR(hidpp_cache_save(dev, tiny, &need), HIDPP_ENOMEM);
    T_TRUE(need > sizeof(tiny));

    static uint8_t buf[CACHE_CAPACITY];
    size_t size = need;
    T_OK(hidpp_cache_save(dev, buf, &size));
    T_TRUE(size <= need);
    fake_free(f);
    return 0;
}

static int test_save_too_small(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    uint8_t buf[64];
    memset(buf, 0x5A, sizeof(buf));

    size_t size = 1;
    T_ERR(hidpp_cache_save(dev, buf, &size), HIDPP_ENOMEM);
    for (size_t n = 0; n < sizeof(buf); n++)
        T_EQ(buf[n], 0x5A); /* nothing may be written */
    fake_free(f);
    return 0;
}

static int test_save_invalid_arguments(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    static uint8_t buf[CACHE_CAPACITY];
    size_t size = sizeof(buf), zero = 0;

    T_ERR(hidpp_cache_save(NULL, buf, &size), HIDPP_EINVAL);
    T_ERR(hidpp_cache_save(dev, NULL, &size), HIDPP_EINVAL);
    T_ERR(hidpp_cache_save(dev, buf, NULL), HIDPP_EINVAL);
    T_ERR(hidpp_cache_save(dev, buf, &zero), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

static int test_save_is_deterministic(int seed, int i) {
    (void)seed, (void)i;
    static uint8_t a[CACHE_CAPACITY], b[CACHE_CAPACITY];
    /* different garbage in the buffers: any unwritten byte would differ */
    memset(a, 0xAA, sizeof(a));
    memset(b, 0x55, sizeof(b));

    size_t sa = make_cache(a, sizeof(a));
    size_t sb = make_cache(b, sizeof(b));
    T_EQ(sa, sb);
    T_MEM(a, b, sa);
    return 0;
}

static int test_save_starts_with_magic_and_id(int seed, int i) {
    (void)seed, (void)i;
    static uint8_t buf[CACHE_CAPACITY];
    make_cache(buf, sizeof(buf));

    /* a readable marker followed by the cache id of the device */
    T_TRUE(memcmp(buf, "\0\0\0\0\0\0\0\0", 8) != 0);
    uint64_t id = HIDPP_QWORD(0, 'R', 'B', 'M', 0x12, 0x34, 0x05, 0x67);
    uint8_t id_bytes[8];
    for (int n = 0; n < 8; n++)
        id_bytes[n] = (uint8_t)(id >> (56 - 8 * n));

    int found = 0;
    for (size_t n = 0; n + 8 <= 64; n++)
        found |= memcmp(buf + n, id_bytes, 8) == 0;
    T_TRUE(found);
    return 0;
}

static int test_save_without_collecting(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    fake_reset_log(f);

    /* saving does not query anything beyond the id */
    static uint8_t buf[CACHE_CAPACITY];
    size_t size = sizeof(buf);
    T_OK(hidpp_cache_save(dev, buf, &size));
    T_TRUE(f->requests <= 2);
    fake_free(f);
    return 0;
}

/* ------------------------------ load --------------------------------- */

static int test_roundtrip_answers_without_requests(int seed, int i) {
    (void)seed, (void)i;
    static uint8_t buf[CACHE_CAPACITY];
    size_t size = make_cache(buf, sizeof(buf));

    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    T_OK(hidpp_cache_load(dev, buf, size));
    fake_reset_log(f);

    read_everything(dev);
    /* the nickname is re-read on purpose, everything else is cached */
    unsigned total = 0;
    for (unsigned idx = 0; idx < 256; idx++)
        if (idx != FAKE_IDX_NICKNAME)
            total += fake_calls(f, (uint8_t)idx);
    T_EQ(total, 0);
    fake_free(f);
    return 0;
}

static int test_roundtrip_restores_values(int seed, int i) {
    (void)seed, (void)i;
    static uint8_t buf[CACHE_CAPACITY];
    size_t size = make_cache(buf, sizeof(buf));

    fake_t *f = fake_new();
    f->silent = 0;
    hidpp_device_t *dev = t_open(f);
    T_OK(hidpp_cache_load(dev, buf, size));
    f->silent = 1; /* from now on only the cache can answer */

    read_everything(dev);
    fake_free(f);
    return 0;
}

static int test_roundtrip_of_a_minimal_device(int seed, int i) {
    (void)seed, (void)i;
    static uint8_t buf[CACHE_CAPACITY];

    fake_t *a = fake_new_bare();
    fake_add_firmware_info(a); /* needed for the cache id */
    hidpp_device_t *da = t_open(a);
    hidpp_cache_collect(da);
    size_t size = sizeof(buf);
    T_OK(hidpp_cache_save(da, buf, &size));
    fake_free(a);

    fake_t *b = fake_new_bare();
    fake_add_firmware_info(b);
    hidpp_device_t *db = t_open(b);
    T_OK(hidpp_cache_load(db, buf, size));

    /* unsupported features stay unsupported */
    T_NULL(hidpp_battery_info(db));
    T_NULL(hidpp_keymap(db));
    T_NULL(hidpp_device_name(db));
    T_NULL(hidpp_nickname_get(db));
    fake_free(b);
    return 0;
}

static int test_load_rejects_other_firmware(int seed, int i) {
    (void)seed, (void)i;
    static uint8_t buf[CACHE_CAPACITY];
    size_t size = make_cache(buf, sizeof(buf));

    fake_t *f = fake_new();
    f->fwEntities[0].build = 0x0999; /* a different build of the firmware */
    hidpp_device_t *dev = t_open(f);
    T_TRUE(hidpp_cache_load(dev, buf, size) != HIDPP_OK);

    /* and it must not have altered anything: live answers still work */
    T_STR(hidpp_device_name(dev), "MX Master 3S");
    T_EQ(hidpp_feature_index(dev, 0x1B04), FAKE_IDX_KEYMAP);
    fake_free(f);
    return 0;
}

static int test_load_rejects_bad_magic(int seed, int i) {
    (void)seed, (void)i;
    static uint8_t buf[CACHE_CAPACITY];
    size_t size = make_cache(buf, sizeof(buf));

    for (size_t n = 0; n < 8; n++) {
        fake_t *f = fake_new();
        hidpp_device_t *dev = t_open(f);
        static uint8_t copy[CACHE_CAPACITY];
        memcpy(copy, buf, size);
        copy[n] ^= 0xFF;
        T_TRUE(hidpp_cache_load(dev, copy, size) != HIDPP_OK);
        fake_free(f);
    }
    return 0;
}

static int test_load_rejects_wrong_id_bytes(int seed, int i) {
    (void)seed, (void)i;
    static uint8_t buf[CACHE_CAPACITY];
    size_t size = make_cache(buf, sizeof(buf));

    int rejected = 0;
    for (size_t n = 8; n < 40; n++) {
        fake_t *f = fake_new();
        hidpp_device_t *dev = t_open(f);
        static uint8_t copy[CACHE_CAPACITY];
        memcpy(copy, buf, size);
        copy[n] ^= 0x01;
        rejected += hidpp_cache_load(dev, copy, size) != HIDPP_OK;
        fake_free(f);
    }
    /* at least the eight id bytes (and possibly the size) are verified */
    T_TRUE(rejected >= 8);
    return 0;
}

static int test_load_invalid_arguments(int seed, int i) {
    (void)seed, (void)i;
    static uint8_t buf[CACHE_CAPACITY];
    size_t size = make_cache(buf, sizeof(buf));
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);

    T_ERR(hidpp_cache_load(NULL, buf, size), HIDPP_EINVAL);
    T_ERR(hidpp_cache_load(dev, NULL, size), HIDPP_EINVAL);
    T_ERR(hidpp_cache_load(dev, buf, 0), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

static int test_load_truncated_buffer(int seed, int i) {
    (void)seed, (void)i;
    static uint8_t buf[CACHE_CAPACITY];
    size_t size = make_cache(buf, sizeof(buf));

    /* every prefix of a valid cache must be handled without crashing */
    for (size_t cut = 1; cut < size; cut += 1 + cut / 8) {
        fake_t *f = fake_new();
        hidpp_device_t *dev = t_open(f);
        /* the header records how many bytes a complete cache has */
        T_TRUE(hidpp_cache_load(dev, buf, cut) != HIDPP_OK);

        /* a rejected cache changes nothing: the device stays usable */
        T_STR(hidpp_device_name(dev), "MX Master 3S");
        fake_free(f);
    }
    return 0;
}

static int test_load_accepts_trailing_bytes(int seed, int i) {
    (void)seed, (void)i;
    static uint8_t buf[CACHE_CAPACITY];
    size_t size = make_cache(buf, sizeof(buf));
    memset(buf + size, 0x77, 64); /* e.g. a file padded by a filesystem */

    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    T_OK(hidpp_cache_load(dev, buf, size + 64));
    fake_free(f);
    return 0;
}

static int test_load_garbage_after_header(int seed, int i) {
    (void)seed, (void)i;
    static uint8_t buf[CACHE_CAPACITY];
    size_t size = make_cache(buf, sizeof(buf));
    uint32_t rng = (uint32_t)seed + 77;

    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    for (size_t n = 32; n < size; n++)
        buf[n] = (uint8_t)t_rand(&rng);
    (void)hidpp_cache_load(dev, buf, size);

    /* the library may report anything, but must not crash on any getter */
    (void)hidpp_device_name(dev);
    (void)hidpp_nickname_get(dev);
    (void)hidpp_nickname_default(dev);
    (void)hidpp_serial_number(dev);
    (void)hidpp_battery_info(dev);
    (void)hidpp_firmware_info(dev);
    (void)hidpp_firmware_entity(dev, 0);
    hidpp_keymap_t *map = hidpp_keymap(dev);
    uint16_t ids[300];
    if (map)
        (void)hidpp_keymap_list(map, ids, 300);
    uint16_t features[300];
    (void)hidpp_feature_list(dev, features, 300);
    fake_free(f);
    return 0;
}

/* ------------------------------ clear -------------------------------- */

static int test_clear_forces_new_requests(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    read_everything(dev);
    fake_reset_log(f);

    hidpp_cache_clear(dev);
    read_everything(dev);
    T_TRUE(f->requests > 10);
    fake_free(f);
    return 0;
}

static int test_clear_picks_up_changes(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    T_STR(hidpp_device_name(dev), "MX Master 3S");
    T_EQ(hidpp_battery_info(dev)->criticalLevel, 5);
    T_EQ(hidpp_feature_index(dev, 0x2201), HIDPP_ENOENT);

    strcpy(f->name, "Other device");
    f->batCritical = 9;
    int idx = fake_add_feature(f, 0x2201, 0, t_null_handler);

    hidpp_cache_clear(dev);
    T_STR(hidpp_device_name(dev), "Other device");
    T_EQ(hidpp_battery_info(dev)->criticalLevel, 9);
    T_EQ(hidpp_feature_index(dev, 0x2201), idx);
    fake_free(f);
    return 0;
}

static int test_clear_null_device(int seed, int i) {
    (void)seed, (void)i;
    hidpp_cache_clear(NULL);
    return 0;
}

static int test_clear_twice_and_empty(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    hidpp_cache_clear(dev);
    hidpp_cache_clear(dev);
    read_everything(dev);
    fake_free(f);
    return 0;
}

/* ----------------------------- collect ------------------------------- */

static int test_collect_prefetches_everything(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    hidpp_cache_collect(dev);
    fake_reset_log(f);

    read_everything(dev);
    unsigned total = 0;
    for (unsigned idx = 0; idx < 256; idx++)
        if (idx != FAKE_IDX_NICKNAME)
            total += fake_calls(f, (uint8_t)idx);
    T_EQ(total, 0);
    fake_free(f);
    return 0;
}

static int test_collect_is_idempotent(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    hidpp_cache_collect(dev);
    fake_reset_log(f);
    hidpp_cache_collect(dev);
    T_EQ(f->requests, 0);
    fake_free(f);
    return 0;
}

static int test_collect_null_device(int seed, int i) {
    (void)seed, (void)i;
    hidpp_cache_collect(NULL);
    return 0;
}

static int test_collect_on_minimal_device(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    hidpp_device_t *dev = t_open(f);
    hidpp_cache_collect(dev);
    fake_free(f);
    return 0;
}

static const pf_test_t cache_tests[] = {
    { test_id, "id" },
    { test_id_is_stable, "id_is_stable" },
    { test_id_changes_with_firmware, "id_changes_with_firmware" },
    { test_id_ignores_other_entities, "id_ignores_other_entities" },
    { test_id_without_firmware_feature, "id_without_firmware_feature" },
    { test_id_unresponsive_device, "id_unresponsive_device" },
    { test_id_null_device, "id_null_device" },
    { test_id_bypasses_cached_indexes, "id_bypasses_cached_indexes" },
    { test_save_reports_size, "save_reports_size" },
    { test_save_into_exactly_sized_buffer, "save_exactly_sized_buffer" },
    { test_save_too_small, "save_too_small" },
    { test_save_invalid_arguments, "save_invalid_arguments" },
    { test_save_is_deterministic, "save_is_deterministic" },
    { test_save_starts_with_magic_and_id, "save_magic_and_id" },
    { test_save_without_collecting, "save_without_collecting" },
    { test_roundtrip_answers_without_requests, "roundtrip_no_requests" },
    { test_roundtrip_restores_values, "roundtrip_restores_values" },
    { test_roundtrip_of_a_minimal_device, "roundtrip_minimal_device" },
    { test_load_rejects_other_firmware, "load_rejects_other_firmware" },
    { test_load_rejects_bad_magic, "load_rejects_bad_magic" },
    { test_load_rejects_wrong_id_bytes, "load_rejects_wrong_id" },
    { test_load_invalid_arguments, "load_invalid_arguments" },
    { test_load_truncated_buffer, "load_truncated_buffer" },
    { test_load_accepts_trailing_bytes, "load_accepts_trailing_bytes" },
    { test_load_garbage_after_header, "load_garbage_after_header" },
    { test_clear_forces_new_requests, "clear_forces_new_requests" },
    { test_clear_picks_up_changes, "clear_picks_up_changes" },
    { test_clear_null_device, "clear_null_device" },
    { test_clear_twice_and_empty, "clear_twice_and_empty" },
    { test_collect_prefetches_everything, "collect_prefetches" },
    { test_collect_is_idempotent, "collect_is_idempotent" },
    { test_collect_null_device, "collect_null_device" },
    { test_collect_on_minimal_device, "collect_minimal_device" },
    { 0 },
};

/* the garbage test is randomised: repeat it with different data */
const pf_suite_t suite_cache = { "cache", 4, cache_tests };
