/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

/*  Device firmware information (0x0003). */

#include "test.h"

static int test_info_fields(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);

    struct hidpp_firmware_info *info = hidpp_firmware_info(dev);
    T_NOT_NULL(info);
    T_TRUE(info->initialized);
    T_EQ(info->entityCount, 3);
    T_EQ(info->unitId, 0xDEADBEEF);
    T_EQ(info->transportFlags, 0x000D);
    T_TRUE(info->modelId == 0x4082B0230000ull);
    T_EQ(info->extendedModelId, 0x03);
    T_EQ(info->capabilities, 0x01);
    T_TRUE(info->hasSerialNumber);
    fake_free(f);
    return 0;
}

static int test_transport_flags(int seed, int i) {
    (void)seed, (void)i;
    static const struct {
        uint16_t flags;
        int usb, equad, btle, bt;
    } cases[] = {
        { 0x0000, 0, 0, 0, 0 }, { 0x0008, 1, 0, 0, 0 }, { 0x0004, 0, 1, 0, 0 },
        { 0x0002, 0, 0, 1, 0 }, { 0x0001, 0, 0, 0, 1 }, { 0x000F, 1, 1, 1, 1 },
        { 0x000D, 1, 1, 0, 1 }, { 0xFFF0, 0, 0, 0, 0 },
    };

    for (size_t n = 0; n < sizeof(cases) / sizeof(cases[0]); n++) {
        fake_t *f = fake_new();
        f->fwTransport = cases[n].flags;
        hidpp_device_t *dev = t_open(f);
        struct hidpp_firmware_info *info = hidpp_firmware_info(dev);
        T_NOT_NULL(info);
        T_EQ(info->transportFlags, cases[n].flags);
        T_EQ(info->supportsUSB, cases[n].usb);
        T_EQ(info->supportsEQuad, cases[n].equad);
        T_EQ(info->supportsBTLE, cases[n].btle);
        T_EQ(info->supportsBT, cases[n].bt);
        fake_free(f);
    }
    return 0;
}

static int test_info_is_cached(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    T_NOT_NULL(hidpp_firmware_info(dev));
    fake_reset_log(f);

    for (int n = 0; n < 10; n++) {
        T_NOT_NULL(hidpp_firmware_info(dev));
        T_NOT_NULL(hidpp_serial_number(dev));
    }
    T_EQ(f->requests, 0);
    fake_free(f);
    return 0;
}

static int test_serial_number(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    T_STR(hidpp_serial_number(dev), "ABCDEF123456");
    fake_free(f);
    return 0;
}

static int test_serial_number_is_terminated(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    /* all twelve bytes used: the library has to add the terminator */
    memcpy(f->fwSerial, "XXXXXXXXXXXX", 12);
    hidpp_device_t *dev = t_open(f);
    const char *serial = hidpp_serial_number(dev);
    T_NOT_NULL(serial);
    T_EQ(strlen(serial), 12);
    T_STR(serial, "XXXXXXXXXXXX");
    fake_free(f);
    return 0;
}

static int test_serial_number_not_supported(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    f->fwCapabilities = 0;
    hidpp_device_t *dev = t_open(f);
    T_NULL(hidpp_serial_number(dev));

    struct hidpp_firmware_info *info = hidpp_firmware_info(dev);
    T_NOT_NULL(info);
    T_FALSE(info->hasSerialNumber);

    /* function 2 must not even be attempted */
    for (unsigned n = 0; n < f->requests; n++) {
        const struct fake_packet *p = fake_logged(f, n);
        if (p->raw[2] == FAKE_IDX_FIRMWARE)
            T_NE(HIDPP_MSN(p->raw[3]), 2);
    }
    fake_free(f);
    return 0;
}

static int test_serial_capability_ignores_other_bits(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    f->fwCapabilities = 0xFE; /* everything but the serial number bit */
    hidpp_device_t *dev = t_open(f);
    T_NULL(hidpp_serial_number(dev));
    fake_free(f);
    return 0;
}

static int test_entity_main_application(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);

    struct hidpp_firmware_entity *e = hidpp_firmware_entity(dev, 0);
    T_NOT_NULL(e);
    T_TRUE(e->initialized);
    T_EQ(e->type, HIDPP_FIRMWARE_MAIN_APP);
    T_EQ(e->firmwareNumber, HIDPP_DWORD('R', 'B', 'M', 0x12));
    T_EQ(e->revision, 0x34);
    T_EQ(e->buildNumber, 0x0567);
    T_EQ(e->reserved, 1);
    T_NOT_NULL(e->typeName);
    T_TRUE(e->typeName[0] != '\0');

    const uint8_t specific[9] = { 0xB0, 0x23, 1, 2, 3, 4, 5, 0, 0 };
    T_MEM(e->specificInfo, specific, 9);
    fake_free(f);
    return 0;
}

static int test_entity_other_types(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);

    struct hidpp_firmware_entity *boot = hidpp_firmware_entity(dev, 1);
    struct hidpp_firmware_entity *hw = hidpp_firmware_entity(dev, 2);
    T_NOT_NULL(boot);
    T_NOT_NULL(hw);
    T_EQ(boot->type, HIDPP_FIRMWARE_BOOT_LOADER);
    T_EQ(hw->type, HIDPP_FIRMWARE_HARDWARE);
    T_EQ(boot->firmwareNumber, HIDPP_DWORD('B', 'L', '1', 0x03));
    T_EQ(boot->buildNumber, 0x0010);
    T_EQ(hw->buildNumber, 0);
    T_TRUE(boot != hw);
    T_FALSE(strcmp(boot->typeName, hw->typeName) == 0);
    fake_free(f);
    return 0;
}

static int test_entity_type_names_for_every_type(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    f->fwEntityCount = FAKE_MAX_ENTITIES;

    for (int t = 0; t < FAKE_MAX_ENTITIES; t++)
        f->fwEntities[t].type = (uint8_t)t;

    hidpp_device_t *dev = t_open(f);

    for (int t = 0; t < FAKE_MAX_ENTITIES; t++) {
        struct hidpp_firmware_entity *e = hidpp_firmware_entity(dev, t);
        T_NOT_NULL(e);
        T_EQ(e->type, t);
        T_NOT_NULL(e->typeName);
        T_TRUE(strlen(e->typeName) > 0);
        for (int u = 0; u < t; u++)
            T_FALSE(
                strcmp(hidpp_firmware_entity(dev, u)->typeName, e->typeName)
                == 0
            );
    }
    fake_free(f);
    return 0;
}

static int test_entity_type_reserved_bits_are_masked(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    f->fwTypeHighBits = 0xA0; /* only bits 0-3 carry the type */
    hidpp_device_t *dev = t_open(f);

    for (int n = 0; n < 3; n++) {
        struct hidpp_firmware_entity *e = hidpp_firmware_entity(dev, n);
        T_NOT_NULL(e);
        T_EQ(e->type, f->fwEntities[n].type);
        T_NOT_NULL(e->typeName);
    }
    fake_free(f);
    return 0;
}

static int test_entity_out_of_range(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    T_NOT_NULL(hidpp_firmware_info(dev));
    fake_reset_log(f);

    T_NULL(hidpp_firmware_entity(dev, 3));
    T_NULL(hidpp_firmware_entity(dev, 100));
    T_NULL(hidpp_firmware_entity(dev, 255));
    T_EQ(f->requests, 0); /* known to be invalid: no need to ask */
    fake_free(f);
    return 0;
}

static int test_entity_is_cached(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    struct hidpp_firmware_entity *a = hidpp_firmware_entity(dev, 1);
    T_NOT_NULL(a);
    fake_reset_log(f);
    struct hidpp_firmware_entity *b = hidpp_firmware_entity(dev, 1);
    T_TRUE(a == b);
    T_EQ(f->requests, 0);
    fake_free(f);
    return 0;
}

static int test_entity_count_zero(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    f->fwEntityCount = 0;
    hidpp_device_t *dev = t_open(f);
    struct hidpp_firmware_info *info = hidpp_firmware_info(dev);
    T_NOT_NULL(info);
    T_EQ(info->entityCount, 0);
    T_NULL(hidpp_firmware_entity(dev, 0));
    fake_free(f);
    return 0;
}

static int test_entity_count_maximum(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    f->fwEntityCount = FAKE_MAX_ENTITIES;
    for (int n = 0; n < FAKE_MAX_ENTITIES; n++)
        f->fwEntities[n].number = (uint8_t)(0x10 + n);
    hidpp_device_t *dev = t_open(f);

    for (int n = 0; n < FAKE_MAX_ENTITIES; n++) {
        struct hidpp_firmware_entity *e = hidpp_firmware_entity(dev, n);
        T_NOT_NULL(e);
        T_EQ(e->firmwareNumber & 0xFF, 0x10 + n);
    }
    fake_free(f);
    return 0;
}

static int test_entity_device_rejects_request(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    f->fwEntityCount = 3;
    hidpp_device_t *dev = t_open(f);
    T_NOT_NULL(hidpp_firmware_info(dev));

    f->fwEntityCount = 1; /* device now refuses entity 2 with an error */
    T_NULL(hidpp_firmware_entity(dev, 2));
    fake_free(f);
    return 0;
}

static int test_unsupported_feature(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    hidpp_device_t *dev = t_open(f);

    /* the answer must be the same on the first and on every later call */
    for (int n = 0; n < 3; n++) {
        T_NULL(hidpp_firmware_info(dev));
        T_NULL(hidpp_firmware_entity(dev, 0));
        T_NULL(hidpp_serial_number(dev));
    }
    fake_free(f);
    return 0;
}

static int test_null_device(int seed, int i) {
    (void)seed, (void)i;
    T_NULL(hidpp_firmware_info(NULL));
    T_NULL(hidpp_firmware_entity(NULL, 0));
    T_NULL(hidpp_serial_number(NULL));
    return 0;
}

static int test_device_dies_during_query(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    f->silent = 1;

    /* must neither crash nor invent data */
    struct hidpp_firmware_info *info = hidpp_firmware_info(dev);
    if (info) {
        T_EQ(info->entityCount, 0);
        T_NULL(hidpp_firmware_entity(dev, 0));
    }
    fake_free(f);
    return 0;
}

static const pf_test_t firmware_tests[] = {
    { test_info_fields, "info_fields" },
    { test_transport_flags, "transport_flags" },
    { test_info_is_cached, "info_is_cached" },
    { test_serial_number, "serial_number" },
    { test_serial_number_is_terminated, "serial_number_is_terminated" },
    { test_serial_number_not_supported, "serial_number_not_supported" },
    { test_serial_capability_ignores_other_bits, "serial_capability_bit" },
    { test_entity_main_application, "entity_main_application" },
    { test_entity_other_types, "entity_other_types" },
    { test_entity_type_names_for_every_type, "entity_type_names" },
    { test_entity_type_reserved_bits_are_masked, "entity_type_masked" },
    { test_entity_out_of_range, "entity_out_of_range" },
    { test_entity_is_cached, "entity_is_cached" },
    { test_entity_count_zero, "entity_count_zero" },
    { test_entity_count_maximum, "entity_count_maximum" },
    { test_entity_device_rejects_request, "entity_device_rejects" },
    { test_unsupported_feature, "unsupported_feature" },
    { test_null_device, "null_device" },
    { test_device_dies_during_query, "device_dies_during_query" },
    { 0 },
};

const pf_suite_t suite_firmware = { "firmware", 1, firmware_tests };
