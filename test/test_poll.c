/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

/*  Event polling: notifications are decoded according to the feature that
    sent them and everything that is not an event is skipped. */

#include "test.h"

enum {
    IDX_WHEEL = FAKE_IDX_KEYMAP + 1,
    IDX_RATCHET,
    IDX_TOUCHPAD,
    IDX_TOUCH_MOUSE,
    IDX_OTHER,
};

/* A device with the event producing features of the library's parser. */
static hidpp_device_t *event_device(fake_t **out) {
    fake_t *f = fake_new();
    T_EQ(fake_add_feature(f, 0x2121, 0, t_null_handler), IDX_WHEEL);
    T_EQ(fake_add_feature(f, 0x2130, 0, t_null_handler), IDX_RATCHET);
    T_EQ(fake_add_feature(f, 0x6100, 0, t_null_handler), IDX_TOUCHPAD);
    T_EQ(fake_add_feature(f, 0x6110, 0, t_null_handler), IDX_TOUCH_MOUSE);
    T_EQ(fake_add_feature(f, 0x7777, 0, t_null_handler), IDX_OTHER);
    hidpp_device_t *dev = t_open(f);

    /*  Feature ids are resolved while parsing an event. Resolving needs a
        request, and requests discard unrelated reports, so the cache is
        filled first to keep queued events safe. */
    uint16_t ids[32];
    T_EQ(hidpp_feature_list(dev, ids, 32), IDX_OTHER + 1);
    fake_reset_log(f);
    *out = f;
    return dev;
}

static struct hidpp_event poll_one(hidpp_device_t *dev) {
    struct hidpp_event e;
    memset(&e, 0xCD, sizeof(e));
    T_OK(hidpp_device_poll(dev, &e));
    return e;
}

/* ------------------------------ events ------------------------------- */

static int test_battery_event(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = event_device(&f);
    const uint8_t data[] = { 60, 40, 1, 2 };
    fake_notify(f, FAKE_IDX_BATTERY, 0, data, sizeof(data));

    struct hidpp_event e = poll_one(dev);
    T_EQ(e.type, HIDPP_EVENT_BATTERY);
    T_EQ(e.as.battery.chargeState, 60);
    T_EQ(e.as.battery.batteryLevel, 40);
    T_EQ(e.as.battery.chargeStatus, 1);
    T_EQ(e.as.battery.externalPower, 2);
    fake_free(f);
    return 0;
}

static int test_button_event(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = event_device(&f);
    const uint8_t data[] = { 0x00, 0x50, 0x00, 0xC3, 0x12, 0x34, 0x00, 0x00 };
    fake_notify(f, FAKE_IDX_KEYMAP, 0, data, sizeof(data));

    struct hidpp_event e = poll_one(dev);
    T_EQ(e.type, HIDPP_EVENT_BUTTON);
    T_EQ(e.as.buttons[0], 0x0050);
    T_EQ(e.as.buttons[1], 0x00C3);
    T_EQ(e.as.buttons[2], 0x1234);
    T_EQ(e.as.buttons[3], 0x0000);
    fake_free(f);
    return 0;
}

static int test_button_release_is_all_zero(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = event_device(&f);
    fake_notify(f, FAKE_IDX_KEYMAP, 0, (const uint8_t[8]) { 0 }, 8);

    struct hidpp_event e = poll_one(dev);
    T_EQ(e.type, HIDPP_EVENT_BUTTON);
    for (int n = 0; n < 4; n++)
        T_EQ(e.as.buttons[n], 0);
    fake_free(f);
    return 0;
}

static int test_mouse_event(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = event_device(&f);
    const uint8_t data[] = { 0x00, 0x0A, 0xFF, 0xF6 }; /* dx = 10, dy = -10 */
    fake_notify(f, FAKE_IDX_KEYMAP, 1, data, sizeof(data));

    struct hidpp_event e = poll_one(dev);
    T_EQ(e.type, HIDPP_EVENT_MOUSE);
    T_EQ(e.as.mouse[0], 10);
    T_EQ((int16_t)e.as.mouse[1], -10);
    fake_free(f);
    return 0;
}

static int test_legacy_button_event(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    int idx = fake_add_keymap(f, 0x1B00);
    hidpp_device_t *dev = t_open(f);
    uint16_t ids[8];
    hidpp_feature_list(dev, ids, 8);

    const uint8_t data[] = { 0x00, 0x51 };
    fake_notify(f, (uint8_t)idx, 0, data, sizeof(data));
    struct hidpp_event e = poll_one(dev);
    T_EQ(e.type, HIDPP_EVENT_BUTTON);
    T_EQ(e.as.buttons[0], 0x0051);
    fake_free(f);
    return 0;
}

static int test_wheel_event(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = event_device(&f);
    const uint8_t data[] = { 0x05, 0xFF, 0x38 }; /* delta = -200 */
    fake_notify(f, IDX_WHEEL, 0, data, sizeof(data));

    struct hidpp_event e = poll_one(dev);
    T_EQ(e.type, HIDPP_EVENT_WHEEL);
    T_EQ(e.as.wheel.flags, 0x05);
    T_EQ(e.as.wheel.delta, -200);
    fake_free(f);
    return 0;
}

static int test_wheel_positive_delta(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = event_device(&f);
    const uint8_t data[] = { 0x00, 0x01, 0x2C }; /* delta = 300 */
    fake_notify(f, IDX_WHEEL, 0, data, sizeof(data));
    T_EQ(poll_one(dev).as.wheel.delta, 300);
    fake_free(f);
    return 0;
}

static int test_ratchet_switch_event(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = event_device(&f);
    const uint8_t data[] = { 0x01 };
    fake_notify(f, IDX_WHEEL, 1, data, sizeof(data));

    struct hidpp_event e = poll_one(dev);
    T_EQ(e.type, HIDPP_EVENT_RATCHET_SWITCH);
    T_EQ(e.as.ratchetSwitch, 1);
    fake_free(f);
    return 0;
}

static int test_ratchet_event(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = event_device(&f);
    const uint8_t data[] = { 0xFD, 0x07 }; /* -3, +7 */
    fake_notify(f, IDX_RATCHET, 0, data, sizeof(data));

    struct hidpp_event e = poll_one(dev);
    T_EQ(e.type, HIDPP_EVENT_RATCHET);
    T_EQ(e.as.ratchet.deltaV, -3);
    T_EQ(e.as.ratchet.deltaH, 7);
    fake_free(f);
    return 0;
}

static int test_touch_mouse_status_event(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = event_device(&f);

    for (int bits = 0; bits < 4; bits++) {
        const uint8_t data[] = { (uint8_t)bits };
        fake_notify(f, IDX_TOUCH_MOUSE, 1, data, sizeof(data));
        struct hidpp_event e = poll_one(dev);
        T_EQ(e.type, HIDPP_EVENT_TOUCH_MOUSE_STATUS);
        T_EQ(e.as.touchMouseStatus.flags, bits);
        T_EQ(!!e.as.touchMouseStatus.buttonDown, !!(bits & 1));
        T_EQ(!!e.as.touchMouseStatus.mouseLifted, !!(bits & 2));
    }
    fake_free(f);
    return 0;
}

static int test_touch_pad_event(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = event_device(&f);

    uint8_t data[16] = {
        0x12, 0x34,                               /* timestamp */
        0x92, 0x34, 0x45, 0x67, 0x80, 0x20, 0x53, /* point 0 */
        0x41, 0x02, 0xC3, 0x04, 0x05, 0x06, 0x70, /* point 1 */
    };
    fake_notify(f, IDX_TOUCHPAD, 0, data, sizeof(data));

    struct hidpp_event e = poll_one(dev);
    T_EQ(e.type, HIDPP_EVENT_TOUCH_PAD_POINTS);
    T_EQ(e.as.touchPadPoints.timestamp, 0x1234);

    struct hidpp_touch_pad_point *p = &e.as.touchPadPoints.data[0];
    T_EQ(p->type, 2);
    T_EQ(p->x, 0x1234);
    T_EQ(p->status, 1);
    T_EQ(p->y, 0x0567);
    T_EQ(p->force, 0x80);
    T_EQ(p->area, 0x20);
    T_EQ(p->flags, 0x53);
    T_EQ(p->finger, 5);

    p = &e.as.touchPadPoints.data[1];
    T_EQ(p->type, 1);
    T_EQ(p->x, 0x0102);
    T_EQ(p->status, 3);
    T_EQ(p->y, 0x0304);
    T_EQ(p->force, 0x05);
    T_EQ(p->area, 0x06);
    T_EQ(p->finger, 7);
    fake_free(f);
    return 0;
}

static int test_touch_mouse_points_event(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = event_device(&f);
    uint8_t data[16];
    for (int n = 0; n < 16; n++)
        data[n] = (uint8_t)(n + 1);
    fake_notify(f, IDX_TOUCH_MOUSE, 0, data, sizeof(data));

    /* only the type is verified here; see the TODO test for the layout */
    struct hidpp_event e = poll_one(dev);
    T_EQ(e.type, HIDPP_EVENT_TOUCH_MOUSE_POINTS);
    fake_free(f);
    return 0;
}

/* --------------------------- unknown events -------------------------- */

static int test_unknown_feature(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = event_device(&f);
    const uint8_t data[] = { 1, 2, 3, 4, 5 };
    fake_notify(f, IDX_OTHER, 0, data, sizeof(data));

    struct hidpp_event e = poll_one(dev);
    T_EQ(e.type, HIDPP_EVENT_UNKNOWN);
    T_EQ(e.as.unknown.kind, HIDPP_KIND_LONG);
    T_EQ(e.as.unknown.device, f->index);
    T_EQ(e.as.unknown.feat, IDX_OTHER);
    T_EQ(e.as.unknown.func, 0);
    T_MEM(e.as.unknown.params, data, sizeof(data));
    fake_free(f);
    return 0;
}

static int test_unknown_function_of_known_feature(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = event_device(&f);

    fake_notify(f, FAKE_IDX_BATTERY, 5, NULL, 0);
    fake_notify(f, FAKE_IDX_KEYMAP, 9, NULL, 0);
    fake_notify(f, IDX_WHEEL, 7, NULL, 0);
    fake_notify(f, IDX_RATCHET, 1, NULL, 0);

    for (int n = 0; n < 4; n++)
        T_EQ(poll_one(dev).type, HIDPP_EVENT_UNKNOWN);
    fake_free(f);
    return 0;
}

static int test_function_is_the_high_nibble(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = event_device(&f);

    /* function 1 of the keymap is the raw XY event, function 0 the buttons */
    fake_notify(f, FAKE_IDX_KEYMAP, 1, NULL, 0);
    fake_notify(f, FAKE_IDX_KEYMAP, 0, NULL, 0);
    T_EQ(poll_one(dev).type, HIDPP_EVENT_MOUSE);
    T_EQ(poll_one(dev).type, HIDPP_EVENT_BUTTON);
    fake_free(f);
    return 0;
}

static int test_feature_index_beyond_the_list(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = event_device(&f);
    fake_notify(f, 0x7E, 0, NULL, 0);
    T_EQ(poll_one(dev).type, HIDPP_EVENT_UNKNOWN);
    fake_notify(f, 0xFE, 0, NULL, 0);
    T_EQ(poll_one(dev).type, HIDPP_EVENT_UNKNOWN);
    fake_free(f);
    return 0;
}

static int test_error_report_is_not_an_event(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = event_device(&f);

    /* error for a request to feature index 0x10: the byte which holds the
       feature index has a zero low nibble, like a notification would */
    const uint8_t err2[7] = { 0x10, 1, 0xFF, 0x10, 0x1E, 0x02, 0x00 };
    const uint8_t err1[7] = { 0x10, 1, 0x8F, 0x10, 0x1E, 0x01, 0x00 };
    fake_queue_raw(f, err2, sizeof(err2));
    fake_queue_raw(f, err1, sizeof(err1));

    struct hidpp_event e;
    T_ERR(hidpp_device_poll(dev, &e), HIDPP_ETIMEDOUT);
    T_ERR(hidpp_poll(f->rcv, &e), HIDPP_ETIMEDOUT);
    fake_free(f);
    return 0;
}

/* ----------------------------- filtering ----------------------------- */

static int test_responses_are_not_events(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = event_device(&f);

    /* anything with a software id belongs to some request */
    const uint8_t response[20] = { 0x11, 1, FAKE_IDX_BATTERY, 0x0E, 9, 9, 9 };
    fake_queue_raw(f, response, sizeof(response));
    const uint8_t data[] = { 70, 0, 0, 0 };
    fake_notify(f, FAKE_IDX_BATTERY, 0, data, sizeof(data));

    struct hidpp_event e = poll_one(dev);
    T_EQ(e.type, HIDPP_EVENT_BATTERY);
    T_EQ(e.as.battery.chargeState, 70);
    fake_free(f);
    return 0;
}

static int test_other_devices_are_ignored(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = event_device(&f);

    const uint8_t foreign[20] = { 0x11, 4, FAKE_IDX_BATTERY, 0x00, 11 };
    fake_queue_raw(f, foreign, sizeof(foreign));
    const uint8_t data[] = { 33 };
    fake_notify(f, FAKE_IDX_BATTERY, 0, data, sizeof(data));

    struct hidpp_event e = poll_one(dev);
    T_EQ(e.as.battery.chargeState, 33);
    fake_free(f);
    return 0;
}

static int test_events_arrive_in_order(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = event_device(&f);

    for (uint8_t n = 0; n < 20; n++)
        fake_notify(f, FAKE_IDX_BATTERY, 0, &n, 1);

    for (uint8_t n = 0; n < 20; n++) {
        struct hidpp_event e = poll_one(dev);
        T_EQ(e.type, HIDPP_EVENT_BATTERY);
        T_EQ(e.as.battery.chargeState, n);
    }
    fake_free(f);
    return 0;
}

static int test_event_from_short_report(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = event_device(&f);
    const uint8_t raw[7] = { 0x10, 1, FAKE_IDX_BATTERY, 0x00, 55, 44, 1 };
    fake_queue_raw(f, raw, sizeof(raw));

    struct hidpp_event e = poll_one(dev);
    T_EQ(e.type, HIDPP_EVENT_BATTERY);
    T_EQ(e.as.battery.chargeState, 55);
    T_EQ(e.as.battery.chargeStatus, 1);
    T_EQ(e.as.battery.externalPower, 0); /* not part of the short report */
    fake_free(f);
    return 0;
}

static int test_poll_timeout(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = event_device(&f);
    struct hidpp_event e;
    T_ERR(hidpp_device_poll(dev, &e), HIDPP_ETIMEDOUT);
    fake_free(f);
    return 0;
}

static int test_poll_timeout_after_only_filtered_reports(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = event_device(&f);
    const uint8_t response[7] = { 0x10, 1, 3, 0x0E, 0, 0, 0 };

    for (int n = 0; n < 5; n++)
        fake_queue_raw(f, response, sizeof(response));

    struct hidpp_event e;
    T_ERR(hidpp_device_poll(dev, &e), HIDPP_ETIMEDOUT);
    fake_free(f);
    return 0;
}

static int test_poll_receive_failure(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = event_device(&f);
    f->failRecv = 1;
    struct hidpp_event e;
    T_ERR(hidpp_device_poll(dev, &e), HIDPP_EIO);
    fake_free(f);
    return 0;
}

static int test_poll_invalid_arguments(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = event_device(&f);
    struct hidpp_event e;
    T_ERR(hidpp_device_poll(NULL, &e), HIDPP_EINVAL);
    T_ERR(hidpp_device_poll(dev, NULL), HIDPP_EINVAL);
    T_ERR(hidpp_poll(NULL, &e), HIDPP_EINVAL);
    T_ERR(hidpp_poll(f->rcv, NULL), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

static int test_event_after_request(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f;
    hidpp_device_t *dev = event_device(&f);

    T_OK(hidpp_ping(dev, 1));
    const uint8_t data[] = { 12 };
    fake_notify(f, FAKE_IDX_BATTERY, 0, data, sizeof(data));
    T_EQ(poll_one(dev).as.battery.chargeState, 12);
    T_OK(hidpp_ping(dev, 2));
    fake_free(f);
    return 0;
}

/* ----------------------------- receiver poll ------------------------- */

static int test_receiver_poll_gives_unknown_events(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    const uint8_t data[] = { 9, 8, 7 };
    fake_notify(f, FAKE_IDX_BATTERY, 0, data, sizeof(data));

    struct hidpp_event e;
    memset(&e, 0xCD, sizeof(e));
    T_OK(hidpp_poll(f->rcv, &e));
    T_EQ(e.type, HIDPP_EVENT_UNKNOWN);
    T_EQ(e.as.unknown.feat, FAKE_IDX_BATTERY);
    T_MEM(e.as.unknown.params, data, sizeof(data));
    fake_free(f);
    return 0;
}

static int test_receiver_poll_skips_responses(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    const uint8_t response[7] = { 0x10, 1, 3, 0x0E, 0, 0, 0 };
    fake_queue_raw(f, response, sizeof(response));
    fake_notify(f, 4, 2, NULL, 0);

    struct hidpp_event e;
    T_OK(hidpp_poll(f->rcv, &e));
    T_EQ(e.as.unknown.feat, 4);
    T_EQ(HIDPP_MSN(e.as.unknown.func), 2);
    fake_free(f);
    return 0;
}

static int test_receiver_poll_sees_every_device(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    const uint8_t other[7] = { 0x10, 5, 9, 0x00, 0, 0, 0 };
    fake_queue_raw(f, other, sizeof(other));

    struct hidpp_event e;
    T_OK(hidpp_poll(f->rcv, &e));
    T_EQ(e.as.unknown.device, 5);
    fake_free(f);
    return 0;
}

static int test_receiver_poll_timeout(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    struct hidpp_event e;
    T_ERR(hidpp_poll(f->rcv, &e), HIDPP_ETIMEDOUT);
    fake_free(f);
    return 0;
}

static const pf_test_t poll_tests[] = {
    { test_battery_event, "battery_event" },
    { test_button_event, "button_event" },
    { test_button_release_is_all_zero, "button_release_is_all_zero" },
    { test_mouse_event, "mouse_event" },
    { test_legacy_button_event, "legacy_button_event" },
    { test_wheel_event, "wheel_event" },
    { test_wheel_positive_delta, "wheel_positive_delta" },
    { test_ratchet_switch_event, "ratchet_switch_event" },
    { test_ratchet_event, "ratchet_event" },
    { test_touch_mouse_status_event, "touch_mouse_status_event" },
    { test_touch_pad_event, "touch_pad_event" },
    { test_touch_mouse_points_event, "touch_mouse_points_type" },
    /* The point stride (4 bytes) overlaps the fields read from `params[4]`;
       the real layout has to be confirmed against the 0x6110 specification. */
    { NULL, "touch_mouse_points_layout" },
    /* hidpp_request() consumes reports it does not recognise, so events
       arriving while a request is pending are lost. Needs a design change
       (e.g. an event queue on the receiver). */
    { NULL, "events_survive_pending_requests" },
    { test_unknown_feature, "unknown_feature" },
    { test_unknown_function_of_known_feature, "unknown_function" },
    { test_function_is_the_high_nibble, "function_is_the_high_nibble" },
    { test_feature_index_beyond_the_list, "feature_index_beyond_list" },
    { test_error_report_is_not_an_event, "error_report_is_not_an_event" },
    { test_responses_are_not_events, "responses_are_not_events" },
    { test_other_devices_are_ignored, "other_devices_are_ignored" },
    { test_events_arrive_in_order, "events_arrive_in_order" },
    { test_event_from_short_report, "event_from_short_report" },
    { test_poll_timeout, "poll_timeout" },
    { test_poll_timeout_after_only_filtered_reports, "poll_timeout_filtered" },
    { test_poll_receive_failure, "poll_receive_failure" },
    { test_poll_invalid_arguments, "poll_invalid_arguments" },
    { test_event_after_request, "event_after_request" },
    { test_receiver_poll_gives_unknown_events, "receiver_poll_unknown" },
    { test_receiver_poll_skips_responses, "receiver_poll_skips_responses" },
    { test_receiver_poll_sees_every_device, "receiver_poll_every_device" },
    { test_receiver_poll_timeout, "receiver_poll_timeout" },
    { 0 },
};

const pf_suite_t suite_poll = { "poll", 1, poll_tests };
