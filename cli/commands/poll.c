/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

#include <signal.h>

static volatile sig_atomic_t terminate = 0;

static void signal_handler(int sig) { terminate = 1; }

static int cmd_poll_handle(
    hidppctl_t *ctl, hidpp_device_t *dev, struct hidpp_event *e
) {
    pf_cli_t *cli = ctl->cli;
    pf_cli_cprintf(cli, PF_CLI_BOLD, "%s ", hidppctl_event_names[e->type]);

    switch (e->type) {
    case HIDPP_EVENT_UNKNOWN: {
        hidpp_packet_t *pkt = &e->as.unknown;
        size_t length;

        if (pkt->kind <= HIDPP_KIND_SHORT)
            length = HIDPP_LEN_SHORT;
        else if (pkt->kind <= HIDPP_KIND_LONG)
            length = HIDPP_LEN_LONG;
        else if (pkt->kind <= HIDPP_KIND_XLONG)
            length = HIDPP_LEN_XLONG;
        else
            return HIDPP_EINVAL;

        for (size_t i = 0; i < length; i++)
            pf_cli_printf(cli, "%.2x", ((uint8_t *)pkt)[i]);
        break;
    }

    case HIDPP_EVENT_BUTTON:
        for (int i = 0; i < 4; i++)
            pf_cli_printf(cli, " 0x%.4x", e->as.buttons[i]);
        break;

    case HIDPP_EVENT_MOUSE:
        pf_cli_printf(cli, "%u %u", e->as.mouse[0], e->as.mouse[1]);
        break;

    case HIDPP_EVENT_BATTERY:
        pf_cli_printf(
            cli,
            "%u %u %u %u",
            e->as.battery.chargeState,
            e->as.battery.batteryLevel,
            e->as.battery.chargeStatus,
            e->as.battery.externalPower
        );
        break;

    case HIDPP_EVENT_WHEEL:
        pf_cli_printf(cli, "%.2x %d", e->as.wheel.flags, e->as.wheel.delta);
        break;

    case HIDPP_EVENT_RATCHET:
        pf_cli_printf(cli, "%d %d", e->as.ratchet.deltaV, e->as.ratchet.deltaH);
        break;

    case HIDPP_EVENT_RATCHET_SWITCH:
        pf_cli_printf(cli, "%u", e->as.ratchetSwitch);
        break;

    case HIDPP_EVENT_TOUCH_PAD_POINTS:
        pf_cli_printf(cli, "%u ", e->as.touchPadPoints.timestamp);

        for (int i = 0; i < 4; i++) {
            struct hidpp_touch_pad_point *p = &e->as.touchPadPoints.data[i];
            pf_cli_printf(
                cli,
                "{%u,%u,%u,%u,%u,%u,%.2x,%u}",
                p->type,
                p->status,
                p->x,
                p->y,
                p->force,
                p->area,
                p->flags,
                p->finger
            );
        }

        break;

    case HIDPP_EVENT_TOUCH_MOUSE_POINTS:
        for (int i = 0; i < 4; i++) {
            struct hidpp_touch_mouse_point *p = &e->as.touchMousePoints[i];
            pf_cli_printf(cli, "{%u,%u,%u,%u}", p->x, p->y, p->wx, p->wy);
        }

        break;

    case HIDPP_EVENT_TOUCH_MOUSE_STATUS:
        pf_cli_printf(
            cli,
            "%u %u %u",
            e->as.touchMouseStatus.flags,
            e->as.touchMouseStatus.mouseLifted,
            e->as.touchMouseStatus.buttonDown
        );

        break;

    default:
        break;
    }

    pf_cli_printf(cli, "\n");
    return HIDPP_OK;
}

int cmd_poll(hidppctl_t *ctl) {
    char *masks = ctl->options->poll.masks;

    hidpp_device_t *dev = ctl->device;
    struct hidpp_event e;

    signal(SIGTERM, signal_handler);
    pf_cli_cprintf(ctl->cli, PF_CLI_BOLD, "Type Ctrl+C to stop the program.\n");

    while (!terminate) {
        if (HIDPP_OK != hidpp_device_poll(dev, &e))
            continue;

        if (e.type > HIDPP__EVENT_MAX || e.type < HIDPP_EVENT_NONE) {
            pf_cli_cprintf(ctl->cli, PF_FG_RED, "ERROR Invalid event type!");
            break;
        }

        if (masks[e.type] == HIDPP_TRUE)
            cmd_poll_handle(ctl, dev, &e);
    }

    return HIDPP_OK;
}