/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

static int cmd_poll_handle(
    hidppctl_t *ctl, hidpp_device_t *dev, struct hidpp_event *e
) {
    pf_cli_t *cli = ctl->cli;
    pf_cli_cprintf(cli, PF_CLI_BOLD, "%s ", hidppctl_event_names[e->type]);
    pf_cli_color(cli, PF_CLI_RESET);

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
        pf_cli_printf(cli, "%d %d", e->as.mouse[0], e->as.mouse[1]);
        break;

    case HIDPP_EVENT_ANALYTICS:
        for (int i = 0; i < 5; i++)
            if (e->as.analytics[i].id)
                pf_cli_printf(
                    cli,
                    " 0x%.4x:%u",
                    e->as.analytics[i].id,
                    e->as.analytics[i].event
                );
        break;

    case HIDPP_EVENT_RAW_WHEEL:
        pf_cli_printf(
            cli,
            "%d %u %s",
            e->as.rawWheel.delta,
            e->as.rawWheel.periods,
            e->as.rawWheel.highRes ? "high" : "low"
        );
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

        for (int i = 0; i < 2; i++) {
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

    case HIDPP_EVENT_BACKLIGHT:
        pf_cli_printf(
            cli,
            "levels=%u level=%u status=%u effect=%u",
            e->as.backlight.levelCount,
            e->as.backlight.level,
            e->as.backlight.status,
            e->as.backlight.effect
        );
        break;

    case HIDPP_EVENT_ILLUMINATION:
        pf_cli_printf(
            cli,
            "kind=%u state=%u source=%u value=%u",
            e->as.illumination.kind,
            e->as.illumination.state,
            e->as.illumination.source,
            e->as.illumination.value
        );
        break;

    case HIDPP_EVENT_WIRELESS_STATUS:
        pf_cli_printf(
            cli,
            "status=%u request=%u reason=%u",
            e->as.wirelessStatus.status,
            e->as.wirelessStatus.request,
            e->as.wirelessStatus.reason
        );
        break;

    case HIDPP_EVENT_THUMBWHEEL:
        pf_cli_printf(
            cli,
            "rotation=%d time=%u state=%u touch=%d proximity=%d tap=%d",
            e->as.thumbwheel.rotation,
            e->as.thumbwheel.timestamp,
            e->as.thumbwheel.status,
            e->as.thumbwheel.touch,
            e->as.thumbwheel.proxy,
            e->as.thumbwheel.singleTap
        );
        break;

    case HIDPP_EVENT_DPI:
        pf_cli_printf(
            cli,
            "sensor=%u x=%u y=%u lod=%u",
            e->as.dpi.sensor,
            e->as.dpi.x,
            e->as.dpi.y,
            e->as.dpi.lod
        );
        break;

    case HIDPP_EVENT_DPI_CALIBRATION:
        pf_cli_printf(
            cli,
            "sensor=%u direction=%u correction=%d delta=%d",
            e->as.dpiCalibration.sensor,
            e->as.dpiCalibration.direction,
            e->as.dpiCalibration.correction,
            e->as.dpiCalibration.delta
        );
        break;

    case HIDPP_EVENT_FN_INVERSION:
        pf_cli_printf(
            cli,
            "host=%u enabled=%d default=%d fn-lock=%d",
            e->as.fnInversion.host,
            e->as.fnInversion.enabled,
            e->as.fnInversion.defaultEnabled,
            e->as.fnInversion.hasFnLock
        );
        break;

    case HIDPP_EVENT_SOLAR:
        pf_cli_printf(
            cli,
            "kind=%u battery=%u%% lux=%u",
            e->as.solar.kind,
            e->as.solar.battery,
            e->as.solar.lux
        );
        break;

    case HIDPP_EVENT_PLATFORM:
        pf_cli_printf(
            cli,
            "host=%u platform=%u source=%u",
            e->as.platform.host,
            e->as.platform.platform,
            e->as.platform.source
        );
        break;

    case HIDPP_EVENT_CROWN:
        pf_cli_printf(
            cli,
            "rotation=%u slots=%d ratchets=%d proximity=%u touch=%u gesture=%u "
            "button=%u speed=%d",
            e->as.crown.rotation,
            e->as.crown.slots,
            e->as.crown.ratchets,
            e->as.crown.proximity,
            e->as.crown.touch,
            e->as.crown.gesture,
            e->as.crown.button,
            e->as.crown.speed
        );
        break;

    case HIDPP_EVENT_BRIGHTNESS:
        pf_cli_printf(
            cli,
            "kind=%u value=%u",
            e->as.brightness.kind,
            e->as.brightness.value
        );
        break;

    case HIDPP_EVENT_REPORT_RATE:
        pf_cli_printf(
            cli,
            "connection=%u interval=%uus",
            e->as.reportRate.connection,
            e->as.reportRate.rate
        );
        break;

    case HIDPP_EVENT_LED:
        pf_cli_printf(
            cli,
            "kind=%u zone=%u effect=%u activity=%u counter=%u persistence=%u",
            e->as.led.kind,
            e->as.led.zone,
            e->as.led.effect,
            e->as.led.activity,
            e->as.led.counter,
            e->as.led.persistence
        );
        break;

    case HIDPP_EVENT_MODE_STATUS:
        pf_cli_printf(
            cli,
            "status=0x%.4x changed=0x%.4x",
            e->as.modeStatus.status,
            e->as.modeStatus.changed
        );
        break;

    case HIDPP_EVENT_SIDETONE:
        pf_cli_printf(
            cli,
            "channel=%u level=%u mute=0x%.2x",
            e->as.sidetone.channel,
            e->as.sidetone.level,
            e->as.sidetone.mute
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
    hidpp_receiver_t *rcv = ctl->receiver;
    struct hidpp_event e;
    int rc;

    cli_catch_signals();
    pf_cli_cprintf(ctl->cli, PF_CLI_BOLD, "Type Ctrl+C to stop the program.\n");
    pf_cli_color(ctl->cli, PF_CLI_RESET);

    while (!cli_terminated()) {
        /* Without a device, the receiver itself is polled. */
        rc = dev ? hidpp_device_poll(dev, &e) : hidpp_poll(rcv, &e);

        if (rc == HIDPP_ETIMEDOUT || rc == HIDPP_EAGAIN || rc == HIDPP_EINTR)
            continue;

        if (rc != HIDPP_OK) {
            pf_cli_errorf(
                ctl->cli, "Unable to poll for events: %s\n", hidpp_error_str(rc)
            );
            return rc;
        }

        if (e.type >= HIDPP__EVENT_MAX || e.type < HIDPP_EVENT_NONE) {
            pf_cli_errorf(ctl->cli, "Invalid event type %d!\n", e.type);
            return HIDPP_EINVAL;
        }

        if (masks[e.type] == HIDPP_TRUE)
            cmd_poll_handle(ctl, dev, &e);
    }

    return HIDPP_OK;
}
