/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

/*  Receiver level behaviour: custom receivers, sending, receiving and the
    request/response matching including HID++ error reports. */

#include "test.h"

#include <wchar.h>

/* ping request for the fake device at the default software id */
static hidpp_packet_t ping(fake_t *f, uint8_t data) {
    hidpp_packet_t req;
    memset(&req, 0, sizeof(req));
    T_OK(hidpp_make(&req, f->index, 0, HIDPP_BYTE(1, 14), NULL, 0));
    req.params[2] = data;
    return req;
}

static int unused_send(
    hidpp_receiver_t *rcv, const char *buffer, size_t size, void *user
) {
    (void)rcv, (void)buffer, (void)size, (void)user;
    return HIDPP_OK;
}

static int unused_recv(
    hidpp_receiver_t *rcv, char *buffer, size_t size, void *user
) {
    (void)rcv, (void)buffer, (void)size, (void)user;
    return 0;
}

/* an extra feature answering requests with an error code from params[0] */
static int error_handler(
    fake_t *f, uint8_t func, const hidpp_packet_t *req, hidpp_packet_t *res
) {
    (void)f, (void)func, (void)res;
    return req->params[0];
}

/* --------------------------- open / close ---------------------------- */

static int test_open_custom_requires_callbacks(int seed, int i) {
    (void)seed, (void)i;
    T_NULL(hidpp_open_custom(NULL, unused_recv, NULL));
    T_NULL(hidpp_open_custom(unused_send, NULL, NULL));
    T_NULL(hidpp_open_custom(NULL, NULL, NULL));
    return 0;
}

static int test_open_custom_and_close(int seed, int i) {
    (void)seed, (void)i;
    hidpp_receiver_t *rcv = hidpp_open_custom(unused_send, unused_recv, NULL);
    T_NOT_NULL(rcv);
    hidpp_close(rcv);
    return 0;
}

static int test_close_null_is_noop(int seed, int i) {
    (void)seed, (void)i;
    hidpp_close(NULL);
    return 0;
}

static int test_works_without_init(int seed, int i) {
    (void)seed, (void)i;
    /* custom receivers must not require hidpp_init() (no hidapi needed) */
    fake_t *f = fake_new_bare();
    hidpp_packet_t req = ping(f, 0x11), res;
    T_OK(hidpp_request(f->rcv, &req, &res));
    fake_free(f);
    return 0;
}

static int test_user_pointer_reaches_callbacks(int seed, int i) {
    (void)seed, (void)i;
    /* fake_t is passed as the user pointer; a response proves it arrived */
    fake_t *f = fake_new_bare();
    hidpp_packet_t req = ping(f, 0x99), res;
    T_OK(hidpp_request(f->rcv, &req, &res));
    T_EQ(res.params[2], 0x99);
    fake_free(f);
    return 0;
}

static int test_info_not_supported_for_custom(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    struct hidpp_receiver_info info;
    T_ERR(hidpp_receiver_info(f->rcv, &info), HIDPP_ENOSYS);
    T_ERR(hidpp_receiver_info(NULL, &info), HIDPP_EINVAL);
    T_ERR(hidpp_receiver_info(f->rcv, NULL), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

static int test_nonblocking_not_supported_for_custom(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    T_ERR(hidpp_set_nonblocking(f->rcv, 1), HIDPP_ENOSYS);
    T_ERR(hidpp_set_nonblocking(NULL, 1), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

static int test_set_timeout(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    T_OK(hidpp_set_timeout(f->rcv, 10));
    T_OK(hidpp_set_timeout(f->rcv, 0));
    T_ERR(hidpp_set_timeout(NULL, 10), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

static int test_no_error_message_initially(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    T_NULL(hidpp_error(f->rcv));
    fake_free(f);
    return 0;
}

/* ------------------------------ send --------------------------------- */

static int test_send_short_report(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    hidpp_packet_t req;
    uint8_t data[3] = { 0xA1, 0xA2, 0xA3 };
    T_OK(hidpp_make(&req, 1, 0, 0x1E, data, 3));
    T_OK(hidpp_send(f->rcv, &req));

    const struct fake_packet *p = fake_last(f);
    T_NOT_NULL(p);
    T_EQ(p->len, HIDPP_LEN_SHORT);
    const uint8_t expect[] = { 0x10, 1, 0, 0x1E, 0xA1, 0xA2, 0xA3 };
    T_MEM(p->raw, expect, sizeof(expect));
    fake_free(f);
    return 0;
}

static int test_send_long_report(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    f->silent = 1;
    hidpp_packet_t req;
    uint8_t data[16];
    for (int n = 0; n < 16; n++)
        data[n] = (uint8_t)(0x40 + n);
    T_OK(hidpp_make(&req, 1, 0, 0x1E, data, 16));
    T_OK(hidpp_send(f->rcv, &req));

    const struct fake_packet *p = fake_last(f);
    T_EQ(p->len, HIDPP_LEN_LONG);
    T_EQ(p->raw[0], HIDPP_KIND_LONG);
    T_MEM(&p->raw[4], data, 16);
    fake_free(f);
    return 0;
}

static int test_send_xlong_report(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    f->silent = 1;
    hidpp_packet_t req;
    uint8_t data[60];
    for (int n = 0; n < 60; n++)
        data[n] = (uint8_t)n;
    T_OK(hidpp_make(&req, 1, 0, 0x1E, data, 60));
    T_OK(hidpp_send(f->rcv, &req));

    const struct fake_packet *p = fake_last(f);
    T_EQ(p->len, HIDPP_LEN_XLONG);
    T_EQ(p->raw[0], HIDPP_KIND_XLONG);
    T_MEM(&p->raw[4], data, 60);
    fake_free(f);
    return 0;
}

static int test_send_short_does_not_leak_params(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    f->silent = 1;
    hidpp_packet_t req;
    memset(&req, 0xEE, sizeof(req));
    req.kind = HIDPP_KIND_SHORT;
    req.device = 1;
    req.feat = 0;
    req.func = 0x1E;
    T_OK(hidpp_send(f->rcv, &req));
    /* only 3 parameter bytes exist in a short report */
    T_EQ(fake_last(f)->len, HIDPP_LEN_SHORT);
    fake_free(f);
    return 0;
}

static int test_send_invalid_kind(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    hidpp_packet_t req;
    memset(&req, 0, sizeof(req));

    uint8_t kinds[] = { 0x00, 0x0F, 0x13, 0x20, 0xFF };
    for (size_t n = 0; n < sizeof(kinds); n++) {
        req.kind = kinds[n];
        T_ERR(hidpp_send(f->rcv, &req), HIDPP_EINVAL);
    }
    T_EQ(f->requests, 0);
    fake_free(f);
    return 0;
}

static int test_send_null_arguments(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    hidpp_packet_t req = ping(f, 0);
    T_ERR(hidpp_send(NULL, &req), HIDPP_EINVAL);
    T_ERR(hidpp_send(f->rcv, NULL), HIDPP_EINVAL);
    T_EQ(f->requests, 0);
    fake_free(f);
    return 0;
}

static int test_send_failure_reports_eio(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    f->failSend = 1;
    hidpp_packet_t req = ping(f, 0);
    T_ERR(hidpp_send(f->rcv, &req), HIDPP_EIO);
    T_NOT_NULL(hidpp_error(f->rcv));
    fake_free(f);
    return 0;
}

static int test_send_uses_request_device_index(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    f->silent = 1;
    hidpp_packet_t req;
    T_OK(hidpp_make(&req, 0xFF, 0, 0x1E, NULL, 0));
    T_OK(hidpp_send(f->rcv, &req));
    T_EQ(fake_last(f)->raw[1], 0xFF);
    fake_free(f);
    return 0;
}

/* ----------------------------- receive ------------------------------- */

static int test_receive_parses_long_report(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    uint8_t raw[20] = { 0x11, 0x01, 0x07, 0x2E };
    for (int n = 4; n < 20; n++)
        raw[n] = (uint8_t)(0x80 + n);
    fake_queue_raw(f, raw, sizeof(raw));

    hidpp_packet_t p;
    memset(&p, 0, sizeof(p));
    T_OK(hidpp_receive(f->rcv, &p));
    T_EQ(p.kind, 0x11);
    T_EQ(p.device, 0x01);
    T_EQ(p.feat, 0x07);
    T_EQ(p.func, 0x2E);
    T_MEM(p.params, &raw[4], 16);
    fake_free(f);
    return 0;
}

static int test_receive_short_report_is_zero_padded(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    const uint8_t raw[7] = { 0x10, 0x02, 0x03, 0x40, 0xAA, 0xBB, 0xCC };
    fake_queue_raw(f, raw, sizeof(raw));

    hidpp_packet_t p;
    memset(&p, 0xFF, sizeof(p));
    T_OK(hidpp_receive(f->rcv, &p));
    T_EQ(p.params[0], 0xAA);
    T_EQ(p.params[2], 0xCC);
    for (size_t n = 3; n < sizeof(p.params); n++)
        T_EQ(p.params[n], 0); /* never stale or uninitialised bytes */
    fake_free(f);
    return 0;
}

static int test_receive_xlong_report(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    uint8_t raw[64];
    for (int n = 0; n < 64; n++)
        raw[n] = (uint8_t)n;
    raw[0] = 0x12;
    fake_queue_raw(f, raw, sizeof(raw));

    hidpp_packet_t p;
    T_OK(hidpp_receive(f->rcv, &p));
    T_EQ(p.kind, 0x12);
    T_MEM(p.params, &raw[4], 60);
    fake_free(f);
    return 0;
}

static int test_receive_in_order(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();

    for (uint8_t n = 0; n < 10; n++) {
        const uint8_t raw[7] = { 0x10, 1, n, 0, 0, 0, 0 };
        fake_queue_raw(f, raw, sizeof(raw));
    }

    for (uint8_t n = 0; n < 10; n++) {
        hidpp_packet_t p;
        T_OK(hidpp_receive(f->rcv, &p));
        T_EQ(p.feat, n);
    }
    fake_free(f);
    return 0;
}

static int test_receive_timeout(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    hidpp_packet_t p;
    T_ERR(hidpp_receive(f->rcv, &p), HIDPP_ETIMEDOUT);
    T_NOT_NULL(hidpp_error(f->rcv));
    fake_free(f);
    return 0;
}

static int test_receive_callback_failure(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    f->failRecv = 1;
    hidpp_packet_t p;
    T_ERR(hidpp_receive(f->rcv, &p), HIDPP_EIO);
    fake_free(f);
    return 0;
}

static int test_receive_truncated_report(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    const uint8_t raw[3] = { 0x11, 0x01, 0x00 };
    fake_queue_raw(f, raw, sizeof(raw));
    hidpp_packet_t p;
    T_ERR(hidpp_receive(f->rcv, &p), HIDPP_EIO);
    fake_free(f);
    return 0;
}

static int test_receive_null_arguments(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    hidpp_packet_t p;
    T_ERR(hidpp_receive(NULL, &p), HIDPP_EINVAL);
    T_ERR(hidpp_receive(f->rcv, NULL), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

/* ----------------------------- request ------------------------------- */

static int test_request_roundtrip(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    hidpp_packet_t req = ping(f, 0x5A), res;
    T_OK(hidpp_request(f->rcv, &req, &res));
    T_EQ(res.device, f->index);
    T_EQ(res.feat, 0);
    T_EQ(res.func, 0x1E);
    T_EQ(res.params[0], 4); /* protocol 4.2 */
    T_EQ(res.params[1], 2);
    T_EQ(res.params[2], 0x5A);
    T_EQ(f->requests, 1);
    fake_free(f);
    return 0;
}

static int test_request_null_arguments(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    hidpp_packet_t req = ping(f, 0), res;
    T_ERR(hidpp_request(NULL, &req, &res), HIDPP_EINVAL);
    T_ERR(hidpp_request(f->rcv, NULL, &res), HIDPP_EINVAL);
    T_ERR(hidpp_request(f->rcv, &req, NULL), HIDPP_EINVAL);
    T_EQ(f->requests, 0);
    fake_free(f);
    return 0;
}

static int test_request_many_in_a_row(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();

    for (int n = 0; n < 200; n++) {
        hidpp_packet_t req = ping(f, (uint8_t)n), res;
        T_OK(hidpp_request(f->rcv, &req, &res));
        T_EQ(res.params[2], (uint8_t)n);
    }
    T_EQ(fake_pending(f), 0);
    fake_free(f);
    return 0;
}

static int test_request_timeout_without_answer(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    f->silent = 1;
    hidpp_packet_t req = ping(f, 1), res;
    T_ERR(hidpp_request(f->rcv, &req, &res), HIDPP_ETIMEDOUT);
    T_EQ(f->requests, 1); /* sent exactly once, never retransmitted */
    fake_free(f);
    return 0;
}

static int test_request_dropped_then_ok(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    f->drop = 1;
    hidpp_packet_t req = ping(f, 1), res;
    T_ERR(hidpp_request(f->rcv, &req, &res), HIDPP_ETIMEDOUT);
    T_OK(hidpp_request(f->rcv, &req, &res)); /* receiver is still usable */
    fake_free(f);
    return 0;
}

static int test_request_gives_up_after_unrelated_reports(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    f->silent = 1;

    /* more unrelated notifications than the retry budget allows */
    for (int n = 0; n < 40; n++)
        fake_notify(f, 5, 0, NULL, 0);

    hidpp_packet_t req = ping(f, 1), res;
    T_ERR(hidpp_request(f->rcv, &req, &res), HIDPP_ENODATA);
    T_NOT_NULL(hidpp_error(f->rcv));
    /* a bounded number was consumed, the rest is left for the caller */
    T_TRUE(fake_pending(f) > 0);
    T_TRUE(fake_pending(f) < 40);
    fake_free(f);
    return 0;
}

static int test_request_discards_stale_input_before_response(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();

    for (int n = 0; n < 5; n++)
        fake_notify(f, 7, 1, NULL, 0);

    hidpp_packet_t req = ping(f, 0x33), res;
    T_OK(hidpp_request(f->rcv, &req, &res));
    T_EQ(res.params[2], 0x33);
    fake_free(f);
    return 0;
}

static int noise_case(int mask) {
    fake_t *f = fake_new_bare();
    f->noise = mask;

    for (int n = 0; n < 5; n++) {
        hidpp_packet_t req = ping(f, (uint8_t)(0x10 + n)), res;
        T_OK(hidpp_request(f->rcv, &req, &res));
        /* the genuine answer is returned, not the noise */
        T_EQ(res.params[0], 4);
        T_EQ(res.params[2], (uint8_t)(0x10 + n));
        T_EQ(res.func, 0x1E);
    }
    T_EQ(fake_pending(f), 0);
    fake_free(f);
    return 0;
}

static int test_request_skips_notification(int seed, int i) {
    (void)seed, (void)i;
    /* same feature and function, but software id 0 */
    return noise_case(FAKE_NOISE_NOTIFY);
}

static int test_request_skips_other_device(int seed, int i) {
    (void)seed, (void)i;
    return noise_case(FAKE_NOISE_OTHER_DEVICE);
}

static int test_request_skips_other_swid(int seed, int i) {
    (void)seed, (void)i;
    /* responses meant for another software must not be accepted */
    return noise_case(FAKE_NOISE_OTHER_SWID);
}

static int test_request_skips_other_function(int seed, int i) {
    (void)seed, (void)i;
    return noise_case(FAKE_NOISE_OTHER_FUNC);
}

static int test_request_skips_foreign_report(int seed, int i) {
    (void)seed, (void)i;
    return noise_case(FAKE_NOISE_FOREIGN_REPORT);
}

static int test_request_skips_all_noise_combined(int seed, int i) {
    (void)seed, (void)i;
    return noise_case(
        FAKE_NOISE_NOTIFY | FAKE_NOISE_OTHER_DEVICE | FAKE_NOISE_OTHER_SWID
        | FAKE_NOISE_OTHER_FUNC | FAKE_NOISE_FOREIGN_REPORT
    );
}

static int test_request_rejects_foreign_swid_response(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    f->swidOffset = 1; /* the only answer carries somebody else's swid */
    hidpp_packet_t req = ping(f, 7), res;
    T_ERR(hidpp_request(f->rcv, &req, &res), HIDPP_ETIMEDOUT);
    fake_free(f);
    return 0;
}

static int test_request_error_report(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    hidpp_packet_t req, res;
    /* feature index 0xEE does not exist */
    T_OK(hidpp_make(&req, f->index, 0xEE, 0x1E, NULL, 0));
    T_ERR(hidpp_request(f->rcv, &req, &res), HIDPP_EIO);

    const wchar_t *msg = hidpp_error(f->rcv);
    T_NOT_NULL(msg);
    T_NOT_NULL(wcsstr(msg, L"Invalid feature index"));
    T_NOT_NULL(wcsstr(msg, L"2.0"));
    fake_free(f);
    return 0;
}

static int test_request_error_report_long_kind(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    f->errorKind = HIDPP_KIND_LONG; /* Bluetooth devices only use long */
    hidpp_packet_t req, res;
    T_OK(hidpp_make(&req, f->index, 0xEE, 0x1E, NULL, 0));
    T_ERR(hidpp_request(f->rcv, &req, &res), HIDPP_EIO);
    T_NOT_NULL(wcsstr(hidpp_error(f->rcv), L"Invalid feature index"));
    fake_free(f);
    return 0;
}

static int test_request_error_codes(int seed, int i) {
    (void)seed, (void)i;
    static const struct {
        uint8_t code;
        const wchar_t *text;
    } cases[] = {
        { 0x01, L"Unknown error" },
        { 0x02, L"Invalid argument" },
        { 0x03, L"Out of range" },
        { 0x04, L"Hardware error" },
        { 0x05, L"Logitech internal error" },
        { 0x06, L"Invalid feature index" },
        { 0x07, L"Invalid function ID" },
        { 0x08, L"Device busy" },
        { 0x09, L"Unsupported" },
        { 0x42, L"Unrecognized error code" },
    };

    fake_t *f = fake_new_bare();
    int idx = fake_add_feature(f, 0x7FFF, 0, error_handler);

    for (size_t n = 0; n < sizeof(cases) / sizeof(cases[0]); n++) {
        hidpp_packet_t req, res;
        T_OK(hidpp_make(&req, f->index, (uint8_t)idx, 0x0E, NULL, 0));
        req.params[0] = cases[n].code;
        T_ERR(hidpp_request(f->rcv, &req, &res), HIDPP_EIO);
        T_NOT_NULL(wcsstr(hidpp_error(f->rcv), cases[n].text));
    }
    fake_free(f);
    return 0;
}

static int test_request_error_is_matched_to_request(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();

    /* an error report for a different function must be ignored */
    const uint8_t stale[7] = { 0x10, 1, 0xFF, 0x00, 0x0E, 0x02, 0x00 };
    fake_queue_raw(f, stale, sizeof(stale));

    hidpp_packet_t req = ping(f, 9), res;
    T_OK(hidpp_request(f->rcv, &req, &res));
    T_EQ(res.params[2], 9);
    fake_free(f);
    return 0;
}

static int test_request_error_from_other_device_ignored(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();

    /* error report for exactly our request, but from device index 3 */
    const uint8_t other[7] = { 0x10, 3, 0xFF, 0x00, 0x1E, 0x02, 0x00 };
    fake_queue_raw(f, other, sizeof(other));

    hidpp_packet_t req = ping(f, 9), res;
    T_OK(hidpp_request(f->rcv, &req, &res));
    fake_free(f);
    return 0;
}

static int test_request_hidpp10_error(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    f->hidpp10 = 1;
    hidpp_packet_t req = ping(f, 1), res;
    T_ERR(hidpp_request(f->rcv, &req, &res), HIDPP_EIO);
    T_NOT_NULL(wcsstr(hidpp_error(f->rcv), L"1.0"));
    fake_free(f);
    return 0;
}

static int test_request_after_error_still_works(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    hidpp_packet_t bad, req = ping(f, 3), res;
    T_OK(hidpp_make(&bad, f->index, 0xEE, 0x1E, NULL, 0));
    T_ERR(hidpp_request(f->rcv, &bad, &res), HIDPP_EIO);
    T_OK(hidpp_request(f->rcv, &req, &res));
    fake_free(f);
    return 0;
}

static int test_request_send_failure(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    f->failSend = 1;
    hidpp_packet_t req = ping(f, 1), res;
    T_ERR(hidpp_request(f->rcv, &req, &res), HIDPP_EIO);
    fake_free(f);
    return 0;
}

static int test_request_recv_failure(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    f->failRecv = 1;
    hidpp_packet_t req = ping(f, 1), res;
    T_ERR(hidpp_request(f->rcv, &req, &res), HIDPP_EIO);
    fake_free(f);
    return 0;
}

/* ------------------------------- swid -------------------------------- */

static int test_default_swid(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    hidpp_packet_t req = ping(f, 1), res;
    T_OK(hidpp_request(f->rcv, &req, &res));
    T_EQ(HIDPP_LSN(fake_last(f)->raw[3]), 14);
    fake_free(f);
    return 0;
}

static int test_set_swid_valid_range(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();

    for (int swid = 1; swid <= 15; swid++)
        T_OK(hidpp_set_swid(f->rcv, (uint8_t)swid));
    fake_free(f);
    return 0;
}

static int test_set_swid_invalid(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    T_ERR(hidpp_set_swid(f->rcv, 0), HIDPP_EINVAL); /* reserved: events */
    T_ERR(hidpp_set_swid(f->rcv, 16), HIDPP_EINVAL);
    T_ERR(hidpp_set_swid(f->rcv, 255), HIDPP_EINVAL);
    T_ERR(hidpp_set_swid(NULL, 3), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

static int test_set_swid_is_used_by_devices(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new(); /* all features, fully populated */
    T_OK(hidpp_set_swid(f->rcv, 5));
    hidpp_device_t *dev = fake_open(f);
    T_NOT_NULL(dev);

    for (unsigned n = 0; n < f->requests; n++)
        T_EQ(HIDPP_LSN(fake_logged(f, n)->raw[3]), 5);
    T_TRUE(f->requests > 0);
    fake_free(f);
    return 0;
}

static int test_failed_set_swid_keeps_old(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    T_OK(hidpp_set_swid(f->rcv, 9));
    T_ERR(hidpp_set_swid(f->rcv, 0), HIDPP_EINVAL);
    hidpp_packet_t req, res;
    T_OK(hidpp_make(&req, f->index, 0, HIDPP_BYTE(1, 9), NULL, 0));
    T_OK(hidpp_request(f->rcv, &req, &res));
    fake_free(f);
    return 0;
}

static const pf_test_t receiver_tests[] = {
    { test_open_custom_requires_callbacks, "open_custom_requires_callbacks" },
    { test_open_custom_and_close, "open_custom_and_close" },
    { test_close_null_is_noop, "close_null_is_noop" },
    { test_works_without_init, "works_without_init" },
    { test_user_pointer_reaches_callbacks, "user_pointer_reaches_callbacks" },
    { test_info_not_supported_for_custom, "info_not_supported_for_custom" },
    { test_nonblocking_not_supported_for_custom,
      "nonblocking_not_supported_for_custom" },
    { test_set_timeout, "set_timeout" },
    { test_no_error_message_initially, "no_error_message_initially" },
    { test_send_short_report, "send_short_report" },
    { test_send_long_report, "send_long_report" },
    { test_send_xlong_report, "send_xlong_report" },
    { test_send_short_does_not_leak_params, "send_short_does_not_leak_params" },
    { test_send_invalid_kind, "send_invalid_kind" },
    { test_send_null_arguments, "send_null_arguments" },
    { test_send_failure_reports_eio, "send_failure_reports_eio" },
    { test_send_uses_request_device_index, "send_uses_request_device_index" },
    { test_receive_parses_long_report, "receive_parses_long_report" },
    { test_receive_short_report_is_zero_padded,
      "receive_short_report_is_zero_padded" },
    { test_receive_xlong_report, "receive_xlong_report" },
    { test_receive_in_order, "receive_in_order" },
    { test_receive_timeout, "receive_timeout" },
    { test_receive_callback_failure, "receive_callback_failure" },
    { test_receive_truncated_report, "receive_truncated_report" },
    { test_receive_null_arguments, "receive_null_arguments" },
    { test_request_roundtrip, "request_roundtrip" },
    { test_request_null_arguments, "request_null_arguments" },
    { test_request_many_in_a_row, "request_many_in_a_row" },
    { test_request_timeout_without_answer, "request_timeout_without_answer" },
    { test_request_dropped_then_ok, "request_dropped_then_ok" },
    { test_request_gives_up_after_unrelated_reports,
      "request_gives_up_after_unrelated_reports" },
    { test_request_discards_stale_input_before_response,
      "request_discards_stale_input" },
    { test_request_skips_notification, "request_skips_notification" },
    { test_request_skips_other_device, "request_skips_other_device" },
    { test_request_skips_other_swid, "request_skips_other_swid" },
    { test_request_skips_other_function, "request_skips_other_function" },
    { test_request_skips_foreign_report, "request_skips_foreign_report" },
    { test_request_skips_all_noise_combined, "request_skips_all_noise" },
    { test_request_rejects_foreign_swid_response,
      "request_rejects_foreign_swid" },
    { test_request_error_report, "request_error_report" },
    { test_request_error_report_long_kind, "request_error_report_long_kind" },
    { test_request_error_codes, "request_error_codes" },
    { test_request_error_is_matched_to_request,
      "request_error_matched_to_request" },
    { test_request_error_from_other_device_ignored,
      "request_error_other_device_ignored" },
    { test_request_hidpp10_error, "request_hidpp10_error" },
    { test_request_after_error_still_works, "request_after_error_still_works" },
    { test_request_send_failure, "request_send_failure" },
    { test_request_recv_failure, "request_recv_failure" },
    { test_default_swid, "default_swid" },
    { test_set_swid_valid_range, "set_swid_valid_range" },
    { test_set_swid_invalid, "set_swid_invalid" },
    { test_set_swid_is_used_by_devices, "set_swid_is_used_by_devices" },
    { test_failed_set_swid_keeps_old, "failed_set_swid_keeps_old" },
    { 0 },
};

const pf_suite_t suite_receiver = { "receiver", 1, receiver_tests };
