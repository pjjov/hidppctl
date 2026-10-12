/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

/*  The protocol hook: observing, modifying and blocking traffic. */

#include "test.h"

struct hook_state {
    int sends;
    int receives;
    int sawRequestOnReceive;
    void *seenUser;
    int sendResult;
    int receiveResult;
    uint8_t patchSend;
    uint8_t patchReceive;
};

static int counting_hook(
    hidpp_receiver_t *rcv, hidpp_packet_t *req, hidpp_packet_t *res, void *user
) {
    (void)rcv;
    struct hook_state *s = user;
    s->seenUser = user;

    if (!res) {
        T_NOT_NULL(req);
        s->sends++;
        if (s->patchSend)
            req->params[2] = s->patchSend;
        return s->sendResult;
    }

    s->receives++;
    if (req)
        s->sawRequestOnReceive++;
    if (s->patchReceive)
        res->params[2] = s->patchReceive;
    return s->receiveResult;
}

static hidpp_packet_t make_ping(fake_t *f, uint8_t data) {
    hidpp_packet_t req;
    memset(&req, 0, sizeof(req));
    T_OK(hidpp_make(&req, f->index, 0, HIDPP_BYTE(1, 14), NULL, 0));
    req.params[2] = data;
    return req;
}

static int test_set_hook_returns_previous(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    struct hook_state s = { 0 };

    T_NULL(hidpp_set_protocol_hook(f->rcv, counting_hook, &s));
    T_TRUE(hidpp_set_protocol_hook(f->rcv, NULL, NULL) == counting_hook);
    T_NULL(hidpp_set_protocol_hook(f->rcv, NULL, NULL));
    fake_free(f);
    return 0;
}

static int test_set_hook_null_receiver(int seed, int i) {
    (void)seed, (void)i;
    struct hook_state s = { 0 };
    T_NULL(hidpp_set_protocol_hook(NULL, counting_hook, &s));
    return 0;
}

static int test_hook_sees_request_and_response(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    struct hook_state s = { 0 };
    hidpp_set_protocol_hook(f->rcv, counting_hook, &s);

    hidpp_packet_t req = make_ping(f, 1), res;
    T_OK(hidpp_request(f->rcv, &req, &res));
    T_EQ(s.sends, 1);
    T_EQ(s.receives, 1);
    /* while a request is pending its packet is given to the hook */
    T_EQ(s.sawRequestOnReceive, 1);
    fake_free(f);
    return 0;
}

static int test_hook_gets_user_pointer(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    struct hook_state s = { 0 };
    hidpp_set_protocol_hook(f->rcv, counting_hook, &s);

    hidpp_packet_t req = make_ping(f, 1), res;
    T_OK(hidpp_request(f->rcv, &req, &res));
    T_TRUE(s.seenUser == &s);
    fake_free(f);
    return 0;
}

static int test_hook_skip_io_does_not_send(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    struct hook_state s = { .sendResult = HIDPP_HOOK_SKIP_IO };
    hidpp_set_protocol_hook(f->rcv, counting_hook, &s);

    hidpp_packet_t req = make_ping(f, 1);
    T_OK(hidpp_send(f->rcv, &req)); /* skipping is not an error */
    T_EQ(f->requests, 0);
    T_EQ(s.sends, 1);
    fake_free(f);
    return 0;
}

static int test_hook_skip_io_then_request_times_out(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    struct hook_state s = { .sendResult = HIDPP_HOOK_SKIP_IO };
    hidpp_set_protocol_hook(f->rcv, counting_hook, &s);

    hidpp_packet_t req = make_ping(f, 1), res;
    T_ERR(hidpp_request(f->rcv, &req, &res), HIDPP_ETIMEDOUT);
    T_EQ(f->requests, 0);
    fake_free(f);
    return 0;
}

static int test_hook_error_aborts_send(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    struct hook_state s = { .sendResult = HIDPP_EINVAL };
    hidpp_set_protocol_hook(f->rcv, counting_hook, &s);

    hidpp_packet_t req = make_ping(f, 1);
    T_ERR(hidpp_send(f->rcv, &req), HIDPP_EINVAL);
    T_EQ(f->requests, 0);
    fake_free(f);
    return 0;
}

static int test_hook_can_modify_outgoing_packet(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    struct hook_state s = { .patchSend = 0x55 };
    hidpp_set_protocol_hook(f->rcv, counting_hook, &s);

    hidpp_packet_t req = make_ping(f, 0x11), res;
    T_OK(hidpp_request(f->rcv, &req, &res));
    T_EQ(fake_last(f)->raw[6], 0x55); /* what hit the wire */
    T_EQ(res.params[2], 0x55);        /* and what the device echoed */
    fake_free(f);
    return 0;
}

static int test_hook_can_modify_incoming_packet(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    struct hook_state s = { .patchReceive = 0x77 };
    hidpp_set_protocol_hook(f->rcv, counting_hook, &s);

    hidpp_packet_t req = make_ping(f, 0x11), res;
    T_OK(hidpp_request(f->rcv, &req, &res));
    T_EQ(res.params[2], 0x77);
    fake_free(f);
    return 0;
}

static int test_hook_error_on_receive_fails_request(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    struct hook_state s = { .receiveResult = HIDPP_EIO };
    hidpp_set_protocol_hook(f->rcv, counting_hook, &s);

    hidpp_packet_t req = make_ping(f, 1), res;
    T_ERR(hidpp_request(f->rcv, &req, &res), HIDPP_EIO);
    fake_free(f);
    return 0;
}

static int test_hook_removed(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    struct hook_state s = { 0 };
    hidpp_set_protocol_hook(f->rcv, counting_hook, &s);
    hidpp_set_protocol_hook(f->rcv, NULL, NULL);

    hidpp_packet_t req = make_ping(f, 1), res;
    T_OK(hidpp_request(f->rcv, &req, &res));
    T_EQ(s.sends, 0);
    T_EQ(s.receives, 0);
    fake_free(f);
    return 0;
}

static int test_hook_called_once_per_report(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    f->noise = FAKE_NOISE_NOTIFY | FAKE_NOISE_OTHER_SWID;
    struct hook_state s = { 0 };
    hidpp_set_protocol_hook(f->rcv, counting_hook, &s);

    hidpp_packet_t req = make_ping(f, 1), res;
    T_OK(hidpp_request(f->rcv, &req, &res));
    T_EQ(s.sends, 1);
    T_EQ(s.receives, 3); /* two noise reports and the answer */
    fake_free(f);
    return 0;
}

static int test_hook_on_plain_receive_has_no_request(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    struct hook_state s = { 0 };
    hidpp_set_protocol_hook(f->rcv, counting_hook, &s);

    const uint8_t raw[7] = { 0x10, 1, 3, 0, 0, 0, 0 };
    fake_queue_raw(f, raw, sizeof(raw));
    hidpp_packet_t p;
    T_OK(hidpp_receive(f->rcv, &p));
    T_EQ(s.receives, 1);
    T_EQ(s.sawRequestOnReceive, 0);
    fake_free(f);
    return 0;
}

static int test_hook_applies_to_device_requests(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    struct hook_state s = { 0 };
    hidpp_set_protocol_hook(f->rcv, counting_hook, &s);

    T_NOT_NULL(fake_open(f));
    T_TRUE(s.sends > 0);
    T_EQ((unsigned)s.sends, f->requests);
    fake_free(f);
    return 0;
}

static const pf_test_t hook_tests[] = {
    { test_set_hook_returns_previous, "set_returns_previous" },
    { test_set_hook_null_receiver, "set_null_receiver" },
    { test_hook_sees_request_and_response, "sees_request_and_response" },
    { test_hook_gets_user_pointer, "gets_user_pointer" },
    { test_hook_skip_io_does_not_send, "skip_io_does_not_send" },
    { test_hook_skip_io_then_request_times_out, "skip_io_request_times_out" },
    { test_hook_error_aborts_send, "error_aborts_send" },
    { test_hook_can_modify_outgoing_packet, "can_modify_outgoing" },
    { test_hook_can_modify_incoming_packet, "can_modify_incoming" },
    { test_hook_error_on_receive_fails_request, "error_on_receive" },
    { test_hook_removed, "removed" },
    { test_hook_called_once_per_report, "called_once_per_report" },
    { test_hook_on_plain_receive_has_no_request, "plain_receive_no_request" },
    { test_hook_applies_to_device_requests, "applies_to_device_requests" },
    { 0 },
};

const pf_suite_t suite_hook = { "hook", 1, hook_tests };
