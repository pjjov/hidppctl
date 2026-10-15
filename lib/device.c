/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "common.h"

#include <assert.h>
#include <stdalign.h>

#include <allocator.h>
#include <allocator_joined.h>
#include <pf_macro.h>

extern struct hidpp_feat_vt hidpp_feat_root_vt;
extern struct hidpp_feat_vt hidpp_feat_keymap_vt;
extern struct hidpp_feat_vt hidpp_feat_firmware_info_vt;
extern struct hidpp_feat_vt hidpp_feat_device_name_vt;
extern struct hidpp_feat_vt hidpp_feat_battery_vt;
extern struct hidpp_feat_vt hidpp_feat_nickname_vt;
extern struct hidpp_feat_vt hidpp_feat_hosts_vt;
extern struct hidpp_feat_vt hidpp_feat_backlight_vt;
extern struct hidpp_feat_vt hidpp_feat_illumination_vt;
extern struct hidpp_feat_vt hidpp_feat_action_vt;
extern struct hidpp_feat_vt hidpp_feat_scrolling_vt;
extern struct hidpp_feat_vt hidpp_feat_smartshift_vt;
extern struct hidpp_feat_vt hidpp_feat_hires_wheel_vt;
extern struct hidpp_feat_vt hidpp_feat_pointer_vt;
extern struct hidpp_feat_vt hidpp_feat_dpi_vt;
extern struct hidpp_feat_vt hidpp_feat_fn_inversion_vt;
extern struct hidpp_feat_vt hidpp_feat_solar_vt;
extern struct hidpp_feat_vt hidpp_feat_disable_keys_vt;
extern struct hidpp_feat_vt hidpp_feat_platform_vt;
extern struct hidpp_feat_vt hidpp_feat_crown_vt;
extern struct hidpp_feat_vt hidpp_feat_brightness_vt;
extern struct hidpp_feat_vt hidpp_feat_report_rate_vt;
extern struct hidpp_feat_vt hidpp_feat_led_vt;
extern struct hidpp_feat_vt hidpp_feat_perkey_vt;
extern struct hidpp_feat_vt hidpp_feat_mode_status_vt;
extern struct hidpp_feat_vt hidpp_feat_equalizer_vt;

PF_ENUM_GUARD(hidpp_feat, 4)
const struct hidpp_feat_vt *hidpp_feat_vtables[hidpp_feat_max] = {
    [HIDPP_FEAT_ROOT] = &hidpp_feat_root_vt,
    [HIDPP_FEAT_KEYMAP] = &hidpp_feat_keymap_vt,
    [HIDPP_FEAT_FIRMWARE_INFO] = &hidpp_feat_firmware_info_vt,
    [HIDPP_FEAT_DEVICE_NAME] = &hidpp_feat_device_name_vt,
    [HIDPP_FEAT_BATTERY] = &hidpp_feat_battery_vt,
    [HIDPP_FEAT_NICKNAME] = &hidpp_feat_nickname_vt,
    [HIDPP_FEAT_HOSTS] = &hidpp_feat_hosts_vt,
    [HIDPP_FEAT_BACKLIGHT] = &hidpp_feat_backlight_vt,
    [HIDPP_FEAT_ILLUMINATION] = &hidpp_feat_illumination_vt,
    [HIDPP_FEAT_ACTION] = &hidpp_feat_action_vt,
    [HIDPP_FEAT_SCROLLING] = &hidpp_feat_scrolling_vt,
    [HIDPP_FEAT_SMARTSHIFT] = &hidpp_feat_smartshift_vt,
    [HIDPP_FEAT_HIRES_WHEEL] = &hidpp_feat_hires_wheel_vt,
    [HIDPP_FEAT_POINTER] = &hidpp_feat_pointer_vt,
    [HIDPP_FEAT_DPI] = &hidpp_feat_dpi_vt,
    [HIDPP_FEAT_FN_INVERSION] = &hidpp_feat_fn_inversion_vt,
    [HIDPP_FEAT_SOLAR] = &hidpp_feat_solar_vt,
    [HIDPP_FEAT_DISABLE_KEYS] = &hidpp_feat_disable_keys_vt,
    [HIDPP_FEAT_PLATFORM] = &hidpp_feat_platform_vt,
    [HIDPP_FEAT_CROWN] = &hidpp_feat_crown_vt,
    [HIDPP_FEAT_BRIGHTNESS] = &hidpp_feat_brightness_vt,
    [HIDPP_FEAT_REPORT_RATE] = &hidpp_feat_report_rate_vt,
    [HIDPP_FEAT_LED] = &hidpp_feat_led_vt,
    [HIDPP_FEAT_PERKEY] = &hidpp_feat_perkey_vt,
    [HIDPP_FEAT_MODE_STATUS] = &hidpp_feat_mode_status_vt,
    [HIDPP_FEAT_SIDETONE] = &hidpp_feat_sidetone_vt,
    [HIDPP_FEAT_EQUALIZER] = &hidpp_feat_equalizer_vt,
};

static int protocol_version(hidpp_device_t *dev) {
    hidpp_receiver_t *rcv = dev->receiver;

    hidpp_packet_t req, res;
    make_packet(&req, dev, 0, 1);
    req.params[2] = 0xAA;

    /* HID++ 1.0 devices answer with an "invalid sub id" error. */
    if (hidpp_request(rcv, &req, &res))
        return HIDPP_EIO;

    if (res.params[2] != 0xAA)
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

    memset(buffer, 0, size);

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
        free_features(dev);
        deallocate(hidpp_allocator, dev->allocBuffer, dev->allocSize);
        return NULL;
    }

    rcv->devices[index] = dev;
    return dev;
}

void hidpp_device_close(hidpp_device_t *dev) {
    if (!dev)
        return;

    if (dev->receiver) {
        size_t slot = dev->index != 0xFF ? dev->index : 0;

        if (slot < PF_COUNTOF(dev->receiver->devices)
            && dev->receiver->devices[slot] == dev)
            dev->receiver->devices[slot] = NULL;
    }

    free_features(dev);

    void *buffer = dev->allocBuffer;
    size_t size = dev->allocSize;
    deallocate(hidpp_allocator, buffer, size);
}

int hidpp_device_info(hidpp_device_t *dev, struct hidpp_device_info *out) {
    if (!dev || !out)
        return HIDPP_EINVAL;

    out->major = HIDPP_MSB(dev->version);
    out->minor = HIDPP_LSB(dev->version);
    out->index = dev->index;

    int type = hidpp_device_type(dev);
    out->type = type >= 0 ? type : 0;
    out->name = hidpp_device_name(dev);
    out->nickname = hidpp_nickname_get(dev);
    out->serialNumber = hidpp_serial_number(dev);
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