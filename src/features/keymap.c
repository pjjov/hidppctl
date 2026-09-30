/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

#include <string.h>

/** Section: HID++ feature documentation

    Remappable controls (id: 0x1b04, 0x1b00)
    Big endian integers are exchanged.

    -----------------------
    Fn 0 - get count
    -----------------------
    request:
    response:
        uint8_t ctrlCount

    ----------------------
    Fn 1 - get control info
    -----------------------
    request:
        uint8_t ctrlIndex
    response:
        uint16_t ctrlId
        uint16_t taskId
        uint8_t flags
        - bit 7 - virtual
        - bit 6 - persist
        - bit 5 - divert
        - bit 4 - reprog
        - bit 3 - fntog
        - bit 2 - hotkey
        - bit 1 - fkey
        - bit 0 - mouse
        uint8_t position
        uint8_t group
        uint8_t groupMask
        uint8_t rawXYflags
        - bit 0 - rawXY

    ----------------------
    Fn 2 - get control reporting
    -----------------------
    request:
        uint16_t ctrlId
    response:
        uint16_t ctrlId
        uint8_t flags
        - bit 4 - rawXY
        - bit 2 - persist
        - bit 0 - divert
        uint16_t remapId

    ----------------------
    Fn 3 - set control reporting
    -----------------------
    request:
        uint16_t ctrlId
        uint8_t flags
        - bit 5 - rvalid
        - bit 4 - rawXY
        - bit 3 - pvalid
        - bit 2 - persist
        - bit 1 - dvalid
        - bit 0 - divert
        uint16_t remapId
    response:
        (echoes request packet)
*/

#define MAX_CONTROLS UINT8_MAX

/* clang-format off */

#define IS_VIRTUAL_FLAG        0x80
#define IS_PERSISTABLE_FLAG    0x40
#define IS_DIVERTABLE_FLAG     0x20
#define IS_REPROGRAMMABLE_FLAG 0x10
#define IS_FN_TOGGLABLE_FLAG   0x08
#define IS_HOTKEY_FLAG         0x04
#define IS_FUNCTION_KEY_FLAG   0x02
#define IS_MOUSE_BUTTON_FLAG   0x01

#define STATE_RAWXY_FLAG       0x10
#define STATE_PERSISTENT_FLAG  0x04
#define STATE_DIVERTED_FLAG    0x01

/* clang-format on */

struct hidpp_keymap_t {
    hidpp_device_t *device;
    hidpp_bool_t initialized;
    hidpp_bool_t unsupported;

    uint8_t featIndex;
    uint8_t featVersion;

    uint8_t ctrlCount;
    struct hidpp_keymap_info controls[MAX_CONTROLS];
};

static int query_keymap_version(hidpp_keymap_t *map) {
    int version, result;

    if ((result = hidpp_feature_index(map->device, 0x1B04)) > 0)
        version = 4;
    else if ((result = hidpp_feature_index(map->device, 0x1B00)) > 0)
        version = 1;

    map->initialized = HIDPP_TRUE;

    if (result <= 0) {
        map->unsupported = HIDPP_TRUE;
        return HIDPP_ENOSYS;
    }

    map->featIndex = result;
    map->featVersion = version;
    return HIDPP_OK;
}

static int query_control_count(hidpp_keymap_t *map) {
    hidpp_packet_t req, res;
    make_packet(&req, map->device, map->featIndex, 0);

    if (hidpp_device_request(map->device, &req, &res))
        return 0;

    return res.params[0];
}

static int query_control_info(
    struct hidpp_keymap_info *out, hidpp_keymap_t *map, uint8_t ctrlIndex
) {
    hidpp_packet_t req, res;
    make_packet(&req, map->device, map->featIndex, 1);
    req.params[0] = ctrlIndex;

    int rc;

    if ((rc = hidpp_device_request(map->device, &req, &res)))
        return rc;

    out->index = ctrlIndex;
    out->id = HIDPP_WORD(res.params[0], res.params[1]);
    out->taskId = HIDPP_WORD(res.params[2], res.params[3]);
    out->flags = res.params[4];
    out->position = res.params[5];
    out->group = res.params[6];
    out->groupMask = res.params[7];
    out->rawXY = res.params[8];

    out->isVirtual = !!(out->flags & IS_VIRTUAL_FLAG);
    out->isPersistable = !!(out->flags & IS_PERSISTABLE_FLAG);
    out->isDivertable = !!(out->flags & IS_DIVERTABLE_FLAG);
    out->isReprogrammable = !!(out->flags & IS_REPROGRAMMABLE_FLAG);
    out->isFnTogglable = !!(out->flags & IS_FN_TOGGLABLE_FLAG);
    out->isHotkey = !!(out->flags & IS_HOTKEY_FLAG);
    out->isFunctionKey = !!(out->flags & IS_FUNCTION_KEY_FLAG);
    out->isMouseButton = !!(out->flags & IS_MOUSE_BUTTON_FLAG);

    out->initialized = HIDPP_TRUE;
    out->name = hidpp_keymap_name(out->id);
    out->remapName = NULL;

    return HIDPP_OK;
}

static int ensure_init(hidpp_keymap_t *map) {
    if (map->initialized)
        return map->unsupported ? HIDPP_ENOSYS : HIDPP_OK;

    memset(map, 0, sizeof(*map));

    if (query_keymap_version(map))
        return HIDPP_ENOSYS;

    map->ctrlCount = query_control_count(map);

    for (size_t i = 0; i < map->ctrlCount; i++)
        query_control_info(&map->controls[i], map, i);

    return HIDPP_OK;
}

static void clear_cache(hidpp_device_t *dev, void *feat) {
    hidpp_keymap_t *map = feat;
    map->initialized = HIDPP_FALSE;
}

const struct hidpp_feat_vt hidpp_feat_keymap_vt = {
    .size = sizeof(hidpp_keymap_t),
    .alignment = _Alignof(hidpp_keymap_t),
    .clearCache = clear_cache,
};

hidpp_keymap_t *hidpp_keymap(hidpp_device_t *dev) {
    if (!dev)
        return NULL;

    hidpp_keymap_t *out = dev->features[HIDPP_FEAT_KEYMAP];
    return ensure_init(out) ? NULL : out;
}

struct hidpp_keymap_info *hidpp_keymap_info(hidpp_keymap_t *map, uint16_t id) {
    if (!map || ensure_init(map))
        return NULL;

    struct hidpp_keymap_info *info;

    for (size_t i = 0; i < map->ctrlCount; i++) {
        info = &map->controls[i];
        if (info->initialized && info->id == id)
            return info;
    }

    return NULL;
}

int hidpp_keymap_index(hidpp_keymap_t *map, uint16_t control) {
    if (!map || ensure_init(map))
        return HIDPP_EINVAL;

    for (int i = 0; i < map->ctrlCount; i++)
        if (map->controls[i].id == control)
            return i;

    return HIDPP_ENOENT;
}

int hidpp_keymap_id(hidpp_keymap_t *map, uint8_t index) {
    if (!map || ensure_init(map))
        return HIDPP_EINVAL;

    return index < map->ctrlCount && map->controls[index].initialized
        ? map->controls[index].id
        : HIDPP_ENOENT;
}

size_t hidpp_keymap_list(hidpp_keymap_t *map, uint16_t *out, size_t max) {
    if (!map || !out || max == 0)
        return 0;

    size_t count = map->ctrlCount < max ? map->ctrlCount : max;

    for (size_t i = 0; i < count; i++)
        out[i] = map->controls[i].initialized ? map->controls[i].id : 0;

    return count;
}

int hidpp_keymap_divert(hidpp_keymap_t *map, uint16_t id, int value) {
    if (!map || ensure_init(map))
        return HIDPP_EINVAL;

    hidpp_packet_t req = { 0 };
    make_packet(&req, map->device, map->featIndex, 3);
    req.params[0] = HIDPP_MSB(id);
    req.params[1] = HIDPP_LSB(id);
    req.params[2] = value ? 3 : 0;

    return hidpp_device_send(map->device, &req);
}

int hidpp_keymap_remap(hidpp_keymap_t *map, uint16_t id, uint16_t remap) {
    if (!map || ensure_init(map))
        return HIDPP_EINVAL;

    hidpp_packet_t req = { 0 };
    make_packet(&req, map->device, map->featIndex, 3);
    req.params[0] = HIDPP_MSB(id);
    req.params[1] = HIDPP_LSB(id);
    req.params[2] = 0;
    req.params[3] = HIDPP_MSB(remap);
    req.params[4] = HIDPP_LSB(remap);

    return hidpp_device_send(map->device, &req);
}

int hidpp_keymap_state(
    hidpp_keymap_t *map, uint16_t id, struct hidpp_keymap_state *out
) {
    if (!map || !out || ensure_init(map))
        return HIDPP_EINVAL;

    hidpp_packet_t req, res;
    make_packet(&req, map->device, map->featIndex, 2);

    int rc;

    if ((rc = hidpp_device_request(map->device, &req, &res)))
        return rc;

    out->remapId = HIDPP_WORD(res.params[3], res.params[4]);
    out->flags = res.params[2];
    out->rawXY = !!(out->flags & STATE_RAWXY_FLAG);
    out->isPersistent = !!(out->flags & STATE_PERSISTENT_FLAG);
    out->isDiverted = !!(out->flags & STATE_DIVERTED_FLAG);

    return HIDPP_OK;
}