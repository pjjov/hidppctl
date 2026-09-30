/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

/** Section: HID++ feature documentation

    Root feature (id: 0x0000)
    Big endian integers are exchanged.

    -----------------------
    Fn 0 - get feature index
    -----------------------
    request:
        uint16_t featureId
    response:
        uint8_t featureIndex
        uint8_t featureType
        - bit 7 - isObsolete
        - bit 6 - isHidden
        - bit 5 - reserved
        uint8_t version

    ----------------------
    Fn 1 - ping
    -----------------------
    request:
        uint16_t allZeros
        uint8_t pingData
    response:
        uint8_t protocolVersionMajor
        uint8_t protocolVersionMinor
        uint8_t pingData (echoed)

    Feature set information (id: 0x0001)
    Big endian integers are exchanged.

    -----------------------
    Fn 0 - get feature count
    -----------------------
    request:
    response:
        uint8_t count

    -----------------------
    Fn 1 - get feature id
    -----------------------
    request:
        uint8_t featureIndex
    response:
        uint16_t featureId
        uint8_t featureType
        - bit 7 - isObsolete
        - bit 6 - isHidden
        - bit 5 - reserved
        uint8_t version
*/

#define FEATURE_SET_ID 0x0001
#define MAX_FEAT_COUNT 256

#define IS_OBSOLETE_FLAG 0x80
#define IS_HIDDEN_FLAG 0x40

struct hidpp_feat_root {
    hidpp_bool_t initialized;
    uint8_t featCount;
    uint8_t featSetIndex; /* 0x0001 */
    struct hidpp_feature_info infos[MAX_FEAT_COUNT];
};

static uint8_t root_feature_index(hidpp_device_t *dev, uint16_t feat) {
    hidpp_packet_t req, res;
    make_packet(&req, dev, 0, 0);
    req.params[0] = HIDPP_MSB(feat);
    req.params[1] = HIDPP_LSB(feat);

    if (hidpp_device_request(dev, &req, &res))
        return 0;
    return res.params[0];
}

static uint8_t query_feature_count(hidpp_device_t *dev, uint8_t featIndex) {
    hidpp_packet_t req, res;
    make_packet(&req, dev, featIndex, 0);

    if (featIndex == 0 || hidpp_device_request(dev, &req, &res))
        return 0;

    return res.params[0];
}

static int query_feature_info_by_id(
    struct hidpp_feature_info *out, hidpp_device_t *dev, uint16_t feat
) {
    hidpp_packet_t req, res;
    make_packet(&req, dev, 0, 0);
    req.params[0] = HIDPP_MSB(feat);
    req.params[1] = HIDPP_LSB(feat);

    int rc;

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    out->initialized = HIDPP_TRUE;
    out->id = feat;
    out->index = res.params[0];
    out->flags = res.params[1];
    out->version = res.params[2];
    out->name = hidpp_feature_name(out->id);
    out->isObsolete = !!(out->flags & IS_OBSOLETE_FLAG);
    out->isHidden = !!(out->flags & IS_HIDDEN_FLAG);

    return HIDPP_OK;
}

static int query_feature_info_by_index(
    struct hidpp_feature_info *out,
    hidpp_device_t *dev,
    uint8_t reqIndex,
    uint8_t featSetIndex
) {
    hidpp_packet_t req, res;
    make_packet(&req, dev, featSetIndex, 1);
    req.params[0] = reqIndex;

    int rc;

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    out->initialized = HIDPP_TRUE;
    out->index = reqIndex;
    out->id = HIDPP_WORD(res.params[0], res.params[1]);
    out->flags = res.params[2];
    out->version = res.params[3];
    out->name = hidpp_feature_name(out->id);
    out->isObsolete = !!(out->flags & IS_OBSOLETE_FLAG);
    out->isHidden = !!(out->flags & IS_HIDDEN_FLAG);

    return HIDPP_OK;
}

static int ensure_init(struct hidpp_feat_root **out, hidpp_device_t *dev) {
    struct hidpp_feat_root *root;

    if (!out || !dev)
        return HIDPP_EINVAL;

    root = dev->features[HIDPP_FEAT_ROOT];
    *out = root;

    if (root->initialized)
        return HIDPP_OK;

    memset(root->infos, 0, sizeof(root->infos));
    root->featSetIndex = root_feature_index(dev, FEATURE_SET_ID);
    root->featCount = query_feature_count(dev, root->featSetIndex);
    root->initialized = HIDPP_TRUE;

    return HIDPP_OK;
}

static void clear_cache(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_root *root = feat;
    root->initialized = HIDPP_FALSE;
}

const struct hidpp_feat_vt hidpp_feat_root_vt = {
    .size = sizeof(struct hidpp_feat_root),
    .alignment = _Alignof(struct hidpp_feat_root),
    .clearCache = clear_cache,
};

int hidpp_feature_index(hidpp_device_t *dev, uint16_t featureId) {
    if (!dev)
        return HIDPP_EINVAL;

    struct hidpp_feat_root *root;
    struct hidpp_feature_info *info, tmp;
    int rc;

    if ((rc = ensure_init(&root, dev)))
        return rc;

    for (size_t i = 0; i < MAX_FEAT_COUNT; i++) {
        info = &root->infos[i];
        if (info->initialized && info->id == featureId)
            return (int)info->index;
    }

    if ((rc = query_feature_info_by_id(&tmp, dev, featureId)))
        return rc;

    memcpy(&root->infos[tmp.index], &tmp, sizeof(tmp));
    return tmp.index;
}

int hidpp_feature_id(hidpp_device_t *dev, uint8_t index) {
    if (!dev)
        return HIDPP_EINVAL;

    struct hidpp_feat_root *root;
    struct hidpp_feature_info *info;
    int rc;

    if ((rc = ensure_init(&root, dev)))
        return rc;

    info = &root->infos[index];

    if (info->initialized)
        return info->id;

    if ((rc = query_feature_info_by_index(
             info, dev, index, root->featSetIndex
         )))
        return rc;

    return info->id;
}

struct hidpp_feature_info *hidpp_feature_info(
    hidpp_device_t *dev, uint16_t featId
) {
    if (!dev)
        return NULL;

    struct hidpp_feat_root *root;
    int index;

    if (ensure_init(&root, dev))
        return NULL;

    if ((index = hidpp_feature_index(dev, featId)) < 0)
        return NULL;

    if (index > MAX_FEAT_COUNT)
        return NULL;

    return &root->infos[index];
}

size_t hidpp_feature_list(hidpp_device_t *dev, uint16_t *out, size_t max) {
    if (!dev || !out || max == 0)
        return 0;

    struct hidpp_feat_root *root;

    if (ensure_init(&root, dev))
        return 0;

    size_t count = root->featCount < max ? root->featCount : max;

    for (size_t i = 0; i < count; i++) {
        int result = hidpp_feature_id(dev, i);
        if (result >= 0 && result <= UINT16_MAX)
            out[i] = result;
        else
            out[i] = 0;
    }

    return count;
}

int hidpp_ping(hidpp_device_t *dev, uint8_t data) {
    if (!dev)
        return HIDPP_EINVAL;

    hidpp_receiver_t *rcv = dev->receiver;

    hidpp_packet_t req, res;
    make_packet(&req, dev, 0, 1);
    req.params[0] = data;

    if (hidpp_request(rcv, &req, &res) || res.params[2] != req.params[2])
        return HIDPP_EIO;

    return HIDPP_OK;
}