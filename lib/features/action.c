/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

/** Section: HID++ feature documentation

    Persistent remappable action (id: 0x1C00)
    Big endian integers are exchanged.

    -----------------------
    Fn 0 - get feature info
    -----------------------
    request:
    response:
        uint16_t flags
        - bit N - the action with id N + 1 is supported
          (keyboard, mouse button, X, Y, roller, AC pan, consumer,
          internal function, power key)

    -----------------------
    Fn 1 - get count
    -----------------------
    request:
    response:
        uint8_t count
        uint8_t hostCount (0xFF if unknown)

    -----------------------
    Fn 2 - get control id
    -----------------------
    request:
        uint8_t index
        uint8_t host (0xFF is the current host)
    response:
        uint16_t controlId

    -----------------------
    Fn 3 - get action
    -----------------------
    request:
        uint16_t controlId
        uint8_t host
    response:
        uint16_t controlId
        uint8_t host
        uint8_t actionId
        uint16_t value
        uint8_t modifierMask
        uint8_t status
        - bit 0 - remapped

    -----------------------
    Fn 4 - set action
    -----------------------
    request:
        uint16_t controlId
        uint8_t host
        uint8_t actionId
        uint16_t value
        uint8_t modifierMask
    response:

    -----------------------
    Fn 5 - reset action
    -----------------------
    request:
        uint16_t controlId
        uint8_t host
    response:

    -----------------------
    Fn 6 - reset everything
    -----------------------
    request:
        uint8_t hostMask
    response:
*/

#define ACTION_ID 0x1C00

#define MAX_CONTROLS 256
#define UNKNOWN_HOST_COUNT 0xFF
#define MAX_ACTION HIDPP_ACTION_POWER

#define STATUS_REMAPPED_FLAG 0x01

struct hidpp_feat_action {
    struct hidpp_feat_base base;
    struct hidpp_action_info info;
    uint16_t controls[MAX_CONTROLS];
};

const char *hidpp_action_type_name(uint8_t type) {
    switch (type) {
        /* clang-format off */
    case HIDPP_ACTION_NONE:              return "None";
    case HIDPP_ACTION_KEY:               return "Key";
    case HIDPP_ACTION_MOUSE_BUTTON:      return "Mouse button";
    case HIDPP_ACTION_X_DISPLACEMENT:    return "X displacement";
    case HIDPP_ACTION_Y_DISPLACEMENT:    return "Y displacement";
    case HIDPP_ACTION_VERTICAL_ROLLER:   return "Vertical roller";
    case HIDPP_ACTION_HORIZONTAL_ROLLER: return "Horizontal roller";
    case HIDPP_ACTION_CONSUMER:          return "Consumer control";
    case HIDPP_ACTION_INTERNAL:          return "Internal function";
    case HIDPP_ACTION_POWER:             return "Power key";
    default:                             return "Unknown";
        /* clang-format on */
    }
}

/* Capability bit N belongs to the action with id N + 1. */
static int supports_type(const struct hidpp_action_info *info, uint8_t type) {
    if (type == HIDPP_ACTION_NONE)
        return 1;

    return type <= MAX_ACTION && (info->flags & (1u << (type - 1)));
}

static void unpack_flags(struct hidpp_action_info *info) {
    info->canKey = supports_type(info, HIDPP_ACTION_KEY);
    info->canMouseButton = supports_type(info, HIDPP_ACTION_MOUSE_BUTTON);
    info->canXDisplacement = supports_type(info, HIDPP_ACTION_X_DISPLACEMENT);
    info->canYDisplacement = supports_type(info, HIDPP_ACTION_Y_DISPLACEMENT);
    info->canVerticalRoller = supports_type(info, HIDPP_ACTION_VERTICAL_ROLLER);
    info->canHorizontalRoller = supports_type(
        info, HIDPP_ACTION_HORIZONTAL_ROLLER
    );
    info->canConsumer = supports_type(info, HIDPP_ACTION_CONSUMER);
    info->canInternal = supports_type(info, HIDPP_ACTION_INTERNAL);
    info->canPower = supports_type(info, HIDPP_ACTION_POWER);
}

static int query_info(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_action *act = feat;
    struct hidpp_action_info *info = &act->info;
    hidpp_packet_t req, res;

    int rc;

    make_packet(&req, dev, act->base.featIndex, 0);

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return is_transient(dev, rc) ? rc : HIDPP_OK;

    info->flags = HIDPP_WORD(res.params[0], res.params[1]);

    make_packet(&req, dev, act->base.featIndex, 1);

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return is_transient(dev, rc) ? rc : HIDPP_OK;

    info->count = res.params[0];
    info->hostCount = res.params[1];

    /* the table does not depend on the host, so the current one is used */
    make_packet(&req, dev, act->base.featIndex, 2);
    req.params[1] = HIDPP_HOST_CURRENT;

    for (size_t i = 0; i < info->count; i++) {
        req.params[0] = (uint8_t)i;

        if ((rc = hidpp_device_request(dev, &req, &res))) {
            if (is_transient(dev, rc))
                return rc;

            act->controls[i] = 0;
        } else {
            act->controls[i] = HIDPP_WORD(res.params[0], res.params[1]);
        }
    }

    unpack_flags(info);
    info->initialized = HIDPP_TRUE;
    return HIDPP_OK;
}

static int ensure_init(struct hidpp_feat_action **out, hidpp_device_t *dev) {
    void *feat;
    int rc = hidpp__feature_init(dev, HIDPP_FEAT_ACTION, &feat);
    *out = feat;
    return rc;
}

static size_t save_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_action *act = feat;
    uint8_t *start = b;
    (void)dev;

    SAVE_BYTE(b, act->info.initialized);
    SAVE_WORD(b, act->info.flags);
    SAVE_BYTE(b, act->info.count);
    SAVE_BYTE(b, act->info.hostCount);

    for (size_t i = 0; i < act->info.count; i++)
        SAVE_WORD(b, act->controls[i]);

    return b - start;
}

static size_t load_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_action *act = feat;
    uint8_t *start = b;
    (void)dev;

    act->info.initialized = LOAD_BYTE(b);
    act->info.flags = LOAD_WORD(b);
    act->info.count = LOAD_BYTE(b);
    act->info.hostCount = LOAD_BYTE(b);

    for (size_t i = 0; i < act->info.count; i++)
        act->controls[i] = LOAD_WORD(b);

    unpack_flags(&act->info);

    return b - start;
}

const struct hidpp_feat_vt hidpp_feat_action_vt = {
    FEAT_VT_SIZE(struct hidpp_feat_action),
    .ids = { ACTION_ID },
    .query = query_info,
    .saveCache = save_cache,
    .loadCache = load_cache,
};

static int has_control(const struct hidpp_feat_action *act, uint16_t id) {
    for (size_t i = 0; i < act->info.count; i++)
        if (act->controls[i] == id && id != 0)
            return 1;

    return 0;
}

static int check_host(const struct hidpp_action_info *info, uint8_t host) {
    if (host == HIDPP_HOST_CURRENT)
        return HIDPP_OK;

    /* without a count there is nothing to check against */
    if (info->hostCount == 0 || info->hostCount == UNKNOWN_HOST_COUNT)
        return HIDPP_OK;

    return host < info->hostCount ? HIDPP_OK : HIDPP_EINVAL;
}

/* The feature must work and `id` must be one of its controls. */
static int check_target(
    struct hidpp_feat_action *act,
    hidpp_device_t *dev,
    uint16_t id,
    uint8_t host
) {
    (void)dev;

    if (!act->info.initialized)
        return HIDPP_EIO;

    if (!has_control(act, id))
        return HIDPP_ENOENT;

    return check_host(&act->info, host);
}

struct hidpp_action_info *hidpp_action_info(hidpp_device_t *dev) {
    struct hidpp_feat_action *act;

    if (!dev || ensure_init(&act, dev))
        return NULL;

    return &act->info;
}

int hidpp_action_id(hidpp_device_t *dev, uint8_t index) {
    struct hidpp_feat_action *act;
    int rc;

    if ((rc = ensure_init(&act, dev)))
        return rc;

    if (index >= act->info.count || act->controls[index] == 0)
        return HIDPP_ENOENT;

    return act->controls[index];
}

size_t hidpp_action_list(hidpp_device_t *dev, uint16_t *out, size_t max) {
    struct hidpp_feat_action *act;

    if (!dev || !out || max == 0 || ensure_init(&act, dev))
        return 0;

    size_t count = act->info.count < max ? act->info.count : max;

    for (size_t i = 0; i < count; i++)
        out[i] = act->controls[i];

    return count;
}

int hidpp_action_get(
    hidpp_device_t *dev, uint16_t id, uint8_t host, struct hidpp_action *out
) {
    struct hidpp_feat_action *act;
    int rc;

    if (!out)
        return HIDPP_EINVAL;

    if ((rc = ensure_init(&act, dev))
        || (rc = check_target(act, dev, id, host)))
        return rc;

    hidpp_packet_t req, res;
    make_packet(&req, dev, act->base.featIndex, 3);
    req.params[0] = HIDPP_MSB(id);
    req.params[1] = HIDPP_LSB(id);
    req.params[2] = host;

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    if (HIDPP_WORD(res.params[0], res.params[1]) != id)
        return HIDPP_EIO;

    out->id = id;
    out->host = res.params[2];
    out->type = res.params[3];
    out->value = HIDPP_WORD(res.params[4], res.params[5]);
    out->modifiers = res.params[6];
    out->status = res.params[7];
    out->isRemapped = !!(out->status & STATUS_REMAPPED_FLAG);
    out->name = hidpp_keymap_name(id);
    out->typeName = hidpp_action_type_name(out->type);

    /* the data of an empty action is meaningless */
    if (out->type == HIDPP_ACTION_NONE)
        out->value = out->modifiers = 0;

    return HIDPP_OK;
}

int hidpp_action_set(
    hidpp_device_t *dev,
    uint16_t id,
    uint8_t host,
    uint8_t type,
    uint16_t value,
    uint8_t modifiers
) {
    struct hidpp_feat_action *act;
    int rc;

    if (type > MAX_ACTION)
        return HIDPP_EINVAL;

    if ((rc = ensure_init(&act, dev))
        || (rc = check_target(act, dev, id, host)))
        return rc;

    if (!supports_type(&act->info, type))
        return HIDPP_ENOSYS;

    /* modifiers only exist for keys, nothing else has data to modify */
    if (type == HIDPP_ACTION_NONE)
        value = 0;

    if (type != HIDPP_ACTION_KEY && modifiers)
        return HIDPP_EINVAL;

    hidpp_packet_t req, res;
    make_packet(&req, dev, act->base.featIndex, 4);
    req.kind = HIDPP_KIND_LONG; /* 7 parameters */
    req.params[0] = HIDPP_MSB(id);
    req.params[1] = HIDPP_LSB(id);
    req.params[2] = host;
    req.params[3] = type;
    req.params[4] = HIDPP_MSB(value);
    req.params[5] = HIDPP_LSB(value);
    req.params[6] = modifiers;

    return hidpp_device_request(dev, &req, &res);
}

int hidpp_action_reset(hidpp_device_t *dev, uint16_t id, uint8_t host) {
    struct hidpp_feat_action *act;
    int rc;

    if ((rc = ensure_init(&act, dev))
        || (rc = check_target(act, dev, id, host)))
        return rc;

    hidpp_packet_t req, res;
    make_packet(&req, dev, act->base.featIndex, 5);
    req.params[0] = HIDPP_MSB(id);
    req.params[1] = HIDPP_LSB(id);
    req.params[2] = host;

    return hidpp_device_request(dev, &req, &res);
}

int hidpp_action_reset_all(hidpp_device_t *dev, uint8_t hostMask) {
    struct hidpp_feat_action *act;
    int rc;

    if (hostMask == 0)
        return HIDPP_EINVAL;

    if ((rc = ensure_init(&act, dev)))
        return rc;

    hidpp_packet_t req, res;
    make_packet(&req, dev, act->base.featIndex, 6);
    req.params[0] = hostMask;

    return hidpp_device_request(dev, &req, &res);
}
