/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

static const struct name_value modes[] = {
    { "none", HIDPP_BACKLIGHT_MODE_NONE },
    { "auto", HIDPP_BACKLIGHT_MODE_AUTO },
    { "temp-manual", HIDPP_BACKLIGHT_MODE_TEMP_MANUAL },
    { "perm-manual", HIDPP_BACKLIGHT_MODE_PERM_MANUAL },
    { NULL, 0 },
};

static const struct name_value effects[] = {
    { "static", HIDPP_BACKLIGHT_EFFECT_STATIC },
    { "none", HIDPP_BACKLIGHT_EFFECT_NONE },
    { "breathing", HIDPP_BACKLIGHT_EFFECT_BREATHING },
    { "contrast", HIDPP_BACKLIGHT_EFFECT_CONTRAST },
    { "reaction", HIDPP_BACKLIGHT_EFFECT_REACTION },
    { "random", HIDPP_BACKLIGHT_EFFECT_RANDOM },
    { "waves", HIDPP_BACKLIGHT_EFFECT_WAVES },
    { NULL, 0 },
};

static int backlight_show(hidppctl_t *ctl) {
    pf_cli_t *cli = ctl->cli;
    hidpp_device_t *dev = ctl->device;
    struct hidpp_backlight_info *info = hidpp_backlight_info(dev);
    struct hidpp_backlight_config cfg;
    struct hidpp_backlight_state st;
    char buf[128];

    if (!info)
        return cli_unsupported(ctl, "a backlight");

    cli_flag_names(effects, info->effectList, buf, sizeof(buf));

    pf_cli_printf(cli, "Version: %u\n", info->version);
    pf_cli_printf(cli, "Levels: %u\n", info->levelCount);
    pf_cli_printf(cli, "Available effects: %s\n", buf);
    pf_cli_printf(
        cli,
        "Supports: wow %s, crown %s, power save %s, auto %s, "
        "temporary manual %s, permanent manual %s\n",
        cli_yesno(info->supportsWow),
        cli_yesno(info->supportsCrown),
        cli_yesno(info->supportsPowerSave),
        cli_yesno(info->supportsAutoMode),
        cli_yesno(info->supportsTempManualMode),
        cli_yesno(info->supportsPermManualMode)
    );

    if (info->hasDurations) {
        pf_cli_printf(
            cli,
            "Default durations: hands out %us, hands in %us, powered %us\n",
            info->defaultDurationHandsOut,
            info->defaultDurationHandsIn,
            info->defaultDurationPowered
        );
    }

    if (!hidpp_backlight_get(dev, &cfg)) {
        pf_cli_printf(cli, "Configuration:\n");
        pf_cli_printf(cli, "  Enabled: %s\n", cli_yesno(cfg.enabled));
        pf_cli_printf(cli, "  Effect at power on: %s\n", cli_onoff(cfg.wow));
        pf_cli_printf(
            cli, "  Effect on crown touch: %s\n", cli_onoff(cfg.crown)
        );
        pf_cli_printf(cli, "  Power save: %s\n", cli_onoff(cfg.powerSave));
        pf_cli_printf(cli, "  Mode: %s\n", cli_str(cli_name(modes, cfg.mode)));
        pf_cli_printf(
            cli, "  Effect: %s\n", cli_str(cli_name(effects, cfg.effect))
        );
        pf_cli_printf(cli, "  Level: %u\n", cfg.level);
        pf_cli_printf(
            cli,
            "  Durations: hands out %us, hands in %us, powered %us\n",
            cfg.durationHandsOut,
            cfg.durationHandsIn,
            cfg.durationPowered
        );
    }

    if (!hidpp_backlight_state(dev, &st)) {
        pf_cli_printf(cli, "Right now:\n");
        pf_cli_printf(cli, "  Status: %s\n", cli_str(st.statusName));
        pf_cli_printf(cli, "  Effect: %s\n", cli_str(st.effectName));
        pf_cli_printf(cli, "  Level: %u of %u\n", st.level, st.levelCount);
    }

    return HIDPP_OK;
}

static int backlight_set(hidppctl_t *ctl) {
    hidpp_device_t *dev = ctl->device;
    struct hidpp_backlight_config cfg;
    char **argv = ctl->options->paramv;
    int rc, v;
    long n;

    if (ctl->options->paramc < 2)
        return cli_usage(
            ctl,
            "Expected 'set <option>=<value>...' where option is one of: "
            "enabled, wow, crown, power-save, mode, effect, level, "
            "hands-out, hands-in, powered."
        );

    if ((rc = hidpp_backlight_get(dev, &cfg)))
        return cli_fail(ctl, rc, "read the backlight configuration");

    for (int i = 1; i < ctl->options->paramc; i++) {
        char *val, *key = cli_split(argv[i], &val);

        if (!val)
            return cli_usage(ctl, "Expected '%s=<value>'.", key);

        if (cli_is(key, "enabled")) {
            if (cli_bool(ctl, val, key, &v))
                return HIDPP_EINVAL;
            cfg.enabled = v;
        } else if (cli_is(key, "wow")) {
            if (cli_bool(ctl, val, key, &v))
                return HIDPP_EINVAL;
            cfg.wow = v;
        } else if (cli_is(key, "crown")) {
            if (cli_bool(ctl, val, key, &v))
                return HIDPP_EINVAL;
            cfg.crown = v;
        } else if (cli_is(key, "power-save")) {
            if (cli_bool(ctl, val, key, &v))
                return HIDPP_EINVAL;
            cfg.powerSave = v;
        } else if (cli_is(key, "mode")) {
            if (cli_named(ctl, val, key, modes, &v))
                return HIDPP_EINVAL;
            cfg.mode = v;
        } else if (cli_is(key, "effect")) {
            if (cli_named(ctl, val, key, effects, &v))
                return HIDPP_EINVAL;
            cfg.effect = v;
        } else if (cli_is(key, "level")) {
            if (cli_long(ctl, val, key, 0, 255, &n))
                return HIDPP_EINVAL;
            cfg.level = n;
        } else if (cli_is(key, "hands-out")) {
            if (cli_long(ctl, val, key, 0, 7200, &n))
                return HIDPP_EINVAL;
            cfg.durationHandsOut = n;
        } else if (cli_is(key, "hands-in")) {
            if (cli_long(ctl, val, key, 0, 7200, &n))
                return HIDPP_EINVAL;
            cfg.durationHandsIn = n;
        } else if (cli_is(key, "powered")) {
            if (cli_long(ctl, val, key, 0, 7200, &n))
                return HIDPP_EINVAL;
            cfg.durationPowered = n;
        } else {
            return cli_usage(ctl, "Unknown backlight option '%s'.", key);
        }
    }

    if ((rc = hidpp_backlight_set(dev, &cfg)))
        return cli_fail(ctl, rc, "write the backlight configuration");
    return HIDPP_OK;
}

int cmd_backlight(hidppctl_t *ctl) {
    char **argv = ctl->options->paramv;
    int rc, effect;

    if (ctl->options->paramc == 0 || cli_is(argv[0], "show"))
        return backlight_show(ctl);

    if (cli_is(argv[0], "set"))
        return backlight_set(ctl);

    if (cli_is(argv[0], "effect") && ctl->options->paramc == 2) {
        if (cli_named(ctl, argv[1], "effect", effects, &effect))
            return HIDPP_EINVAL;
        if ((rc = hidpp_backlight_set_effect(ctl->device, effect)))
            return cli_fail(ctl, rc, "apply the backlight effect");
        return HIDPP_OK;
    }

    return cli_usage(
        ctl, "Expected 'show', 'set <option>=<value>...' or 'effect <name>'."
    );
}
