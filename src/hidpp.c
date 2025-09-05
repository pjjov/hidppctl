/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2025 Предраг Јовановић
    SPDX-FileCopyrightText: 2025 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "hidpp.h"
#include <stdlib.h>

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
size_t hidpp_device_name(hidpp_device *dev, char *buf, size_t max) { return 0; }
int hidpp_device_type(hidpp_device *dev) { return 0; }

uint8_t hidpp_feat_index(hidpp_device *dev, uint16_t featid) { return 0; }
uint16_t hidpp_feat_id(hidpp_device *dev, uint8_t featindex) { return 0; }
uint16_t hidpp_feat_count(hidpp_device *dev) { return 0; }
