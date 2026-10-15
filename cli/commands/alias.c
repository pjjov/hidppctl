/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

#include <ctype.h>

static int find(
    const struct hidppctl_alias *list, int count, const char *name
) {
    for (int i = 0; i < count; i++)
        if (0 == strcmp(list[i].name, name))
            return i;

    return -1;
}

static int valid_name(const char *name) {
    if (!*name || strlen(name) >= sizeof(((struct hidppctl_alias *)0)->name))
        return 0;

    for (; *name; name++)
        if (!isalnum((unsigned char)*name) && !strchr("_-.", *name))
            return 0;

    return 1;
}

/* Builds an alias from the options which were given to the program. */
static int alias_from_options(hidppctl_t *ctl, struct hidppctl_alias *a) {
    struct hidppctl_opt *opt = ctl->options;
    const char *err;
    char value[32];

    if (opt->setReceiver
        && (err = hidppctl_alias_set(a, "receiver", opt->receiver)))
        return cli_usage(ctl, "receiver: %s.", err);
    if (opt->setSocket
        && (err = hidppctl_alias_set(a, "socket", opt->socket.value)))
        return cli_usage(ctl, "socket: %s.", err);

#define NUMBER(set, key, field)                             \
    if (set) {                                              \
        snprintf(value, sizeof(value), "%d", (int)(field)); \
        hidppctl_alias_set(a, key, value);                  \
    }
    NUMBER(opt->setDevice, "device", opt->device)
    NUMBER(opt->setInterface, "interface", opt->interface)
    NUMBER(opt->setSwid, "swid", opt->swid)
    NUMBER(opt->setTimeout, "timeout", opt->timeout)
#undef NUMBER

    return HIDPP_OK;
}

static int alias_set(hidppctl_t *ctl, struct hidppctl_alias *list, int *count) {
    char **argv = ctl->options->paramv;
    int argc = ctl->options->paramc;
    struct hidppctl_alias a;
    int index, rc;

    if (argc < 2)
        return cli_usage(ctl, "Expected 'set <name> [<key>=<value>...]'.");
    if (!valid_name(argv[1]))
        return cli_usage(
            ctl, "Alias names are 1 to 63 letters, digits, '_', '-' or '.'."
        );

    hidppctl_alias_init(&a, argv[1]);

    if ((rc = alias_from_options(ctl, &a)))
        return rc;

    for (int i = 2; i < argc; i++) {
        char *val, *key = cli_split(argv[i], &val);
        const char *err;

        if (!val)
            return cli_usage(ctl, "Expected '%s=<value>'.", key);
        if ((err = hidppctl_alias_set(&a, key, val)))
            return cli_usage(ctl, "%s: %s.", key, err);
    }

    if (!a.receiver[0] && !a.socket[0])
        return cli_usage(
            ctl, "An alias needs a receiver or a socket (-r, --socket)."
        );

    if ((index = find(list, *count, a.name)) < 0) {
        if (*count >= HIDPPCTL_MAX_ALIASES)
            return cli_usage(ctl, "Too many aliases.");
        index = (*count)++;
    }

    list[index] = a;
    return hidppctl_alias_save(ctl, list, *count);
}

int cmd_alias(hidppctl_t *ctl) {
    char **argv = ctl->options->paramv;
    int argc = ctl->options->paramc;
    static struct hidppctl_alias list[HIDPPCTL_MAX_ALIASES];
    char line[1024];
    int count, index, rc;

    if ((rc = hidppctl_alias_load(ctl, list, HIDPPCTL_MAX_ALIASES, &count)))
        return rc;

    if (argc == 0 || cli_is(argv[0], "list")) {
        if (count == 0)
            pf_cli_printf(ctl->cli, "No aliases are defined.\n");

        for (int i = 0; i < count; i++) {
            hidppctl_alias_format(&list[i], line, sizeof(line));
            pf_cli_printf(ctl->cli, "%s:%s\n", list[i].name, line);
        }

        return HIDPP_OK;
    }

    if (cli_is(argv[0], "set"))
        return alias_set(ctl, list, &count);

    if ((cli_is(argv[0], "show") || cli_is(argv[0], "remove")) && argc == 2) {
        if ((index = find(list, count, argv[1])) < 0)
            return cli_usage(ctl, "Unknown alias '%s'.", argv[1]);

        if (cli_is(argv[0], "show")) {
            hidppctl_alias_format(&list[index], line, sizeof(line));
            pf_cli_printf(ctl->cli, "%s:%s\n", list[index].name, line);
            return HIDPP_OK;
        }

        memmove(
            &list[index],
            &list[index + 1],
            (count - index - 1) * sizeof(list[0])
        );
        return hidppctl_alias_save(ctl, list, count - 1);
    }

    return cli_usage(
        ctl,
        "Expected 'list', 'show <name>', 'set <name> ...' or 'remove <name>'."
    );
}
