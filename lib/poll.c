/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "common.h" /* IWYU pragma: keep */

static int parse_event(
    hidpp_device_t *dev, struct hidpp_event *e, hidpp_packet_t *pkt
) {
    int id = hidpp_feature_id(dev, pkt->feat);

    memset(&e->as, 0, sizeof(e->as));

    if (id >= 0
        && hidpp__feature_parse_event(dev, (uint16_t)id, pkt, e) == HIDPP_OK)
        return HIDPP_OK;

    e->type = HIDPP_EVENT_UNKNOWN;
    memcpy(&e->as.unknown, pkt, sizeof(*pkt));
    return HIDPP_OK; /* the event type tells the caller it is unparsed */
}

/*  Notifications use software id 0. HID++ error reports have the 0xFF (2.0)
    or 0x8F (1.0) marker in place of the feature index and carry the failed
    request's feature index in the next byte; they are never events. */
static int is_event(const hidpp_packet_t *pkt) {
    return !(pkt->func & 0xF) && pkt->feat != 0xFF && pkt->feat != 0x8F;
}

int hidpp_device_poll(hidpp_device_t *dev, struct hidpp_event *out) {
    if (!dev || !out)
        return HIDPP_EINVAL;

    hidpp_packet_t pkt;
    int res;

    do {
        if ((res = hidpp_device_receive(dev, &pkt)))
            return res;
    } while (dev->index != pkt.device || !is_event(&pkt));

    return parse_event(dev, out, &pkt);
}

int hidpp_poll(hidpp_receiver_t *rcv, struct hidpp_event *out) {
    if (!rcv || !out)
        return HIDPP_EINVAL;

    hidpp_packet_t pkt;
    int res;

    do {
        if ((res = hidpp_receive(rcv, &pkt)))
            return res;
    } while (!is_event(&pkt));

    out->type = HIDPP_EVENT_UNKNOWN;
    memcpy(&out->as.unknown, &pkt, sizeof(pkt));
    return HIDPP_OK;
}