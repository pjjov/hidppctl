/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#ifndef HIDPP_COMMON_H
#define HIDPP_COMMON_H

#include <hidpp.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HIDPP_MAX_ERROR 256

/* Forward declarations */
typedef struct hid_device_ hid_device;
typedef struct allocator_t allocator_t;

extern allocator_t *hidpp_allocator;

enum {
    HIDPP_FEAT_ROOT,
    HIDPP_FEAT_KEYMAP,
    HIDPP_FEAT_FIRMWARE_INFO,
    HIDPP__FEAT_MAX,
};

struct hidpp_feat_vt {
    size_t size;
    size_t alignment;
    int (*init)(hidpp_device_t *dev, void *feat);
    void (*clearCache)(hidpp_device_t *dev, void *feat);
    void (*free)(hidpp_device_t *dev, void *feat);
};

struct hidpp_device_t {
    hidpp_receiver_t *receiver;
    uint16_t version;
    uint8_t index;
    uint8_t swid;

    void *allocBuffer;
    size_t allocSize;

    void *features[HIDPP__FEAT_MAX];
};

struct hidpp_receiver_t {
    hid_device *handle;
    uint8_t swid;
    uint8_t retries;
    uint8_t nonblocking : 1;
    int timeout;

    hidpp_device_t *devices[7];
    wchar_t error[HIDPP_MAX_ERROR];
};

static inline void make_packet(
    hidpp_packet_t *out, hidpp_device_t *dev, uint8_t feat, uint8_t func
) {
    hidpp_make(out, dev->index, feat, HIDPP_BYTE(func, dev->swid), NULL, 0);
    memset(out->params, 0, sizeof(out->params));
}

#ifdef __cplusplus
}
#endif

#endif /* HIDPP_COMMON_H */