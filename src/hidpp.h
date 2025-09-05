/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2025 Предраг Јовановић
    SPDX-FileCopyrightText: 2025 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#ifndef HIDPP_H
#define HIDPP_H

#include <hidapi.h>
#include <stddef.h>
#include <stdint.h>

struct hidpp_event {
    int type;
    union {
        uint16_t ctrl;
    } as;
};

typedef struct hidpp_device hidpp_device;
typedef int (*hidpp_handler)(const struct hidpp_event *e, void *user);

enum hidpp_device_type {
    HIDPP_TYPE_KEYBOARD,
    HIDPP_TYPE_REMOTE,
    HIDPP_TYPE_NUMPAD,
    HIDPP_TYPE_MOUSE,
    HIDPP_TYPE_TOUCHPAD,
    HIDPP_TYPE_TRACKBALL,
    HIDPP_TYPE_PRESENTER,
    HIDPP_TYPE_RECEIVER,
};

enum hidpp_error {
    HIDPP_OK = 0,
    HIDPP_ENOENT = -2,
    HIDPP_EINTR = -4,
    HIDPP_EIO = -5,
    HIDPP_ENOMEM = -12,
    HIDPP_EEXIST = -17,
    HIDPP_EINVAL = -22,
    HIDPP_ENOSYS = -38,
    HIDPP_ENODATA = -61,
};

hidpp_device *hidpp_open(hid_device *handle, uint8_t id, uint8_t swid);
int hidpp_ping(hidpp_device *dev, uint8_t data);
int hidpp_poll(hidpp_device *dev, hidpp_handler *handler, void *user);
void hidpp_close(hidpp_device *dev);

uint16_t hidpp_version(hidpp_device *dev);
uint8_t hidpp_swid(hidpp_device *dev);
uint8_t hidpp_device_id(hidpp_device *dev);
size_t hidpp_device_name(hidpp_device *dev, char *buf, size_t max);
int hidpp_device_type(hidpp_device *dev);

uint8_t hidpp_feat_index(hidpp_device *dev, uint16_t featid);
uint16_t hidpp_feat_id(hidpp_device *dev, uint8_t featindex);
uint16_t hidpp_feat_count(hidpp_device *dev);

#endif
