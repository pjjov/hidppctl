/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

static const struct name_value types[] = {
    { "none", HIDPP_ACTION_NONE },
    { "key", HIDPP_ACTION_KEY },
    { "mouse-button", HIDPP_ACTION_MOUSE_BUTTON },
    { "x", HIDPP_ACTION_X_DISPLACEMENT },
    { "y", HIDPP_ACTION_Y_DISPLACEMENT },
    { "vertical-roller", HIDPP_ACTION_VERTICAL_ROLLER },
    { "horizontal-roller", HIDPP_ACTION_HORIZONTAL_ROLLER },
    { "consumer", HIDPP_ACTION_CONSUMER },
    { "internal", HIDPP_ACTION_INTERNAL },
    { "power", HIDPP_ACTION_POWER },
    { NULL, 0 },
};

static const struct name_value modifiers[] = {
    { "lctrl", HIDPP_MOD_LEFT_CTRL },
    { "lshift", HIDPP_MOD_LEFT_SHIFT },
    { "lalt", HIDPP_MOD_LEFT_ALT },
    { "lmeta", HIDPP_MOD_LEFT_META },
    { "rctrl", HIDPP_MOD_RIGHT_CTRL },
    { "rshift", HIDPP_MOD_RIGHT_SHIFT },
    { "ralt", HIDPP_MOD_RIGHT_ALT },
    { "rmeta", HIDPP_MOD_RIGHT_META },
    { NULL, 0 },
};

static void print_action(hidppctl_t *ctl, const struct hidpp_action *a) {
    char mods[128];
    cli_flag_names(modifiers, a->modifiers, mods, sizeof(mods));

    pf_cli_printf(
        ctl->cli,
        "  [0x%.4x] %s: %s %u",
        a->id,
        cli_str(a->name),
        cli_str(a->typeName),
        a->value
    );

    if (a->type == HIDPP_ACTION_KEY)
        pf_cli_printf(ctl->cli, " (modifiers: %s)", mods);
    pf_cli_printf(ctl->cli, "%s\n", a->isRemapped ? "" : " [default]");
}

static int action_list(hidppctl_t *ctl) {
    hidpp_device_t *dev = ctl->device;
    struct hidpp_action_info *info = hidpp_action_info(dev);
    uint16_t ids[256];
    struct hidpp_action a;

    if (!info)
        return cli_unsupported(ctl, "persistent actions");

    pf_cli_printf(
        ctl->cli, "Controls: %u, hosts: %u\n", info->count, info->hostCount
    );
    pf_cli_printf(
        ctl->cli,
        "Can perform: key %s, mouse button %s, X %s, Y %s, vertical roller %s, "
        "horizontal roller %s, consumer %s, internal %s, power %s\n",
        cli_yesno(info->canKey),
        cli_yesno(info->canMouseButton),
        cli_yesno(info->canXDisplacement),
        cli_yesno(info->canYDisplacement),
        cli_yesno(info->canVerticalRoller),
        cli_yesno(info->canHorizontalRoller),
        cli_yesno(info->canConsumer),
        cli_yesno(info->canInternal),
        cli_yesno(info->canPower)
    );

    size_t count = hidpp_action_list(dev, ids, 256);
    pf_cli_printf(ctl->cli, "Actions of host %u:\n", ctl->options->host);

    for (size_t i = 0; i < count; i++)
        if (!hidpp_action_get(dev, ids[i], ctl->options->host, &a))
            print_action(ctl, &a);

    return HIDPP_OK;
}

static int action_set(hidppctl_t *ctl) {
    char **argv = ctl->options->paramv;
    int argc = ctl->options->paramc;
    uint16_t id;
    int type, mods = 0, rc;
    long value = 0;

    if (argc < 3 || argc > 5)
        return cli_usage(
            ctl, "Expected 'set <control> <type> [value] [modifiers]'."
        );
    if (cli_control(ctl, argv[1], &id))
        return HIDPP_EINVAL;
    if (cli_named(ctl, argv[2], "action type", types, &type))
        return HIDPP_EINVAL;

    if (argc >= 4 && cli_long(ctl, argv[3], "value", -32768, 65535, &value))
        return HIDPP_EINVAL;
    if (argc == 5 && cli_flags(ctl, argv[4], "modifier", modifiers, &mods))
        return HIDPP_EINVAL;

    rc = hidpp_action_set(
        ctl->device, id, ctl->options->host, type, (uint16_t)value, mods
    );

    if (rc)
        return cli_fail(ctl, rc, "set the action");
    return HIDPP_OK;
}

static int action_reset(hidppctl_t *ctl) {
    char **argv = ctl->options->paramv;
    int argc = ctl->options->paramc;
    uint16_t id;
    int rc;

    if (argc != 2)
        return cli_usage(ctl, "Expected 'reset <control>|all'.");

    if (cli_is(argv[1], "all")) {
        /* Without --host every host is reset. */
        uint8_t mask = ctl->options->host == HIDPP_HOST_CURRENT
            ? 0xFF
            : 1u << ctl->options->host;

        if ((rc = hidpp_action_reset_all(ctl->device, mask)))
            return cli_fail(ctl, rc, "reset the actions");
        return HIDPP_OK;
    }

    if (cli_control(ctl, argv[1], &id))
        return HIDPP_EINVAL;
    if ((rc = hidpp_action_reset(ctl->device, id, ctl->options->host)))
        return cli_fail(ctl, rc, "reset the action");
    return HIDPP_OK;
}

int cmd_action(hidppctl_t *ctl) {
    char **argv = ctl->options->paramv;
    int argc = ctl->options->paramc;
    struct hidpp_action a;
    uint16_t id;
    int rc;

    if (argc == 0 || cli_is(argv[0], "list"))
        return action_list(ctl);
    if (cli_is(argv[0], "set"))
        return action_set(ctl);
    if (cli_is(argv[0], "reset"))
        return action_reset(ctl);

    if (cli_is(argv[0], "get") && argc == 2) {
        if (cli_control(ctl, argv[1], &id))
            return HIDPP_EINVAL;
        if ((rc = hidpp_action_get(ctl->device, id, ctl->options->host, &a)))
            return cli_fail(ctl, rc, "read the action");
        print_action(ctl, &a);
        return HIDPP_OK;
    }

    return cli_usage(
        ctl, "Expected 'list', 'get <control>', 'set ...' or 'reset ...'."
    );
}
