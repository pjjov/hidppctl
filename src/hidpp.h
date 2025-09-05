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

typedef struct hid_device hid_device;
typedef struct hidpp_device hidpp_device;

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

hidpp_device* hidpp_open(hid_device* handle, uint8_t id, uint8_t swid);
int hidpp_ping(hidpp_device* dev, uint8_t data);
void hidpp_close(hidpp_device* dev);

uint16_t hidpp_version(hidpp_device* dev);
uint8_t hidpp_swid(hidpp_device* dev);
uint8_t hidpp_device_id(hidpp_device* dev);
size_t hidpp_device_name(hidpp_device* dev, char* buf, size_t max);
int hidpp_device_type(hidpp_device* dev);

uint8_t hidpp_feat_index(hidpp_device* dev, uint16_t featid);
uint16_t hidpp_feat_id(hidpp_device* dev, uint8_t featindex);
uint16_t hidpp_feat_count(hidpp_device* dev);

#endif
