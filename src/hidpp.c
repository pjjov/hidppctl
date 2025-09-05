/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2025 Предраг Јовановић
    SPDX-FileCopyrightText: 2025 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "hidpp.h"
#include <stdlib.h>
#include <string.h>

enum hidpp_report_types {
    HIDPP_SHORT = 0x10,
    HIDPP_LONG = 0x11,
};

#define HIDPP_MAKE(size, dev, featid, fn, ...) \
    { HIDPP_##size, (dev)->id, (featid), (fn << 4) | (dev)->swid, __VA_ARGS__ }

#define HIDPP_WORD(msb, lsb) (((uint16_t)(msb) << 8) | (uint16_t)(lsb))

typedef uint8_t hidpp_report[20];

struct hidpp_device {
    hid_device *handle;
    uint8_t id;
    uint8_t swid;
    uint16_t version;

    uint8_t featcount;
    uint8_t namelen;
    uint16_t features[256];
    char name[256];
};

int hidpp_recv_report(hidpp_device *dev, hidpp_report res) {
    while (1) {
        if (20 != hid_read(dev->handle, res, 20))
            return HIDPP_EIO;

        if (res[1] == dev->id) {
            if (dev->swid == (res[3] & 0xF))
                break;
        }
    }

    if (res[0] != HIDPP_LONG)
        return HIDPP_EIO;

    return HIDPP_OK;
}

int hidpp_send_report(hidpp_device *dev, hidpp_report req, hidpp_report res) {
    if (!dev || !req || !res)
        return HIDPP_EINVAL;

    size_t size = req[0] == 0x11 ? 20 : 7;
    if (size != hid_write(dev->handle, req, size))
        return HIDPP_EIO;

    if (hidpp_recv_report(dev, res))
        return HIDPP_EIO;

    int failed = res[1] != req[1];
    failed |= res[2] != req[2];
    failed |= res[3] != req[3];
    failed |= res[2] == 0x8F;
    return failed ? HIDPP_EIO : HIDPP_OK;
}

static uint16_t hidpp__version(hidpp_device *dev) {
    hidpp_report res, req = HIDPP_MAKE(SHORT, dev, 0, 1, 0, 0, 0x2A);
    if (hidpp_send_report(dev, req, res) || res[6] != req[6])
        return 0;

    return HIDPP_WORD(res[4], res[5]);
}

static uint8_t feature_count(hidpp_device *dev) {
    hidpp_report req = HIDPP_MAKE(SHORT, dev, hidpp_feat_index(dev, 0x0001), 0);
    hidpp_report res;

    if (hidpp_send_report(dev, req, res))
        return 0;

    return res[4];
}

static uint8_t feature_index(hidpp_device *dev, uint16_t id) {
    hidpp_report req = HIDPP_MAKE(SHORT, dev, 0, 0, id >> 8, id & 0xF);
    hidpp_report res;

    if (hidpp_send_report(dev, req, res))
        return 0;

    return res[4];
}

static void find_features(hidpp_device *dev) {
    dev->featcount = feature_count(dev);
    uint8_t index = feature_index(dev, 0x0001);

    for (int i = 0; i < dev->featcount; i++) {
        hidpp_report res, req = HIDPP_MAKE(SHORT, dev, index, 1, (uint8_t)i);

        if (0 == hidpp_send_report(dev, req, res))
            dev->features[i] = HIDPP_WORD(res[4], res[5]);
        else
            dev->features[i] = 0;
    }
}

static uint8_t device_name_length(hidpp_device *dev) {
    hidpp_report req = HIDPP_MAKE(SHORT, dev, hidpp_feat_index(dev, 0x0005), 0);
    hidpp_report res;
    return hidpp_send_report(dev, req, res) ? 0 : res[4];
}

static uint8_t device_name(hidpp_device *dev) {
    uint8_t length = device_name_length(dev);

    hidpp_report req = HIDPP_MAKE(SHORT, dev, hidpp_feat_index(dev, 0x0005), 1);

    hidpp_report res;
    size_t read = 0;
    while (read < length) {
        req[4] = read;

        if (hidpp_send_report(dev, req, res))
            break;

        size_t size = length - read < 16 ? length - read : 16;
        memcpy(&dev->name[read], &res[4], size);
        read += size;
    }

    dev->name[read + 1] = '\0';
    return read;
}

hidpp_device *hidpp_open(hid_device *handle, uint8_t id, uint8_t swid) {
    if (!handle || swid == 0)
        return NULL;

    hidpp_device *dev = malloc(sizeof(hidpp_device));
    if (!dev)
        return NULL;

    dev->handle = handle;
    dev->id = id;
    dev->swid = swid;
    dev->version = hidpp__version(dev);

    if (dev->version < HIDPP_WORD(2, 0)) {
        free(dev);
        return NULL;
    }

    device_name(dev);
    find_features(dev);
    return dev;
}

void hidpp_close(hidpp_device *dev) {
    if (dev) {
        free(dev);
    }
}

int hidpp_ping(hidpp_device *dev, uint8_t data) {
    if (!dev)
        return HIDPP_EINVAL;

    hidpp_report res, req = HIDPP_MAKE(SHORT, dev, 0, 1, 0, 0, data);
    if (hidpp_send_report(dev, req, res) || res[6] != req[6])
        return HIDPP_EIO;

    return HIDPP_OK;
}

uint16_t hidpp_version(hidpp_device *dev) { return dev->version; }
uint8_t hidpp_swid(hidpp_device *dev) { return dev->swid; }
uint8_t hidpp_device_id(hidpp_device *dev) { return dev->id; }
uint16_t hidpp_feat_count(hidpp_device *dev) { return dev->featcount; }

size_t hidpp_device_name(hidpp_device *dev, char *buf, size_t max) {
    if (!dev)
        return 0;

    if (!buf || max == 0)
        return dev->namelen;

    size_t len = dev->namelen > max ? max : dev->namelen;
    memcpy(buf, dev->name, len);
    return len;
}

int hidpp_device_type(hidpp_device *dev) {
    if (!dev)
        return -1;

    hidpp_report req = HIDPP_MAKE(SHORT, dev, hidpp_feat_index(dev, 0x0005), 2);
    hidpp_report res;
    return hidpp_send_report(dev, req, res) ? -1 : res[4];
}

uint8_t hidpp_feat_index(hidpp_device *dev, uint16_t featid) {
    if (!dev)
        return 0;

    for (int i = 0; i < dev->featcount; i++)
        if (dev->features[i] == featid)
            return i;

    return 0;
}

uint16_t hidpp_feat_id(hidpp_device *dev, uint8_t featindex) {
    if (!dev || featindex >= dev->featcount)
        return 0;
    return dev->features[featindex];
}
