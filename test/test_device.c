/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

/*  Device objects: opening, closing, protocol detection and the generic
    send/receive/request wrappers. */

#include "test.h"

static int corrupt_ping_hook(
    hidpp_receiver_t *rcv, hidpp_packet_t *req, hidpp_packet_t *res, void *user
) {
    (void)rcv, (void)user;
    if (res && req && req->feat == 0 && HIDPP_MSN(req->func) == 1)
        res->params[2] ^= 0xFF;
    return 0;
}

static int test_open_ok(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);

    struct hidpp_device_info info;
    T_OK(hidpp_device_info(dev, &info));
    T_EQ(info.major, 4);
    T_EQ(info.minor, 2);
    T_EQ(info.index, 1);
    fake_free(f);
    return 0;
}

static int test_open_sends_ping_first(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    t_open(f);
    const struct fake_packet *p = fake_logged(f, 0);
    T_NOT_NULL(p);
    T_EQ(p->raw[2], 0);            /* root feature */
    T_EQ(HIDPP_MSN(p->raw[3]), 1); /* function 1: ping */
    T_EQ(p->raw[6], 0xAA);         /* well known ping payload */
    fake_free(f);
    return 0;
}

static int test_open_twice_returns_same_object(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *a = t_open(f);
    unsigned before = f->requests;
    hidpp_device_t *b = hidpp_device_open(f->rcv, f->index);
    T_TRUE(a == b);
    T_EQ(f->requests, before); /* nothing is asked a second time */
    fake_free(f);
    return 0;
}

static int test_open_invalid_arguments(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    T_NULL(hidpp_device_open(NULL, 1));
    T_NULL(hidpp_device_open(f->rcv, 0)); /* 0 addresses the receiver */
    T_NULL(hidpp_device_open(f->rcv, 7));
    T_NULL(hidpp_device_open(f->rcv, 100));
    T_NULL(hidpp_device_open(f->rcv, 0xFE));
    T_EQ(f->requests, 0);
    fake_free(f);
    return 0;
}

static int test_open_every_valid_index(int seed, int i) {
    (void)seed, (void)i;
    for (int index = 1; index <= 6; index++) {
        fake_t *f = fake_new_bare();
        f->index = (uint8_t)index;
        hidpp_device_t *dev = t_open(f);
        struct hidpp_device_info info;
        T_OK(hidpp_device_info(dev, &info));
        T_EQ(info.index, index);
        fake_free(f);
    }
    return 0;
}

static int test_open_direct_bluetooth_index(int seed, int i) {
    (void)seed, (void)i;
    /* 0xFF addresses a device which is not behind a receiver */
    fake_t *f = fake_new_bare();
    f->index = 0xFF;
    hidpp_device_t *dev = t_open(f);
    T_OK(hidpp_ping(dev, 0x12));
    T_EQ(fake_last(f)->raw[1], 0xFF);
    T_TRUE(hidpp_device_open(f->rcv, 0xFF) == dev);
    fake_free(f);
    return 0;
}

static int test_open_unresponsive_device(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    f->silent = 1;
    T_NULL(fake_open(f));

    /* a failed open must not leave a half-initialised device behind */
    f->silent = 0;
    T_NOT_NULL(fake_open(f));
    fake_free(f);
    return 0;
}

static int test_open_wrong_device_answers(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    f->index = 3; /* the library will talk to the device at index 3 ... */
    T_NOT_NULL(fake_open(f));
    f->dev = NULL;

    fake_t *g = fake_new();
    /* ... but nobody answers at index 2 */
    T_NULL(hidpp_device_open(g->rcv, 2));
    fake_free(f);
    fake_free(g);
    return 0;
}

static int test_open_hidpp10_device_is_rejected(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    f->hidpp10 = 1;
    T_NULL(fake_open(f));
    fake_free(f);
    return 0;
}

static int test_open_protocol_versions(int seed, int i) {
    (void)seed, (void)i;
    static const struct {
        uint8_t major, minor;
        int ok;
    } cases[] = {
        { 1, 0, 0 }, { 1, 99, 0 }, { 0, 0, 0 }, { 2, 0, 1 },     { 2, 1, 1 },
        { 3, 0, 1 }, { 4, 2, 1 },  { 4, 5, 1 }, { 255, 255, 1 },
    };

    for (size_t n = 0; n < sizeof(cases) / sizeof(cases[0]); n++) {
        fake_t *f = fake_new_bare();
        f->protoMajor = cases[n].major;
        f->protoMinor = cases[n].minor;
        hidpp_device_t *dev = fake_open(f);

        if (cases[n].ok) {
            T_NOT_NULL(dev);
            struct hidpp_device_info info;
            T_OK(hidpp_device_info(dev, &info));
            T_EQ(info.major, cases[n].major);
            T_EQ(info.minor, cases[n].minor);
        } else {
            T_NULL(dev);
        }
        fake_free(f);
    }
    return 0;
}

static int test_open_rejects_wrong_ping_echo(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_set_protocol_hook(f->rcv, corrupt_ping_hook, NULL);
    T_NULL(fake_open(f));
    fake_free(f);
    return 0;
}

static int test_close_then_reopen(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    hidpp_device_close(dev);
    f->dev = NULL;

    hidpp_device_t *again = t_open(f);
    T_OK(hidpp_ping(again, 7));
    fake_free(f);
    return 0;
}

static int test_close_unregisters_device(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    hidpp_device_close(dev);

    /* the receiver must not hand out the freed object again */
    unsigned before = f->requests;
    hidpp_device_t *again = hidpp_device_open(f->rcv, f->index);
    T_NOT_NULL(again);
    T_TRUE(f->requests > before); /* it was initialised from scratch */
    f->dev = NULL;
    fake_free(f);
    return 0;
}

static int test_close_device_then_receiver(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    hidpp_device_close(dev);
    fake_free(f); /* closes the receiver: must not free the device twice */
    return 0;
}

static int test_close_receiver_closes_devices(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    t_open(f);
    fake_free(f); /* leaks or double frees show up under ASan/valgrind */
    return 0;
}

static int test_close_null_is_noop(int seed, int i) {
    (void)seed, (void)i;
    hidpp_device_close(NULL);
    return 0;
}

static int test_repeated_open_close(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();

    for (int n = 0; n < 50; n++) {
        hidpp_device_t *dev = t_open(f);
        T_OK(hidpp_ping(dev, (uint8_t)n));
        hidpp_device_close(dev);
        f->dev = NULL;
    }
    fake_free(f);
    return 0;
}

static int test_device_request_roundtrip(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);

    hidpp_packet_t req, res;
    T_OK(hidpp_make(&req, f->index, 0, HIDPP_BYTE(1, 14), NULL, 0));
    req.params[2] = 0x3C;
    T_OK(hidpp_device_request(dev, &req, &res));
    T_EQ(res.params[2], 0x3C);
    fake_free(f);
    return 0;
}

static int test_device_send_and_receive(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    fake_flush(f);

    hidpp_packet_t req, res;
    T_OK(hidpp_make(&req, f->index, 0, HIDPP_BYTE(1, 14), NULL, 0));
    req.params[2] = 0x21;
    T_OK(hidpp_device_send(dev, &req));
    T_OK(hidpp_device_receive(dev, &res));
    T_EQ(res.params[2], 0x21);
    fake_free(f);
    return 0;
}

static int test_device_wrappers_reject_null(int seed, int i) {
    (void)seed, (void)i;
    hidpp_packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    T_ERR(hidpp_device_send(NULL, &pkt), HIDPP_EINVAL);
    T_ERR(hidpp_device_receive(NULL, &pkt), HIDPP_EINVAL);
    T_ERR(hidpp_device_request(NULL, &pkt, &pkt), HIDPP_EINVAL);
    struct hidpp_event e;
    T_ERR(hidpp_device_poll(NULL, &e), HIDPP_EINVAL);
    struct hidpp_device_info info;
    T_ERR(hidpp_device_info(NULL, &info), HIDPP_EINVAL);
    T_ERR(hidpp_device_type(NULL), HIDPP_EINVAL);
    T_NULL(hidpp_device_name(NULL));
    return 0;
}

static int test_device_info_full(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);

    struct hidpp_device_info info;
    memset(&info, 0xAB, sizeof(info));
    T_OK(hidpp_device_info(dev, &info));
    T_EQ(info.major, 4);
    T_EQ(info.minor, 2);
    T_EQ(info.index, 1);
    T_EQ(info.type, HIDPP_TYPE_MOUSE);
    T_STR(info.name, "MX Master 3S");
    T_STR(info.nickname, "Desk mouse");
    T_STR(info.serialNumber, "ABCDEF123456");
    fake_free(f);
    return 0;
}

static int test_device_info_minimal_device(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    hidpp_device_t *dev = t_open(f);

    struct hidpp_device_info info;
    memset(&info, 0xAB, sizeof(info));
    T_OK(hidpp_device_info(dev, &info));
    T_EQ(info.type, 0);
    T_NULL(info.name);
    T_NULL(info.nickname);
    T_NULL(info.serialNumber);
    fake_free(f);
    return 0;
}

static int test_device_info_null_out(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    T_ERR(hidpp_device_info(dev, NULL), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

static const pf_test_t device_tests[] = {
    { test_open_ok, "open_ok" },
    { test_open_sends_ping_first, "open_sends_ping_first" },
    { test_open_twice_returns_same_object, "open_twice_same_object" },
    { test_open_invalid_arguments, "open_invalid_arguments" },
    { test_open_every_valid_index, "open_every_valid_index" },
    { test_open_direct_bluetooth_index, "open_direct_bluetooth_index" },
    { test_open_unresponsive_device, "open_unresponsive_device" },
    { test_open_wrong_device_answers, "open_wrong_device" },
    { test_open_hidpp10_device_is_rejected, "open_hidpp10_rejected" },
    { test_open_protocol_versions, "open_protocol_versions" },
    { test_open_rejects_wrong_ping_echo, "open_rejects_wrong_ping_echo" },
    { test_close_then_reopen, "close_then_reopen" },
    { test_close_unregisters_device, "close_unregisters_device" },
    { test_close_device_then_receiver, "close_device_then_receiver" },
    { test_close_receiver_closes_devices, "close_receiver_closes_devices" },
    { test_close_null_is_noop, "close_null_is_noop" },
    { test_repeated_open_close, "repeated_open_close" },
    { test_device_request_roundtrip, "request_roundtrip" },
    { test_device_send_and_receive, "send_and_receive" },
    { test_device_wrappers_reject_null, "wrappers_reject_null" },
    { test_device_info_full, "info_full" },
    { test_device_info_minimal_device, "info_minimal_device" },
    { test_device_info_null_out, "info_null_out" },
    { 0 },
};

const pf_suite_t suite_device = { "device", 1, device_tests };
