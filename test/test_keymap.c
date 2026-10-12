/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

/*  Reprogrammable controls (0x1b04 / 0x1b00). */

#include "test.h"

#define LEFT 0x0050
#define RIGHT 0x0051
#define MIDDLE 0x0052
#define BACK 0x0053
#define GESTURE 0x00C3

static hidpp_keymap_t *open_keymap(fake_t *f) {
    hidpp_device_t *dev = t_open(f);
    hidpp_keymap_t *map = hidpp_keymap(dev);
    T_NOT_NULL(map);
    return map;
}

/* the wire format of the last request with `func` addressed at the keymap */
static const struct fake_packet *last_with_func(const fake_t *f, uint8_t func) {
    for (unsigned n = f->requests; n > 0; n--) {
        const struct fake_packet *p = fake_logged(f, n - 1);
        if (p->raw[2] == FAKE_IDX_KEYMAP && HIDPP_MSN(p->raw[3]) == func)
            return p;
    }
    return NULL;
}

static int fail_state_hook(
    hidpp_receiver_t *rcv, hidpp_packet_t *req, hidpp_packet_t *res, void *user
) {
    (void)rcv, (void)user;
    if (res && req && req->feat == FAKE_IDX_KEYMAP && HIDPP_MSN(req->func) == 2)
        return HIDPP_EIO;
    return 0;
}

/* ------------------------------ opening ------------------------------ */

static int test_open(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    open_keymap(f);
    fake_free(f);
    return 0;
}

static int test_same_object_every_time(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    hidpp_keymap_t *a = hidpp_keymap(dev);
    T_NOT_NULL(a);
    T_TRUE(a == hidpp_keymap(dev));
    fake_free(f);
    return 0;
}

static int test_legacy_feature_id(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    fake_add_keymap(f, 0x1B00);
    hidpp_keymap_t *map = open_keymap(f);

    uint16_t ids[8];
    T_EQ(hidpp_keymap_list(map, ids, 8), 5);
    T_EQ(ids[0], LEFT);
    fake_free(f);
    return 0;
}

static int test_prefers_newer_feature(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    fake_add_keymap(f, 0x1B00);
    f->keymapCount = 0;         /* the legacy feature is empty ... */
    fake_add_keymap(f, 0x1B04); /* ... the newer one has five controls */
    hidpp_keymap_t *map = open_keymap(f);

    uint16_t ids[8];
    T_EQ(hidpp_keymap_list(map, ids, 8), 5);
    fake_free(f);
    return 0;
}

static int test_unsupported_feature(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    hidpp_device_t *dev = t_open(f);

    for (int n = 0; n < 3; n++)
        T_NULL(hidpp_keymap(dev));
    fake_free(f);
    return 0;
}

static int test_null_device(int seed, int i) {
    (void)seed, (void)i;
    T_NULL(hidpp_keymap(NULL));
    return 0;
}

static int test_null_map(int seed, int i) {
    (void)seed, (void)i;
    uint16_t ids[2];
    struct hidpp_keymap_state st;
    T_ERR(hidpp_keymap_index(NULL, LEFT), HIDPP_EINVAL);
    T_ERR(hidpp_keymap_id(NULL, 0), HIDPP_EINVAL);
    T_EQ(hidpp_keymap_list(NULL, ids, 2), 0);
    T_NULL(hidpp_keymap_info(NULL, LEFT));
    T_ERR(hidpp_keymap_divert(NULL, LEFT, 1), HIDPP_EINVAL);
    T_ERR(hidpp_keymap_remap(NULL, LEFT, RIGHT), HIDPP_EINVAL);
    T_ERR(hidpp_keymap_state(NULL, LEFT, &st), HIDPP_EINVAL);
    return 0;
}

/* ----------------------------- enumeration --------------------------- */

static int test_list(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_keymap_t *map = open_keymap(f);

    uint16_t ids[16];
    memset(ids, 0xEE, sizeof(ids));
    T_EQ(hidpp_keymap_list(map, ids, 16), 5);
    T_EQ(ids[0], LEFT);
    T_EQ(ids[1], RIGHT);
    T_EQ(ids[2], MIDDLE);
    T_EQ(ids[3], BACK);
    T_EQ(ids[4], GESTURE);
    T_EQ(ids[5], 0xEEEE);
    fake_free(f);
    return 0;
}

static int test_list_truncated(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_keymap_t *map = open_keymap(f);

    uint16_t ids[8];
    memset(ids, 0xEE, sizeof(ids));
    T_EQ(hidpp_keymap_list(map, ids, 2), 2);
    T_EQ(ids[1], RIGHT);
    T_EQ(ids[2], 0xEEEE);
    T_EQ(hidpp_keymap_list(map, ids, 0), 0);
    T_EQ(hidpp_keymap_list(map, NULL, 4), 0);
    fake_free(f);
    return 0;
}

static int test_index_and_id(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_keymap_t *map = open_keymap(f);

    T_EQ(hidpp_keymap_index(map, LEFT), 0);
    T_EQ(hidpp_keymap_index(map, GESTURE), 4);
    T_ERR(hidpp_keymap_index(map, 0x7777), HIDPP_ENOENT);

    T_EQ(hidpp_keymap_id(map, 0), LEFT);
    T_EQ(hidpp_keymap_id(map, 4), GESTURE);
    T_ERR(hidpp_keymap_id(map, 5), HIDPP_ENOENT);
    T_ERR(hidpp_keymap_id(map, 255), HIDPP_ENOENT);

    for (int n = 0; n < 5; n++)
        T_EQ(hidpp_keymap_index(map, (uint16_t)hidpp_keymap_id(map, n)), n);
    fake_free(f);
    return 0;
}

static int test_no_controls(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    fake_add_keymap(f, 0x1B04);
    f->keymapCount = 0;
    hidpp_keymap_t *map = open_keymap(f);

    uint16_t ids[4];
    T_EQ(hidpp_keymap_list(map, ids, 4), 0);
    T_NULL(hidpp_keymap_info(map, LEFT));
    T_ERR(hidpp_keymap_id(map, 0), HIDPP_ENOENT);
    fake_free(f);
    return 0;
}

static int test_many_controls(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    fake_add_keymap(f, 0x1B04);
    f->keymapCount = 0;

    for (int n = 0; n < FAKE_MAX_CONTROLS; n++)
        fake_add_control(
            f, (uint16_t)(0x0100 + n), (uint16_t)n, 0x31, (uint8_t)n
        );

    hidpp_keymap_t *map = open_keymap(f);
    uint16_t ids[FAKE_MAX_CONTROLS + 8];
    T_EQ(hidpp_keymap_list(map, ids, FAKE_MAX_CONTROLS + 8), FAKE_MAX_CONTROLS);

    for (int n = 0; n < FAKE_MAX_CONTROLS; n++) {
        T_EQ(ids[n], 0x0100 + n);
        struct hidpp_keymap_info *info = hidpp_keymap_info(map, 0x0100 + n);
        T_NOT_NULL(info);
        T_EQ(info->index, n);
        T_EQ(info->position, n);
    }
    fake_free(f);
    return 0;
}

static int test_enumeration_is_cached(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_keymap_t *map = open_keymap(f);
    fake_reset_log(f);

    uint16_t ids[8];
    for (int n = 0; n < 5; n++) {
        T_EQ(hidpp_keymap_list(map, ids, 8), 5);
        T_NOT_NULL(hidpp_keymap_info(map, LEFT));
        T_EQ(hidpp_keymap_index(map, BACK), 3);
        T_EQ(hidpp_keymap_id(map, 2), MIDDLE);
    }
    T_EQ(f->requests, 0);
    fake_free(f);
    return 0;
}

/* -------------------------------- info ------------------------------- */

static int test_info(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_keymap_t *map = open_keymap(f);

    struct hidpp_keymap_info *info = hidpp_keymap_info(map, LEFT);
    T_NOT_NULL(info);
    T_TRUE(info->initialized);
    T_EQ(info->id, LEFT);
    T_EQ(info->index, 0);
    T_EQ(info->taskId, 0x0038);
    T_EQ(info->flags, 0x31);
    T_EQ(info->position, 0);
    T_EQ(info->group, 0);
    T_EQ(info->groupMask, 0);
    T_EQ(info->rawXY, 0);
    T_STR(info->name, "Left_Button");
    T_NULL(info->remapName);
    fake_free(f);
    return 0;
}

static int test_info_group_fields(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_keymap_t *map = open_keymap(f);

    struct hidpp_keymap_info *info = hidpp_keymap_info(map, GESTURE);
    T_NOT_NULL(info);
    T_EQ(info->index, 4);
    T_EQ(info->taskId, 0x00B4);
    T_EQ(info->position, 4);
    T_EQ(info->group, 1);
    T_EQ(info->groupMask, 0x03);
    T_EQ(info->rawXY, 1);
    T_STR(info->name, "Mouse_Gesture_Button");
    fake_free(f);
    return 0;
}

static int test_info_unknown_control_name(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new_bare();
    fake_add_keymap(f, 0x1B04);
    fake_add_control(f, 0x7A7A, 1, 0, 9);
    hidpp_keymap_t *map = open_keymap(f);
    struct hidpp_keymap_info *info = hidpp_keymap_info(map, 0x7A7A);
    T_NOT_NULL(info);
    T_STR(info->name, "Unknown HID++ button");
    fake_free(f);
    return 0;
}

static int test_info_flag_decoding(int seed, int i) {
    (void)seed, (void)i;

    for (int bit = 0; bit < 8; bit++) {
        fake_t *f = fake_new();
        f->controls[0].flags = (uint8_t)(1u << bit);
        hidpp_keymap_t *map = open_keymap(f);
        struct hidpp_keymap_info *info = hidpp_keymap_info(map, LEFT);
        T_NOT_NULL(info);

        int actual[8] = { info->isMouseButton,    info->isFunctionKey,
                          info->isHotkey,         info->isFnTogglable,
                          info->isReprogrammable, info->isDivertable,
                          info->isPersistable,    info->isVirtual };

        for (int k = 0; k < 8; k++)
            T_EQ(actual[k], k == bit);
        fake_free(f);
    }
    return 0;
}

static int test_info_all_flags(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    f->controls[0].flags = 0xFF;
    hidpp_keymap_t *map = open_keymap(f);
    struct hidpp_keymap_info *info = hidpp_keymap_info(map, LEFT);
    T_NOT_NULL(info);
    T_TRUE(info->isMouseButton && info->isFunctionKey && info->isHotkey);
    T_TRUE(info->isFnTogglable && info->isReprogrammable);
    T_TRUE(info->isDivertable && info->isPersistable && info->isVirtual);
    fake_free(f);
    return 0;
}

static int test_info_unknown_id(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_keymap_t *map = open_keymap(f);
    T_NULL(hidpp_keymap_info(map, 0x7777));
    T_NULL(hidpp_keymap_info(map, 0));
    fake_free(f);
    return 0;
}

static int test_info_pointer_is_stable(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_keymap_t *map = open_keymap(f);
    T_TRUE(hidpp_keymap_info(map, RIGHT) == hidpp_keymap_info(map, RIGHT));
    fake_free(f);
    return 0;
}

static int test_info_request_per_control(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    fake_reset_log(f);
    T_NOT_NULL(hidpp_keymap(dev));

    /* count + one information request for each of the five controls */
    T_TRUE(fake_calls(f, FAKE_IDX_KEYMAP) == 6);
    for (uint8_t n = 0; n < 5; n++) {
        const struct fake_packet *p = NULL;
        unsigned seen = 0;
        for (unsigned r = 0; r < f->requests; r++) {
            p = fake_logged(f, r);
            if (p->raw[2] == FAKE_IDX_KEYMAP && HIDPP_MSN(p->raw[3]) == 1
                && p->raw[4] == n)
                seen++;
        }
        T_EQ(seen, 1);
    }
    fake_free(f);
    return 0;
}

/* ------------------------------- state ------------------------------- */

static int test_state_default(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_keymap_t *map = open_keymap(f);

    struct hidpp_keymap_state st;
    memset(&st, 0xAB, sizeof(st));
    T_OK(hidpp_keymap_state(map, LEFT, &st));
    T_EQ(st.remapId, LEFT);
    T_EQ(st.flags, 0);
    T_FALSE(st.isDiverted);
    T_FALSE(st.isPersistent);
    T_FALSE(st.rawXY);
    fake_free(f);
    return 0;
}

static int test_state_flag_decoding(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    struct fake_control *c = fake_control(f, LEFT);
    c->divert = 1;
    c->persist = 1;
    c->rawXY = 1;
    hidpp_keymap_t *map = open_keymap(f);

    struct hidpp_keymap_state st;
    T_OK(hidpp_keymap_state(map, LEFT, &st));
    T_EQ(st.flags, 0x15);
    T_TRUE(st.isDiverted);
    T_TRUE(st.isPersistent);
    T_TRUE(st.rawXY);
    fake_free(f);
    return 0;
}

static int test_state_each_flag_alone(int seed, int i) {
    (void)seed, (void)i;

    for (int which = 0; which < 3; which++) {
        fake_t *f = fake_new();
        struct fake_control *c = fake_control(f, LEFT);
        c->divert = which == 0;
        c->persist = which == 1;
        c->rawXY = which == 2;
        hidpp_keymap_t *map = open_keymap(f);

        struct hidpp_keymap_state st;
        T_OK(hidpp_keymap_state(map, LEFT, &st));
        T_EQ(st.isDiverted, which == 0);
        T_EQ(st.isPersistent, which == 1);
        T_EQ(st.rawXY, which == 2);
        fake_free(f);
    }
    return 0;
}

static int test_state_asks_for_the_requested_control(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    fake_control(f, RIGHT)->divert = 1;
    fake_control(f, BACK)->remap = LEFT;
    hidpp_keymap_t *map = open_keymap(f);

    struct hidpp_keymap_state st;
    T_OK(hidpp_keymap_state(map, LEFT, &st));
    T_FALSE(st.isDiverted);
    T_OK(hidpp_keymap_state(map, RIGHT, &st));
    T_TRUE(st.isDiverted);
    T_OK(hidpp_keymap_state(map, BACK, &st));
    T_EQ(st.remapId, LEFT);

    /* and the identifier was really transmitted */
    const struct fake_packet *p = last_with_func(f, 2);
    T_NOT_NULL(p);
    T_EQ(HIDPP_WORD(p->raw[4], p->raw[5]), BACK);
    fake_free(f);
    return 0;
}

static int test_state_unknown_control(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_keymap_t *map = open_keymap(f);
    struct hidpp_keymap_state st;
    T_TRUE(hidpp_keymap_state(map, 0x7777, &st) < 0);
    fake_free(f);
    return 0;
}

static int test_state_null_output(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_keymap_t *map = open_keymap(f);
    T_ERR(hidpp_keymap_state(map, LEFT, NULL), HIDPP_EINVAL);
    fake_free(f);
    return 0;
}

static int test_state_is_never_cached(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_keymap_t *map = open_keymap(f);
    struct hidpp_keymap_state st;

    T_OK(hidpp_keymap_state(map, LEFT, &st));
    T_FALSE(st.isDiverted);
    fake_control(f, LEFT)->divert = 1;
    T_OK(hidpp_keymap_state(map, LEFT, &st));
    T_TRUE(st.isDiverted);
    fake_free(f);
    return 0;
}

static int test_state_device_failure(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_keymap_t *map = open_keymap(f);
    hidpp_set_protocol_hook(f->rcv, fail_state_hook, NULL);
    struct hidpp_keymap_state st;
    T_ERR(hidpp_keymap_state(map, LEFT, &st), HIDPP_EIO);
    fake_free(f);
    return 0;
}

/* ------------------------------- divert ------------------------------ */

static int test_divert_on_and_off(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_keymap_t *map = open_keymap(f);
    struct hidpp_keymap_state st;

    T_OK(hidpp_keymap_divert(map, LEFT, 1));
    T_EQ(fake_control(f, LEFT)->divert, 1);
    T_OK(hidpp_keymap_state(map, LEFT, &st));
    T_TRUE(st.isDiverted);

    /* turning it off needs the "valid" bit as well, or it is ignored */
    T_OK(hidpp_keymap_divert(map, LEFT, 0));
    T_EQ(fake_control(f, LEFT)->divert, 0);
    T_OK(hidpp_keymap_state(map, LEFT, &st));
    T_FALSE(st.isDiverted);
    fake_free(f);
    return 0;
}

static int test_divert_wire_format(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_keymap_t *map = open_keymap(f);

    T_OK(hidpp_keymap_divert(map, RIGHT, 1));
    const struct fake_packet *p = last_with_func(f, 3);
    T_NOT_NULL(p);
    T_EQ(HIDPP_WORD(p->raw[4], p->raw[5]), RIGHT);
    T_EQ(p->raw[6] & 0x03, 0x03); /* divert + divert-valid */

    T_OK(hidpp_keymap_divert(map, RIGHT, 0));
    p = last_with_func(f, 3);
    T_EQ(p->raw[6] & 0x03, 0x02); /* only divert-valid */
    fake_free(f);
    return 0;
}

static int test_divert_touches_only_that_control(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_keymap_t *map = open_keymap(f);
    T_OK(hidpp_keymap_divert(map, MIDDLE, 1));

    for (unsigned n = 0; n < f->keymapCount; n++)
        T_EQ(f->controls[n].divert, f->controls[n].cid == MIDDLE);
    fake_free(f);
    return 0;
}

static int test_divert_keeps_other_settings(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    struct fake_control *c = fake_control(f, GESTURE);
    c->persist = 1;
    c->rawXY = 1;
    hidpp_keymap_t *map = open_keymap(f);

    T_OK(hidpp_keymap_divert(map, GESTURE, 1));
    T_OK(hidpp_keymap_divert(map, GESTURE, 0));
    T_EQ(c->persist, 1);
    T_EQ(c->rawXY, 1);
    fake_free(f);
    return 0;
}

static int test_divert_repeatedly(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_keymap_t *map = open_keymap(f);

    for (int n = 0; n < 50; n++) {
        T_OK(hidpp_keymap_divert(map, LEFT, n & 1));
        T_EQ(fake_control(f, LEFT)->divert, n & 1);
    }
    fake_free(f);
    return 0;
}

static int test_divert_waits_for_the_answer(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_keymap_t *map = open_keymap(f);
    T_OK(hidpp_keymap_divert(map, LEFT, 1));
    /* an unread answer would be mistaken for the next request's reply */
    T_EQ(fake_pending(f), 0);
    fake_free(f);
    return 0;
}

static int test_divert_unknown_control(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_keymap_t *map = open_keymap(f);
    T_TRUE(hidpp_keymap_divert(map, 0x7777, 1) < 0);
    fake_free(f);
    return 0;
}

static int test_divert_device_silent(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_keymap_t *map = open_keymap(f);
    f->silent = 1;
    T_TRUE(hidpp_keymap_divert(map, LEFT, 1) < 0);
    fake_free(f);
    return 0;
}

/* ------------------------------- remap ------------------------------- */

static int test_remap(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_keymap_t *map = open_keymap(f);

    T_OK(hidpp_keymap_remap(map, LEFT, RIGHT));
    T_EQ(fake_control(f, LEFT)->remap, RIGHT);

    struct hidpp_keymap_state st;
    T_OK(hidpp_keymap_state(map, LEFT, &st));
    T_EQ(st.remapId, RIGHT);
    fake_free(f);
    return 0;
}

static int test_remap_uses_a_long_report(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_keymap_t *map = open_keymap(f);
    T_OK(hidpp_keymap_remap(map, LEFT, 0x1234));

    /* five parameter bytes do not fit in a short report */
    const struct fake_packet *p = last_with_func(f, 3);
    T_NOT_NULL(p);
    T_TRUE(p->raw[0] != HIDPP_KIND_SHORT);
    T_EQ(HIDPP_WORD(p->raw[4], p->raw[5]), LEFT);
    T_EQ(HIDPP_WORD(p->raw[7], p->raw[8]), 0x1234);
    fake_free(f);
    return 0;
}

static int test_remap_leaves_flags_alone(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_keymap_t *map = open_keymap(f);

    T_OK(hidpp_keymap_divert(map, LEFT, 1));
    fake_control(f, LEFT)->rawXY = 1;
    T_OK(hidpp_keymap_remap(map, LEFT, MIDDLE));

    struct fake_control *c = fake_control(f, LEFT);
    T_EQ(c->divert, 1);
    T_EQ(c->rawXY, 1);
    T_EQ(c->remap, MIDDLE);
    T_EQ(last_with_func(f, 3)->raw[6], 0); /* no *-valid bit set */
    fake_free(f);
    return 0;
}

static int test_remap_back_to_itself(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_keymap_t *map = open_keymap(f);
    T_OK(hidpp_keymap_remap(map, LEFT, RIGHT));
    T_OK(hidpp_keymap_remap(map, LEFT, LEFT));
    T_EQ(fake_control(f, LEFT)->remap, LEFT);
    fake_free(f);
    return 0;
}

static int test_remap_only_one_control(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_keymap_t *map = open_keymap(f);
    T_OK(hidpp_keymap_remap(map, BACK, LEFT));

    for (unsigned n = 0; n < f->keymapCount; n++)
        T_EQ(
            f->controls[n].remap,
            f->controls[n].cid == BACK ? LEFT : f->controls[n].cid
        );
    fake_free(f);
    return 0;
}

static int test_remap_by_name(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_keymap_t *map = open_keymap(f);

    int from = hidpp_keymap_from_name("Back_Button");
    int to = hidpp_keymap_from_name("Middle_Button");
    T_EQ(from, BACK);
    T_OK(hidpp_keymap_remap(map, (uint16_t)from, (uint16_t)to));
    T_EQ(fake_control(f, BACK)->remap, MIDDLE);
    fake_free(f);
    return 0;
}

static int test_remap_unknown_control(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_keymap_t *map = open_keymap(f);
    T_TRUE(hidpp_keymap_remap(map, 0x7777, LEFT) < 0);
    fake_free(f);
    return 0;
}

static int test_remap_waits_for_the_answer(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_keymap_t *map = open_keymap(f);
    T_OK(hidpp_keymap_remap(map, LEFT, RIGHT));
    T_EQ(fake_pending(f), 0);
    fake_free(f);
    return 0;
}

/* ------------------------------- cache ------------------------------- */

static int test_cache_clear_rereads_controls(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    hidpp_keymap_t *map = hidpp_keymap(dev);
    T_NOT_NULL(map);
    uint16_t ids[16];
    T_EQ(hidpp_keymap_list(map, ids, 16), 5);

    fake_add_control(f, 0x0056, 0x003D, 0x31, 5);
    hidpp_cache_clear(dev);

    map = hidpp_keymap(dev);
    T_NOT_NULL(map);
    T_EQ(hidpp_keymap_list(map, ids, 16), 6);
    T_EQ(ids[5], 0x0056);
    fake_free(f);
    return 0;
}

static int test_first_use_after_failed_enumeration(int seed, int i) {
    (void)seed, (void)i;
    fake_t *f = fake_new();
    hidpp_device_t *dev = t_open(f);
    f->silent = 1; /* the device vanishes before the keymap is first used */

    hidpp_keymap_t *map = hidpp_keymap(dev);
    if (map) {
        uint16_t ids[8];
        T_EQ(hidpp_keymap_list(map, ids, 8), 0);
    }
    fake_free(f);
    return 0;
}

static const pf_test_t keymap_tests[] = {
    { test_open, "open" },
    { test_same_object_every_time, "same_object_every_time" },
    { test_legacy_feature_id, "legacy_feature_id" },
    { test_prefers_newer_feature, "prefers_newer_feature" },
    { test_unsupported_feature, "unsupported_feature" },
    { test_null_device, "null_device" },
    { test_null_map, "null_map" },
    { test_list, "list" },
    { test_list_truncated, "list_truncated" },
    { test_index_and_id, "index_and_id" },
    { test_no_controls, "no_controls" },
    { test_many_controls, "many_controls" },
    { test_enumeration_is_cached, "enumeration_is_cached" },
    { test_info, "info" },
    { test_info_group_fields, "info_group_fields" },
    { test_info_unknown_control_name, "info_unknown_control_name" },
    { test_info_flag_decoding, "info_flag_decoding" },
    { test_info_all_flags, "info_all_flags" },
    { test_info_unknown_id, "info_unknown_id" },
    { test_info_pointer_is_stable, "info_pointer_is_stable" },
    { test_info_request_per_control, "info_request_per_control" },
    { test_state_default, "state_default" },
    { test_state_flag_decoding, "state_flag_decoding" },
    { test_state_each_flag_alone, "state_each_flag_alone" },
    { test_state_asks_for_the_requested_control, "state_asks_for_control" },
    { test_state_unknown_control, "state_unknown_control" },
    { test_state_null_output, "state_null_output" },
    { test_state_is_never_cached, "state_is_never_cached" },
    { test_state_device_failure, "state_device_failure" },
    { test_divert_on_and_off, "divert_on_and_off" },
    { test_divert_wire_format, "divert_wire_format" },
    { test_divert_touches_only_that_control, "divert_only_that_control" },
    { test_divert_keeps_other_settings, "divert_keeps_other_settings" },
    { test_divert_repeatedly, "divert_repeatedly" },
    { test_divert_waits_for_the_answer, "divert_waits_for_answer" },
    { test_divert_unknown_control, "divert_unknown_control" },
    { test_divert_device_silent, "divert_device_silent" },
    { test_remap, "remap" },
    { test_remap_uses_a_long_report, "remap_uses_long_report" },
    { test_remap_leaves_flags_alone, "remap_leaves_flags_alone" },
    { test_remap_back_to_itself, "remap_back_to_itself" },
    { test_remap_only_one_control, "remap_only_one_control" },
    { test_remap_by_name, "remap_by_name" },
    { test_remap_unknown_control, "remap_unknown_control" },
    { test_remap_waits_for_the_answer, "remap_waits_for_answer" },
    { test_cache_clear_rereads_controls, "cache_clear_rereads" },
    { test_first_use_after_failed_enumeration, "failed_enumeration" },
    { 0 },
};

const pf_suite_t suite_keymap = { "keymap", 1, keymap_tests };
