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
    uint16_t feat = id >= 0 ? (uint16_t)id : 0xFFFF;

    /* function id is the high nibble, the low one is the software id */
    uint8_t func = HIDPP_MSN(pkt->func);

    switch (feat) {

    case 0x1000: /* Battery status */
        if (func == 0) {
            e->type = HIDPP_EVENT_BATTERY;
            e->as.battery.chargeState = pkt->params[0];
            e->as.battery.batteryLevel = pkt->params[1];
            e->as.battery.chargeStatus = pkt->params[2];
            e->as.battery.externalPower = pkt->params[3];
            return HIDPP_OK;
        }

        break;

    case 0x1b00: /* Remappable buttons */
    case 0x1b02:
    case 0x1b04:
        if (func == 0) {
            e->type = HIDPP_EVENT_BUTTON;
            e->as.buttons[0] = HIDPP_WORD(pkt->params[0], pkt->params[1]);
            e->as.buttons[1] = HIDPP_WORD(pkt->params[2], pkt->params[3]);
            e->as.buttons[2] = HIDPP_WORD(pkt->params[4], pkt->params[5]);
            e->as.buttons[3] = HIDPP_WORD(pkt->params[6], pkt->params[7]);
            return HIDPP_OK;
        } else if (func == 1) {
            e->type = HIDPP_EVENT_MOUSE;
            e->as.mouse[0] = HIDPP_WORD(pkt->params[0], pkt->params[1]);
            e->as.mouse[1] = HIDPP_WORD(pkt->params[2], pkt->params[3]);
            return HIDPP_OK;
        }

        break;

    case 0x1982: /* Backlight */
        if (func == 0) {
            e->type = HIDPP_EVENT_BACKLIGHT;
            e->as.backlight.levelCount = pkt->params[0];
            e->as.backlight.level = pkt->params[1];
            e->as.backlight.status = pkt->params[2];
            e->as.backlight.effect = pkt->params[3];
            return HIDPP_OK;
        }

        break;

    case 0x1990: /* Illumination light */
        if (func == 0) {
            e->type = HIDPP_EVENT_ILLUMINATION;
            e->as.illumination.kind = HIDPP_ILLUMINATION_EVENT_STATE;
            e->as.illumination.state = pkt->params[0] & 1;
            e->as.illumination.source = 0;
            e->as.illumination.value = 0;
            return HIDPP_OK;
        } else if (func >= 1 && func <= 3) {
            static const uint8_t kinds[] = {
                0,
                HIDPP_ILLUMINATION_EVENT_BRIGHTNESS,
                HIDPP_ILLUMINATION_EVENT_COLOR_TEMPERATURE,
                HIDPP_ILLUMINATION_EVENT_BRIGHTNESS_MAX,
            };

            e->type = HIDPP_EVENT_ILLUMINATION;
            e->as.illumination.kind = kinds[func];
            e->as.illumination.state = 0;
            e->as.illumination.source = 0;
            e->as.illumination.value = HIDPP_WORD(
                pkt->params[0], pkt->params[1]
            );
            return HIDPP_OK;
        } else if (func >= 4) {
            e->type = HIDPP_EVENT_ILLUMINATION;
            e->as.illumination.kind = HIDPP_ILLUMINATION_EVENT_CLAMPED;
            e->as.illumination.state = 0;
            e->as.illumination.source = pkt->params[0];
            e->as.illumination.value = HIDPP_WORD(
                pkt->params[1], pkt->params[2]
            );
            return HIDPP_OK;
        }

        break;

    case 0x2121: /* High resolution wheel */
        if (func == 0) {
            e->type = HIDPP_EVENT_WHEEL;
            e->as.wheel.flags = pkt->params[0];
            e->as.wheel.delta = HIDPP_WORD(pkt->params[1], pkt->params[2]);
            return HIDPP_OK;
        } else if (func == 1) {
            e->type = HIDPP_EVENT_RATCHET_SWITCH;
            e->as.ratchetSwitch = pkt->params[0];
            return HIDPP_OK;
        }

        break;

    case 0x2130: /* Ratchet */
        if (func == 0) {
            e->type = HIDPP_EVENT_RATCHET;
            e->as.ratchet.deltaV = pkt->params[0];
            e->as.ratchet.deltaH = pkt->params[1];
            return HIDPP_OK;
        }

        break;

    case 0x6110:
        if (func == 0) {
            e->type = HIDPP_EVENT_TOUCH_MOUSE_POINTS;
            struct hidpp_touch_mouse_point *p;

            for (int i = 0; i < 4; i++) {
                p = &e->as.touchMousePoints[i];
                uint8_t *params = &pkt->params[i * 4];

                uint16_t xHi = ((uint16_t)params[0]) << 4;
                uint16_t xLo = HIDPP_LSN(params[3]);
                uint16_t yHi = ((uint16_t)params[1]) << 4;
                uint16_t yLo = HIDPP_MSN(params[3]);

                p->x = xHi | xLo;
                p->y = yHi | yLo;
                p->wx = HIDPP_LSN(params[4]);
                p->wy = HIDPP_MSN(params[4]);
            }

            return HIDPP_OK;
        } else if (func == 1) {
            e->type = HIDPP_EVENT_TOUCH_MOUSE_STATUS;
            e->as.touchMouseStatus.flags = pkt->params[0];
            e->as.touchMouseStatus.mouseLifted = pkt->params[0] & 2;
            e->as.touchMouseStatus.buttonDown = pkt->params[0] & 1;
            return HIDPP_OK;
        }

        break;

    case 0x6100:
        if (func == 0) {
            e->type = HIDPP_EVENT_TOUCH_PAD_POINTS;
            e->as.touchPadPoints.timestamp = HIDPP_WORD(
                pkt->params[0], pkt->params[1]
            );

            struct hidpp_touch_pad_point *p;

            for (int i = 0; i < 2; i++) {
                p = &e->as.touchPadPoints.data[i];
                uint8_t *params = &pkt->params[i * 7 + 2];

                uint8_t type = (params[0] & 0xC0) >> 6;
                uint16_t xHi = (uint16_t)params[0] & 0x3F;
                uint16_t xLo = params[1];
                uint8_t status = (params[2] & 0xC0) >> 6;
                uint16_t yHi = (uint16_t)params[2] & 0x3F;
                uint16_t yLo = params[3];

                p->type = type;
                p->status = status;
                p->x = (xHi << 8) | xLo;
                p->y = (yHi << 8) | yLo;
                p->force = params[4];
                p->area = params[5];
                p->flags = params[6];
                p->finger = HIDPP_MSN(params[6]);
            }

            return HIDPP_OK;
        }

        break;

    default:
        break;
    }

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