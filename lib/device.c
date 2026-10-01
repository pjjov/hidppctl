/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "common.h"

#include <allocator.h>
#include <allocator_joined.h>
#include <pf_macro.h>

#include <assert.h>

extern struct hidpp_feat_vt hidpp_feat_root_vt;
extern struct hidpp_feat_vt hidpp_feat_keymap_vt;
extern struct hidpp_feat_vt hidpp_feat_firmware_info_vt;

HIDPP_ENUM_GUARD(hidpp_feat, 0)
const struct hidpp_feat_vt *hidpp_feat_vtables[hidpp_feat_max] = {
    [HIDPP_FEAT_ROOT] = &hidpp_feat_root_vt,
    [HIDPP_FEAT_KEYMAP] = &hidpp_feat_keymap_vt,
    [HIDPP_FEAT_FIRMWARE_INFO] = &hidpp_feat_firmware_info_vt,
};

static int protocol_version(hidpp_device_t *dev) {
    hidpp_receiver_t *rcv = dev->receiver;

    hidpp_packet_t req, res;
    make_packet(&req, dev, 0, 1);
    req.params[2] = 0xAA;

    if (hidpp_request(rcv, &req, &res))
        return HIDPP_EIO;

    dev->version = HIDPP_WORD(res.params[0], res.params[1]);

    if (dev->version < HIDPP_WORD(2, 0))
        return HIDPP_EINVAL;

    return HIDPP_OK;
}

static hidpp_device_t *alloc_device() {
    struct joined_allocation_t blocks[hidpp_feat_max + 1];

    blocks[0].size = sizeof(hidpp_device_t);
    blocks[0].align = _Alignof(hidpp_device_t);

    for (size_t i = 0; i < hidpp_feat_max; i++) {
        blocks[i + 1].size = hidpp_feat_vtables[i]->size;
        blocks[i + 1].align = hidpp_feat_vtables[i]->alignment;
    }

    size_t size;
    void *buffer = allocate_joined(
        hidpp_allocator, blocks, PF_COUNTOF(blocks), &size
    );

    if (!buffer)
        return NULL;

    hidpp_device_t *dev = blocks[0].buffer;

    dev->allocBuffer = buffer;
    dev->allocSize = size;

    for (int i = 0; i < hidpp_feat_max; i++) {
        memset(blocks[i + 1].buffer, 0, blocks[i + 1].size);
        dev->features[i] = blocks[i + 1].buffer;
    }

    return dev;
}

static int init_features(hidpp_device_t *dev) {
    int rc = HIDPP_OK;

    for (size_t i = 0; !rc && i < hidpp_feat_max; i++)
        if (hidpp_feat_vtables[i]->init)
            rc = hidpp_feat_vtables[i]->init(dev, dev->features[i]);

    return rc;
}

static void free_features(hidpp_device_t *dev) {
    for (size_t i = 0; i < hidpp_feat_max; i++)
        if (hidpp_feat_vtables[i]->free)
            hidpp_feat_vtables[i]->free(dev, dev->features[i]);
}

hidpp_device_t *hidpp_device_open(hidpp_receiver_t *rcv, uint8_t device) {
    if (!rcv || device == 0 || (device > 6 && device != 0xFF))
        return NULL;

    int index = device != 0xFF ? device : 0;
    hidpp_device_t *dev;

    if ((dev = rcv->devices[index]))
        return dev;

    if (!(dev = alloc_device()))
        return NULL;

    dev->receiver = rcv;
    dev->index = device;
    dev->swid = rcv->swid;

    if (protocol_version(dev) || init_features(dev)) {
        dev->version = 0;
        free_features(dev);
        return NULL;
    }

    rcv->devices[index] = dev;
    return dev;
}

void hidpp_device_close(hidpp_device_t *dev) {
    if (!dev)
        return;

    free_features(dev);
    deallocate(hidpp_allocator, dev->allocBuffer, dev->allocSize);
}

int hidpp_device_info(hidpp_device_t *dev, struct hidpp_device_info *out) {
    if (!dev || !out)
        return HIDPP_EINVAL;

    out->major = HIDPP_MSB(dev->version);
    out->minor = HIDPP_LSB(dev->version);
    out->index = dev->index;
    return HIDPP_OK;
}

int hidpp_device_send(hidpp_device_t *dev, hidpp_packet_t *pkt) {
    return dev ? hidpp_send(dev->receiver, pkt) : HIDPP_EINVAL;
}

int hidpp_device_receive(hidpp_device_t *dev, hidpp_packet_t *out) {
    return dev ? hidpp_receive(dev->receiver, out) : HIDPP_EINVAL;
}

int hidpp_device_request(
    hidpp_device_t *dev, hidpp_packet_t *req, hidpp_packet_t *res
) {
    return dev ? hidpp_request(dev->receiver, req, res) : HIDPP_EINVAL;
}