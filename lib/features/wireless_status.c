/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

/** Section: HID++ feature documentation

    Wireless device status (id: 0x1D4B)
    The feature has no functions, its only event is always enabled.

    -----------------------
    Event 0 - wireless device status broadcast (after a power-on reset)
    -----------------------
        uint8_t status (0 = unknown, 1 = reconnection)
        uint8_t request (0 = none, 1 = software reconfiguration needed)
        uint8_t reason (0 = unknown, 1 = power switch activated)
*/

#define WIRELESS_STATUS_ID 0x1D4B

struct hidpp_feat_wireless_status {
    struct hidpp_feat_base base;
};

static int parse_event(
    hidpp_device_t *dev,
    void *feat,
    uint16_t id,
    uint8_t func,
    const hidpp_packet_t *pkt,
    struct hidpp_event *e
) {
    (void)dev, (void)feat;

    if (id != WIRELESS_STATUS_ID || func != 0)
        return HIDPP_ENOENT;

    e->type = HIDPP_EVENT_WIRELESS_STATUS;
    e->as.wirelessStatus.status = pkt->params[0];
    e->as.wirelessStatus.request = pkt->params[1];
    e->as.wirelessStatus.reason = pkt->params[2];
    return HIDPP_OK;
}

const struct hidpp_feat_vt hidpp_feat_wireless_status_vt = {
    FEAT_VT_SIZE(struct hidpp_feat_wireless_status),
    .ids = { WIRELESS_STATUS_ID },
    .parseEvent = parse_event,
};
