/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

/*  Report rates (0x8060, 0x8061), mode status (0x8090), audio sidetone
    (0x8300) and equalizer (0x8310), and the wireless device status event
    (0x1D4B). */

#include "test.h"

/* --------------------------- report rate ----------------------------- */

static int test_report_rate_basic(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = t_new();
    int idx = t_add_canned(f, 0x8060, 0);
    t_answer(idx, 0, (const uint8_t[]) { 0x8B }, 1); /* 1, 2, 4 and 8 ms */
    t_answer(idx, 1, (const uint8_t[]) { 2 }, 1);
    hidpp_device_t *dev = t_open(f);

    T_FALSE(hidpp_report_rate_info(dev)->isExtended);

    uint16_t rates[8];
    T_EQ(hidpp_report_rate_list(dev, HIDPP_CONNECTION_CURRENT, rates, 8), 4);
    T_EQ(rates[0], 1000);
    T_EQ(rates[1], 2000);
    T_EQ(rates[2], 4000);
    T_EQ(rates[3], 8000);
    T_EQ(hidpp_report_rate_list(dev, HIDPP_CONNECTION_CURRENT, rates, 2), 2);
    T_ERR(
        hidpp_report_rate_list(dev, HIDPP_CONNECTION_WIRED, rates, 8),
        HIDPP_EINVAL
    );

    T_EQ(hidpp_report_rate_get(dev, HIDPP_CONNECTION_CURRENT), 2000);
    T_ERR(hidpp_report_rate_get(dev, HIDPP_CONNECTION_WIRELESS), HIDPP_EINVAL);

    T_OK(hidpp_report_rate_set(dev, 4000));
    T_EQ(T_FUNC(f), 2);
    T_EQ(T_PARAM(f, 0), 4);
    T_ERR(hidpp_report_rate_set(dev, 3000), HIDPP_EINVAL); /* unsupported */
    T_ERR(hidpp_report_rate_set(dev, 1500), HIDPP_EINVAL);
    T_ERR(hidpp_report_rate_set(dev, 9000), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

static int report_rate_handler(
    fake_t *f, uint8_t func, const hidpp_packet_t *req, hidpp_packet_t *res
) {
    (void)f;

    switch (func) {
    case 0: /* wired: 125 Hz to 1 kHz, wireless: up to 8 kHz */
        if (req->params[0] > 1)
            return HIDPP_ERR_INVALID_ARGUMENT;
        res->params[1] = req->params[0] ? 0x7F : 0x0F;
        return 0;
    case 1:
        res->params[1] = 0x1F; /* up to 2 kHz right now */
        return 0;
    case 2:
        if (req->params[0] > 1)
            return HIDPP_ERR_INVALID_ARGUMENT;
        res->params[0] = req->params[0] ? 4 : 3;
        return 0;
    case 3:
        return 0;
    default:
        return HIDPP_ERR_INVALID_FUNCTION_ID;
    }
}

static int report_rate_index;

static fake_t *make_report_rate(void) {
    fake_t *f = t_new();
    fake_add_firmware_info(f);
    report_rate_index = fake_add_feature(f, 0x8061, 0, report_rate_handler);
    return f;
}

static int test_report_rate_extended(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_report_rate();
    hidpp_device_t *dev = t_open(f);
    T_TRUE(hidpp_report_rate_info(dev)->isExtended);

    uint16_t rates[8];
    T_EQ(hidpp_report_rate_list(dev, HIDPP_CONNECTION_WIRED, rates, 8), 4);
    T_EQ(rates[0], 1000);
    T_EQ(rates[3], 8000);
    T_EQ(hidpp_report_rate_list(dev, HIDPP_CONNECTION_WIRELESS, rates, 8), 7);
    T_EQ(rates[0], 125);
    T_EQ(rates[6], 8000);

    T_EQ(hidpp_report_rate_list(dev, HIDPP_CONNECTION_CURRENT, rates, 8), 5);
    T_EQ(T_FUNC(f), 1);
    T_EQ(rates[0], 500);
    T_ERR(hidpp_report_rate_list(dev, 2, rates, 8), HIDPP_EINVAL);

    /* the device sits behind a receiver */
    T_EQ(hidpp_report_rate_get(dev, HIDPP_CONNECTION_CURRENT), 500);
    T_EQ(T_PARAM(f, 0), HIDPP_CONNECTION_WIRELESS);
    T_EQ(hidpp_report_rate_get(dev, HIDPP_CONNECTION_WIRED), 1000);
    T_EQ(T_PARAM(f, 0), HIDPP_CONNECTION_WIRED);

    T_OK(hidpp_report_rate_set(dev, 250));
    T_EQ(T_FUNC(f), 3);
    T_EQ(T_PARAM(f, 0), 5);
    T_ERR(hidpp_report_rate_set(dev, 300), HIDPP_EINVAL);
    T_ERR(hidpp_report_rate_set(dev, 62), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

static int test_report_rate_event(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_report_rate();
    hidpp_device_t *dev = t_open(f);
    const uint8_t data[] = { HIDPP_CONNECTION_WIRELESS, 6 };
    struct hidpp_event e = t_event(f, dev, report_rate_index, 0, data, 2);
    T_EQ(e.type, HIDPP_EVENT_REPORT_RATE);
    T_EQ(e.as.reportRate.connection, HIDPP_CONNECTION_WIRELESS);
    T_EQ(e.as.reportRate.rate, 125);
    fake_free(f);
    return 0;
}

static int test_report_rate_unsupported_and_cache(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = t_new();
    hidpp_device_t *dev = t_open(f);
    uint16_t rates[4];
    T_NULL(hidpp_report_rate_info(dev));
    T_ERR(hidpp_report_rate_list(dev, 0xFF, rates, 4), HIDPP_ENOSYS);
    T_ERR(hidpp_report_rate_get(NULL, 0xFF), HIDPP_EINVAL);
    fake_free(f);

    dev = t_from_cache(make_report_rate, &f);
    T_EQ(hidpp_report_rate_list(dev, HIDPP_CONNECTION_WIRELESS, rates, 4), 4);
    T_EQ(rates[0], 125);
    T_EQ(f->requests, 0);
    fake_free(f);
    return 0;
}

static const pf_test_t report_rate_tests[] = {
    { test_report_rate_basic, "basic" },
    { test_report_rate_extended, "extended" },
    { test_report_rate_event, "event" },
    { test_report_rate_unsupported_and_cache, "unsupported_and_cache" },
    { 0 },
};

const pf_suite_t suite_report_rate = { "report_rate", 1, report_rate_tests };

/* --------------------------- mode status ----------------------------- */

static int mode_index;

static fake_t *make_mode_status_version(uint8_t version) {
    fake_t *f = t_new();
    fake_add_firmware_info(f);
    mode_index = t_add_canned(f, 0x8090, version);
    t_answer(mode_index, 0, (const uint8_t[]) { 1, 0 }, 2);
    t_answer(mode_index, 2, (const uint8_t[]) { 0, 3 }, 2);
    return f;
}

static fake_t *make_mode_status(void) { return make_mode_status_version(1); }

static int test_mode_status(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_mode_status();
    hidpp_device_t *dev = t_open(f);
    struct hidpp_mode_status_info *info = hidpp_mode_status_info(dev);
    T_NOT_NULL(info);
    T_EQ(info->capabilities, 3);
    T_TRUE(info->canSet && info->hasHwSwitch && info->hasSwSwitch);

    T_EQ(hidpp_mode_status_get(dev), HIDPP_MODE_PERFORMANCE);
    T_OK(hidpp_mode_status_set(dev, 0, HIDPP_MODE_PERFORMANCE));
    T_EQ(T_FUNC(f), 1);
    T_MEM(&fake_last(f)->raw[4], "\x00\x00\x01\x00", 4);
    T_OK(hidpp_mode_status_set(dev, 0xFFFF, HIDPP_MODE_PERFORMANCE));
    T_MEM(&fake_last(f)->raw[4], "\x01\x00\x01\x00", 4);
    fake_free(f);
    return 0;
}

static int test_mode_status_restrictions(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_mode_status_version(0);
    hidpp_device_t *dev = t_open(f);
    T_FALSE(hidpp_mode_status_info(dev)->canSet);
    T_ERR(hidpp_mode_status_set(dev, 1, 1), HIDPP_ENOSYS);
    T_EQ(hidpp_mode_status_get(dev), 1);
    fake_free(f);

    /* only a hardware switch changes the mode */
    f = make_mode_status();
    t_answer(mode_index, 2, (const uint8_t[]) { 0, 1 }, 2);
    dev = t_open(f);
    T_ERR(hidpp_mode_status_set(dev, 1, 1), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

static int test_mode_status_event_and_cache(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_mode_status();
    hidpp_device_t *dev = t_open(f);
    const uint8_t data[] = { 0, 0, 1, 0 };
    struct hidpp_event e = t_event(f, dev, mode_index, 0, data, 4);
    T_EQ(e.type, HIDPP_EVENT_MODE_STATUS);
    T_EQ(e.as.modeStatus.status, 0);
    T_EQ(e.as.modeStatus.changed, HIDPP_MODE_PERFORMANCE);
    fake_free(f);

    dev = t_from_cache(make_mode_status, &f);
    T_TRUE(hidpp_mode_status_info(dev)->hasSwSwitch);
    T_EQ(f->requests, 0);
    fake_free(f);

    f = t_new();
    dev = t_open(f);
    T_NULL(hidpp_mode_status_info(dev));
    T_ERR(hidpp_mode_status_get(dev), HIDPP_ENOSYS);
    fake_free(f);
    return 0;
}

static const pf_test_t mode_status_tests[] = {
    { test_mode_status, "mode_status" },
    { test_mode_status_restrictions, "restrictions" },
    { test_mode_status_event_and_cache, "event_and_cache" },
    { 0 },
};

const pf_suite_t suite_mode_status = { "mode_status", 1, mode_status_tests };

/* ----------------------------- sidetone ------------------------------ */

static int sidetone_index;

static hidpp_device_t *open_sidetone(fake_t **out, uint8_t version) {
    fake_t *f = t_new();
    sidetone_index = t_add_canned(f, 0x8300, version);
    t_answer(sidetone_index, 0, (const uint8_t[]) { 90 }, 1);
    t_answer(sidetone_index, 2, (const uint8_t[]) { 0x02 }, 1);
    *out = f;
    return t_open(f);
}

static int test_sidetone(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_sidetone(&f, 1);
    T_EQ(hidpp_sidetone_get(dev), 90);
    T_OK(hidpp_sidetone_set(dev, 100));
    T_EQ(T_FUNC(f), 1);
    T_EQ(T_PARAM(f, 0), 100);
    T_ERR(hidpp_sidetone_set(dev, 101), HIDPP_EINVAL);

    T_EQ(hidpp_sidetone_mute(dev), 0x02);
    T_OK(hidpp_sidetone_set_mute(dev, 0x03, 0x02)); /* example of the spec */
    T_EQ(T_FUNC(f), 3);
    T_EQ(T_PARAM(f, 0), 0x03);
    T_EQ(T_PARAM(f, 1), 0x02);
    T_OK(hidpp_sidetone_set_mute(dev, 0x01, 0xFF));
    T_EQ(T_PARAM(f, 1), 0x01); /* only masked channels */

    const uint8_t data[] = { 2, 40, 0x01 };
    struct hidpp_event e = t_event(f, dev, sidetone_index, 0, data, 3);
    T_EQ(e.type, HIDPP_EVENT_SIDETONE);
    T_EQ(e.as.sidetone.channel, 2);
    T_EQ(e.as.sidetone.level, 40);
    T_EQ(e.as.sidetone.mute, 0x01);
    fake_free(f);
    return 0;
}

static int test_sidetone_v0_and_unsupported(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = open_sidetone(&f, 0);
    T_EQ(hidpp_sidetone_get(dev), 90);
    T_ERR(hidpp_sidetone_mute(dev), HIDPP_ENOSYS);
    T_ERR(hidpp_sidetone_set_mute(dev, 1, 1), HIDPP_ENOSYS);
    fake_free(f);

    f = t_new();
    dev = t_open(f);
    T_ERR(hidpp_sidetone_get(dev), HIDPP_ENOSYS);
    T_ERR(hidpp_sidetone_set(NULL, 1), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

static const pf_test_t sidetone_tests[] = {
    { test_sidetone, "sidetone" },
    { test_sidetone_v0_and_unsupported, "v0_and_unsupported" },
    { 0 },
};

const pf_suite_t suite_sidetone = { "sidetone", 1, sidetone_tests };

/* ----------------------------- equalizer ----------------------------- */

static const uint16_t eq_frequencies[] = { 32,   64,   125,  250,  500,
                                           1000, 2000, 4000, 8000, 16000 };

static struct {
    uint8_t version;
    int8_t dbMin, dbMax;
} eq_state;

static int eq_handler(
    fake_t *f, uint8_t func, const hidpp_packet_t *req, hidpp_packet_t *res
) {
    (void)f;
    uint8_t *r = res->params;

    switch (func) {
    case 0:
        r[0] = 10;
        r[1] = 12;
        r[2] = 1;
        r[3] = (uint8_t)eq_state.dbMin;
        r[4] = (uint8_t)eq_state.dbMax;
        return 0;
    case 1: {
        uint8_t start = req->params[0];
        r[0] = start;

        for (uint8_t k = 0; k < 7 && start + k < 10; k++) {
            r[1 + 2 * k] = HIDPP_MSB(eq_frequencies[start + k]);
            r[2 + 2 * k] = HIDPP_LSB(eq_frequencies[start + k]);
        }
        return 0;
    }
    case 2: /* the RAM holds different gains */
        memcpy(r, "\x00\xF4\x0C\x00\x00\x00\x00\x00\x00\x00", 10);
        r[0] = req->params[0];
        return 0;
    case 3:
        return 0;
    case 4:
        r[0] = 1;
        return 0;
    case 5:
        return eq_state.version >= 2 ? 0 : HIDPP_ERR_INVALID_FUNCTION_ID;
    default:
        return HIDPP_ERR_INVALID_FUNCTION_ID;
    }
}

static int eq_index;

static fake_t *make_eq_version(uint8_t version, int8_t min, int8_t max) {
    fake_t *f = t_new();
    fake_add_firmware_info(f);
    memset(&eq_state, 0, sizeof(eq_state));
    eq_state.version = version;
    eq_state.dbMin = min;
    eq_state.dbMax = max;
    eq_index = fake_add_feature(f, 0x8310, version, eq_handler);
    return f;
}

static fake_t *make_eq(void) { return make_eq_version(2, -6, 0); }

static int test_eq_info(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_eq();
    hidpp_device_t *dev = t_open(f);
    struct hidpp_eq_info *info = hidpp_eq_info(dev);
    T_NOT_NULL(info);
    T_EQ(info->bandCount, 10);
    T_EQ(info->dbRange, 12);
    T_EQ(info->capabilities, 1);
    T_EQ(info->dbMin, -6);
    T_EQ(info->dbMax, 12); /* zero means +dbRange */
    T_TRUE(info->hasLocation && info->hasNoiseReduction);

    for (int k = 0; k < 10; k++)
        T_EQ(info->frequencies[k], eq_frequencies[k]);

    fake_free(f);
    return 0;
}

static int test_eq_info_v0(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_eq_version(0, -6, 6);
    hidpp_device_t *dev = t_open(f);
    struct hidpp_eq_info *info = hidpp_eq_info(dev);
    T_NOT_NULL(info);
    T_EQ(info->dbMin, -12); /* the fields do not exist yet */
    T_EQ(info->dbMax, 12);
    T_EQ(info->capabilities, 0);
    T_FALSE(info->hasLocation || info->hasNoiseReduction);

    int8_t gains[10];
    T_ERR(hidpp_eq_get(dev, 1, gains, 10), HIDPP_ENOSYS);
    T_EQ(hidpp_eq_get(dev, 0, gains, 10), 10);
    T_ERR(hidpp_eq_noise_reduction(dev), HIDPP_ENOSYS);
    fake_free(f);
    return 0;
}

static int test_eq_gains(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_eq();
    hidpp_device_t *dev = t_open(f);
    int8_t gains[10];
    T_EQ(hidpp_eq_get(dev, 1, gains, 10), 10);
    T_EQ(T_FUNC(f), 2);
    T_EQ(T_PARAM(f, 0), 1);
    T_EQ(gains[1], -12);
    T_EQ(gains[2], 12);
    T_ERR(hidpp_eq_get(dev, 0, gains, 9), HIDPP_EINVAL);

    /* the example of the specification: 125 Hz to -4 dB, 500 Hz to 4 dB */
    const int8_t set[10] = { 0, 0, -4, 0, 4 };
    T_OK(hidpp_eq_set(dev, set, 10, HIDPP_EQ_VOLATILE));
    T_EQ(T_FUNC(f), 3);
    T_MEM(&fake_last(f)->raw[4], "\x00\x00\x00\xFC\x00\x04\x00", 7);

    const int8_t low[10] = { -7 };
    T_ERR(hidpp_eq_set(dev, low, 10, 0), HIDPP_EINVAL);
    T_ERR(hidpp_eq_set(dev, set, 9, 0), HIDPP_EINVAL);
    T_ERR(hidpp_eq_set(dev, set, 10, 3), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

static int test_eq_noise_reduction(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = make_eq();
    hidpp_device_t *dev = t_open(f);
    T_EQ(hidpp_eq_noise_reduction(dev), 1);
    T_OK(hidpp_eq_set_noise_reduction(dev, 0));
    T_EQ(T_FUNC(f), 5);
    T_EQ(T_PARAM(f, 0), 0);
    fake_free(f);
    return 0;
}

static int test_eq_unsupported_and_cache(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = t_new();
    hidpp_device_t *dev = t_open(f);
    int8_t gains[4];
    T_NULL(hidpp_eq_info(dev));
    T_ERR(hidpp_eq_get(dev, 0, gains, 4), HIDPP_ENOSYS);
    fake_free(f);

    dev = t_from_cache(make_eq, &f);
    struct hidpp_eq_info *info = hidpp_eq_info(dev);
    T_EQ(info->frequencies[9], 16000);
    T_EQ(info->dbMin, -6);
    T_EQ(info->dbMax, 12);
    T_EQ(f->requests, 0);
    fake_free(f);
    return 0;
}

static const pf_test_t equalizer_tests[] = {
    { test_eq_info, "info" },
    { test_eq_info_v0, "info_v0" },
    { test_eq_gains, "gains" },
    { test_eq_noise_reduction, "noise_reduction" },
    { test_eq_unsupported_and_cache, "unsupported_and_cache" },
    { 0 },
};

const pf_suite_t suite_equalizer = { "equalizer", 1, equalizer_tests };

/* ------------------------------ events ------------------------------- */

static int test_wireless_status_event(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = t_new();
    int idx = t_add_canned(f, 0x1D4B, 0);
    hidpp_device_t *dev = t_open(f);
    const uint8_t data[] = { 1, 1, 1 };
    struct hidpp_event e = t_event(f, dev, idx, 0, data, 3);
    T_EQ(e.type, HIDPP_EVENT_WIRELESS_STATUS);
    T_EQ(e.as.wirelessStatus.status, 1);
    T_EQ(e.as.wirelessStatus.request, 1);
    T_EQ(e.as.wirelessStatus.reason, 1);
    fake_free(f);
    return 0;
}

/*  Every new event fits into the union, and none of the parsers reads
    past the payload of a long report when the device sends garbage. */
static int test_events_survive_garbage(int seed, int i) {
    (void)i;
    uint32_t rng = (uint32_t)seed | 1;
    const uint16_t ids[] = { 0x1D4B, 0x2121, 0x2150, 0x2202, 0x40A3, 0x4301,
                             0x4530, 0x4531, 0x4600, 0x6100, 0x6110, 0x8040,
                             0x8061, 0x8070, 0x8071, 0x8090, 0x8300 };
    const size_t count = sizeof(ids) / sizeof(ids[0]);

    fake_t *f = t_new();
    for (size_t k = 0; k < count; k++)
        t_add_canned(f, ids[k], 0);
    hidpp_device_t *dev = t_open(f);

    for (int n = 0; n < 300; n++) {
        uint8_t data[16];
        for (int k = 0; k < 16; k++)
            data[k] = (uint8_t)t_rand(&rng);

        int idx = 2 + (int)(t_rand(&rng) % count);
        struct hidpp_event e = t_event(
            f, dev, idx, (uint8_t)(t_rand(&rng) % 4), data, sizeof(data)
        );
        T_TRUE(e.type > HIDPP_EVENT_NONE && e.type < HIDPP__EVENT_MAX);
    }

    fake_free(f);
    return 0;
}

static const pf_test_t events_tests[] = {
    { test_wireless_status_event, "wireless_status" },
    { test_events_survive_garbage, "survive_garbage" },
    { 0 },
};

const pf_suite_t suite_events = { "events", 1, events_tests };
