/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

static const struct name_value effect_ids[] = {
    { "disabled", HIDPP_LED_DISABLED },
    { "fixed", HIDPP_LED_FIXED },
    { "pulsing", HIDPP_LED_PULSING },
    { "cycling", HIDPP_LED_CYCLING },
    { "wave", HIDPP_LED_WAVE },
    { "starlight", HIDPP_LED_STARLIGHT },
    { "light-on-press", HIDPP_LED_LIGHT_ON_PRESS },
    { "audio", HIDPP_LED_AUDIO },
    { "boot-up", HIDPP_LED_BOOT_UP },
    { "demo", HIDPP_LED_DEMO },
    { "breathing", HIDPP_LED_BREATHING },
    { "ripple", HIDPP_LED_RIPPLE },
    { "onboard", HIDPP_LED_ONBOARD },
    { NULL, 0 },
};

static const struct name_value persistence[] = {
    { "volatile", HIDPP_LED_VOLATILE },
    { "stored", HIDPP_LED_NON_VOLATILE },
    { "both", HIDPP_LED_VOLATILE | HIDPP_LED_NON_VOLATILE },
    { NULL, 0 },
};

static const struct name_value power_modes[] = {
    { "full", HIDPP_LED_POWER_FULL },
    { "save", HIDPP_LED_POWER_SAVE },
    { "off", HIDPP_LED_POWER_OFF },
    { NULL, 0 },
};

static const struct name_value controls[] = {
    { "effects", HIDPP_LED_SW_EFFECTS },
    { "power-modes", HIDPP_LED_SW_POWER_MODES },
    { NULL, 0 },
};

static const struct name_value notifications[] = {
    { "sync", HIDPP_LED_NOTIFY_SYNC },
    { "activity", HIDPP_LED_NOTIFY_ACTIVITY },
    { "timeout", HIDPP_LED_NOTIFY_TIMEOUT },
    { NULL, 0 },
};

static void print_color(
    hidppctl_t *ctl, const char *label, int r, int g, int b
) {
    pf_cli_printf(ctl->cli, " %s=%.2X%.2X%.2X", label, r, g, b);
}

static void print_params(hidppctl_t *ctl, int id, const uint8_t raw[10]) {
    struct hidpp_led_params p;

    if (hidpp_led_unpack(id, raw, &p)) {
        pf_cli_printf(ctl->cli, " raw=");
        cli_hex(ctl, raw, 10);
        return;
    }

    print_color(ctl, "color", p.red, p.green, p.blue);
    print_color(ctl, "color2", p.red2, p.green2, p.blue2);
    pf_cli_printf(
        ctl->cli,
        " period=%u speed=%u intensity=%u waveform=%u direction=%u ramp=%u "
        "delay=%u",
        p.period,
        p.speed,
        p.intensity,
        p.waveform,
        p.direction,
        p.ramp,
        p.delay
    );
}

static int led_show(hidppctl_t *ctl) {
    pf_cli_t *cli = ctl->cli;
    hidpp_device_t *dev = ctl->device;
    struct hidpp_led_info *info = hidpp_led_info(dev);
    struct hidpp_led_zone *zone;
    struct hidpp_led_effect_info *ef;

    if (!info)
        return cli_unsupported(ctl, "color LEDs");

    pf_cli_printf(cli, "Version: %u\n", info->version);
    pf_cli_printf(
        cli,
        "Kind: %s\n",
        info->isRgbEffects ? "RGB effects (0x8071)"
                           : "color LED effects (0x8070)"
    );
    pf_cli_printf(cli, "Zones: %u\n", info->zoneCount);
    pf_cli_printf(
        cli,
        "Has: zone effects %s, settings %s, sync %s, shutdown %s, changed "
        "events %s, monochrome %s\n",
        cli_yesno(info->hasZoneEffect),
        cli_yesno(info->hasSettings),
        cli_yesno(info->hasSync),
        cli_yesno(info->hasShutdown),
        cli_yesno(info->hasChangedEvents),
        cli_yesno(info->isMonochrome)
    );

    for (unsigned z = 0; z < info->zoneCount; z++) {
        if (!(zone = hidpp_led_zone(dev, z)))
            continue;

        pf_cli_printf(
            cli,
            "Zone %u: %s, %u effects\n",
            z,
            cli_str(zone->locationName),
            zone->effectCount
        );

        for (unsigned i = 0; i < zone->effectCount; i++) {
            if (!(ef = hidpp_led_effect_info(dev, z, i)))
                continue;

            pf_cli_printf(
                cli,
                "  [%u] %s (period %u ms)\n",
                i,
                cli_str(ef->name),
                ef->period
            );
        }
    }

    return HIDPP_OK;
}

static int led_get(hidppctl_t *ctl) {
    char **argv = ctl->options->paramv;
    struct hidpp_led_effect e;
    struct hidpp_led_effect_info *ef;
    int persist = HIDPP_LED_VOLATILE, rc;

    if (ctl->options->paramc > 2)
        return cli_usage(ctl, "Expected 'get [volatile|stored]'.");
    if (ctl->options->paramc == 2
        && cli_named(ctl, argv[1], "persistence", persistence, &persist))
        return HIDPP_EINVAL;

    if ((rc = hidpp_led_get_effect(
             ctl->device, ctl->options->zone, persist, &e
         )))
        return cli_fail(ctl, rc, "read the effect");

    ef = hidpp_led_effect_info(ctl->device, e.zone, e.index);
    pf_cli_printf(
        ctl->cli,
        "Zone %u effect %u (%s):",
        e.zone,
        e.index,
        ef ? cli_str(ef->name) : "unknown"
    );

    if (ef)
        print_params(ctl, ef->id, e.params);
    pf_cli_printf(ctl->cli, "\n");
    return HIDPP_OK;
}

static int led_set(hidppctl_t *ctl) {
    char **argv = ctl->options->paramv;
    int argc = ctl->options->paramc;
    struct hidpp_led_params p = { 0 };
    struct hidpp_led_effect e = { 0 };
    struct hidpp_color c;
    int id, index, rc, persist = HIDPP_LED_VOLATILE;
    long n;

    if (argc < 2)
        return cli_usage(
            ctl,
            "Expected 'set <effect> [<param>=<value>...]' where param is one "
            "of: "
            "color, color2, period, speed, intensity, waveform, direction, "
            "ramp, "
            "delay, persistence, power-mode."
        );

    if (cli_named(ctl, argv[1], "effect", effect_ids, &id))
        return HIDPP_EINVAL;

    for (int i = 2; i < argc; i++) {
        char *val, *key = cli_split(argv[i], &val);

        if (!val)
            return cli_usage(ctl, "Expected '%s=<value>'.", key);

        if (cli_is(key, "color") || cli_is(key, "color2")) {
            if (cli_color(ctl, val, &c))
                return HIDPP_EINVAL;
            if (cli_is(key, "color")) {
                p.red = c.red;
                p.green = c.green;
                p.blue = c.blue;
            } else {
                p.red2 = c.red;
                p.green2 = c.green;
                p.blue2 = c.blue;
            }
        } else if (cli_is(key, "period")) {
            if (cli_long(ctl, val, key, 0, 65535, &n))
                return HIDPP_EINVAL;
            p.period = n;
        } else if (cli_is(key, "speed")) {
            if (cli_long(ctl, val, key, 0, 255, &n))
                return HIDPP_EINVAL;
            p.speed = n;
        } else if (cli_is(key, "intensity")) {
            if (cli_long(ctl, val, key, 0, 100, &n))
                return HIDPP_EINVAL;
            p.intensity = n;
        } else if (cli_is(key, "waveform")) {
            if (cli_long(ctl, val, key, 0, 255, &n))
                return HIDPP_EINVAL;
            p.waveform = n;
        } else if (cli_is(key, "direction")) {
            if (cli_long(ctl, val, key, 0, 255, &n))
                return HIDPP_EINVAL;
            p.direction = n;
        } else if (cli_is(key, "ramp")) {
            if (cli_long(ctl, val, key, 0, 255, &n))
                return HIDPP_EINVAL;
            p.ramp = n;
        } else if (cli_is(key, "delay")) {
            if (cli_long(ctl, val, key, 0, 65535, &n))
                return HIDPP_EINVAL;
            p.delay = n;
        } else if (cli_is(key, "persistence")) {
            if (cli_named(ctl, val, key, persistence, &persist))
                return HIDPP_EINVAL;
        } else if (cli_is(key, "power-mode")) {
            if (cli_long(ctl, val, key, 0, 1, &n))
                return HIDPP_EINVAL;
            e.powerMode = n;
        } else {
            return cli_usage(ctl, "Unknown effect parameter '%s'.", key);
        }
    }

    if ((index = hidpp_led_find_effect(ctl->device, ctl->options->zone, id))
        < 0)
        return cli_fail(ctl, index, "find the effect in the zone");
    if ((rc = hidpp_led_pack(id, &p, e.params)))
        return cli_usage(
            ctl, "The parameters cannot be used with this effect."
        );

    e.zone = ctl->options->zone;
    e.index = index;
    e.persistence = persist;

    if ((rc = hidpp_led_set_effect(ctl->device, &e)))
        return cli_fail(ctl, rc, "play the effect");
    return HIDPP_OK;
}

static int led_power(hidppctl_t *ctl) {
    char **argv = ctl->options->paramv;
    int argc = ctl->options->paramc;
    struct hidpp_led_power_config cfg;
    int mode, rc;
    long n;

    if (argc == 1) {
        if ((rc = hidpp_led_power_mode(ctl->device)) < 0)
            return cli_fail(ctl, rc, "read the power mode");
        pf_cli_printf(
            ctl->cli, "Mode: %s\n", cli_str(cli_name(power_modes, rc))
        );

        if (!hidpp_led_power_config_get(ctl->device, &cfg)) {
            pf_cli_printf(
                ctl->cli, "To power save after: %u s\n", cfg.toPowerSave
            );
            pf_cli_printf(ctl->cli, "To off after: %u s\n", cfg.toOff);
            pf_cli_printf(ctl->cli, "(65535 means never)\n");
        }
        return HIDPP_OK;
    }

    if (argc == 2) {
        if (cli_named(ctl, argv[1], "power mode", power_modes, &mode))
            return HIDPP_EINVAL;
        if ((rc = hidpp_led_set_power_mode(ctl->device, mode)))
            return cli_fail(ctl, rc, "set the power mode");
        return HIDPP_OK;
    }

    if (argc == 4 && cli_is(argv[1], "timeouts")) {
        if ((rc = hidpp_led_power_config_get(ctl->device, &cfg)))
            return cli_fail(ctl, rc, "read the power configuration");
        if (cli_long(ctl, argv[2], "power save timeout", 0, 65535, &n))
            return HIDPP_EINVAL;
        cfg.toPowerSave = n;
        if (cli_long(ctl, argv[3], "off timeout", 0, 65535, &n))
            return HIDPP_EINVAL;
        cfg.toOff = n;
        if ((rc = hidpp_led_power_config_set(ctl->device, &cfg)))
            return cli_fail(ctl, rc, "set the power configuration");
        return HIDPP_OK;
    }

    return cli_usage(
        ctl,
        "Expected 'power', 'power <mode>' or 'power timeouts <save> <off>'."
    );
}

static int led_control(hidppctl_t *ctl) {
    char **argv = ctl->options->paramv;
    int argc = ctl->options->paramc;
    uint8_t control, notify;
    int c, nt, rc;
    char a[64], b[64];

    if (argc == 1) {
        if ((rc = hidpp_led_sw_control_get(ctl->device, &control, &notify)))
            return cli_fail(ctl, rc, "read the software control");

        cli_flag_names(controls, control, a, sizeof(a));
        cli_flag_names(notifications, notify, b, sizeof(b));
        pf_cli_printf(ctl->cli, "Software controls: %s\n", a);
        pf_cli_printf(ctl->cli, "Notifications: %s\n", b);
        return HIDPP_OK;
    }

    if (argc == 3) {
        if (cli_flags(ctl, argv[1], "control", controls, &c)
            || cli_flags(ctl, argv[2], "notification", notifications, &nt))
            return HIDPP_EINVAL;
        if ((rc = hidpp_led_sw_control_set(ctl->device, c, nt)))
            return cli_fail(ctl, rc, "change the software control");
        return HIDPP_OK;
    }

    return cli_usage(
        ctl, "Expected 'control' or 'control <controls> <notifications>'."
    );
}

int cmd_led(hidppctl_t *ctl) {
    pf_cli_t *cli = ctl->cli;
    hidpp_device_t *dev = ctl->device;
    char **argv = ctl->options->paramv;
    int argc = ctl->options->paramc;
    uint8_t rgb[3];
    long n;
    int rc;

    if (argc == 0 || cli_is(argv[0], "show"))
        return led_show(ctl);
    if (cli_is(argv[0], "get"))
        return led_get(ctl);
    if (cli_is(argv[0], "set"))
        return led_set(ctl);
    if (cli_is(argv[0], "power"))
        return led_power(ctl);
    if (cli_is(argv[0], "control"))
        return led_control(ctl);

    if (cli_is(argv[0], "shutdown") && argc == 1) {
        if ((rc = hidpp_led_shutdown(dev)))
            return cli_fail(ctl, rc, "shut the LEDs down");
        return HIDPP_OK;
    }

    if (cli_is(argv[0], "color") && argc == 1) {
        if ((rc = hidpp_led_current_color(dev, ctl->options->zone, rgb)))
            return cli_fail(ctl, rc, "read the color");
        pf_cli_printf(cli, "%.2X%.2X%.2X\n", rgb[0], rgb[1], rgb[2]);
        return HIDPP_OK;
    }

    if (cli_is(argv[0], "clear") && argc == 1) {
        if ((rc = hidpp_led_clear_settings(dev, ctl->options->zone)))
            return cli_fail(ctl, rc, "reset the zone");
        return HIDPP_OK;
    }

    if (cli_is(argv[0], "cycling") && argc == 2) {
        if (!cli_is(argv[1], "clockwise")
            && !cli_is(argv[1], "counter-clockwise"))
            return cli_usage(
                ctl, "Expected 'clockwise' or 'counter-clockwise'."
            );
        if ((rc = hidpp_led_cycling_direction(
                 dev, cli_is(argv[1], "counter-clockwise")
             )))
            return cli_fail(ctl, rc, "change the cycling direction");
        return HIDPP_OK;
    }

    if (cli_is(argv[0], "pattern") && argc == 2) {
        if (cli_long(ctl, argv[1], "pattern", 0, 255, &n))
            return HIDPP_EINVAL;
        if ((rc = hidpp_led_set_pattern(dev, ctl->options->zone, n)))
            return cli_fail(ctl, rc, "light the pattern");
        return HIDPP_OK;
    }

    return cli_usage(
        ctl,
        "Expected 'show', 'get', 'set', 'power', 'control', 'shutdown', "
        "'color', 'clear', 'cycling' or 'pattern'."
    );
}
