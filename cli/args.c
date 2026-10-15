/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "common.h"

#include <errno.h>
#include <pf_cli.h>

#define PF_ARGPARSE_QUIET_SHORT 0
#include <pf_argparse.h>

#define COMMON_OPTIONS                                                      \
    (PF_COMMON_HELP | PF_COMMON_VERBOSE | PF_COMMON_QUIET | PF_COMMON_COLOR \
     | PF_COMMON_INTERACTIVE | PF_COMMON_VERSION)

static const char *help_epilog = "For more information, run `man hidppctl.1'.";

/* The common options (help, verbose, ...) are listed by the library. */
static const pf_cli_definition_t help_options[] = {
    { "-r, --receiver", "Specifies which HID++ receiver to use." },
    { "-d, --device", "Specifies which HID++ device index to use." },
    { "--host",
      "Selects a host (a number or 'current') for host-specific commands." },
    { "--sensor", "Selects a motion sensor for the 'dpi' command." },
    { "--zone",
      "Selects a LED zone for the 'led' command (255 is all zones)." },
    { "--alias", "Uses the options saved by the 'alias' command." },
    { "--alias-file", "Reads and writes aliases to the given file." },
    { "--interface", "Specifies which HID interface to use." },
    { "--socket",
      "Uses a Unix socket path, a TCP port or <host>:<port> as a receiver." },
    { "--timeout", "Sets the timeout for IO operations in milliseconds." },
    { "--swid", "Sets the software ID for interacting with devices." },
    { "--cache-path", "Sets a path to use for saving the device cache." },
    { "--log-path", "Logs the device traffic to the given file." },
    { "--no-cache", "Disables cache file saving and loading." },
    { "--no-log", "Disables file logging, even with -v." },
    { 0 },
};

static const struct hidppctl_command_spec *find_command(const char *name) {
    for (int i = 1; i < hidppctl_command_max; i++)
        if (0 == strcmp(name, hidppctl_commands[i].name))
            return &hidppctl_commands[i];

    return NULL;
}

int hidppctl_print_help(hidppctl_t *ctl) {
    pf_cli_t *cli = ctl->cli;
    const struct hidppctl_command_spec
        *spec = &hidppctl_commands[ctl->options->command];

    if (ctl->options->command == HIDPPCTL_NONE)
        pf_cli_printf(cli, "usage: hidppctl [OPTIONS]... <command>\n\n");
    else
        pf_cli_printf(
            cli,
            "usage: hidppctl [OPTIONS]... %s %s\n\n",
            spec->name,
            spec->args
        );

    pf_cli_printf(cli, "%s\n", spec->help);

    pf_cli_help_section(cli, "Options:");
    pf_argparser_help_options(ctl->argparser, COMMON_OPTIONS);
    pf_cli_help_definitions(cli, help_options);

    if (ctl->options->command == HIDPPCTL_NONE) {
        pf_cli_definition_t defs[hidppctl_command_max] = { 0 };

        for (int i = 1; i < hidppctl_command_max; i++) {
            defs[i - 1].name = hidppctl_commands[i].name;
            defs[i - 1].description = hidppctl_commands[i].summary;
        }

        pf_cli_help_section(cli, "Subcommands:");
        pf_cli_help_definitions(cli, defs);
    }

    pf_cli_printf(cli, "\n%s\n", help_epilog);
    return HIDPP_OK;
}

/* ------------------------------------------------------------------------ */
/* Argument conversion                                                       */
/* ------------------------------------------------------------------------ */

static int parse_u16(
    pf_argparser_t *p, const char *arg, const char *what, uint16_t *out
) {
    long value;

    if (!pf_arg_long(p, arg, what, 0, UINT16_MAX, &value))
        return HIDPP_EINVAL;

    *out = (uint16_t)value;
    return HIDPP_OK;
}

/* A button is either a number or a name known to the keymap. */
static int parse_diversion_ctrl(pf_argparser_t *p, char *str, uint16_t *out) {
    char *end;
    long ctrl = strtol(str, &end, 0);

    if (end == str || *end) {
        ctrl = hidpp_keymap_from_name(str);

        if (ctrl == 0) {
            pf_argparser_error(p, "Unknown control name '%s'", str);
            return HIDPP_EINVAL;
        }
    }

    if (ctrl < 0 || ctrl > UINT16_MAX) {
        pf_argparser_error(
            p,
            "Control codes must be between 0 and %u; got %ld",
            (unsigned)UINT16_MAX,
            ctrl
        );
        return HIDPP_EINVAL;
    }

    *out = (uint16_t)ctrl;
    return HIDPP_OK;
}

/* "<key>[+<modifier>]..." */
static int parse_diversion_keys(
    pf_argparser_t *p, struct hidppctl_opt *o, char *str, int i
) {
    char *tok;
    int count = -1; /* -1: the key itself has not been seen yet */

    while ((tok = pf_next_token(&str, '+'))) {
        int key;

        if (count >= HIDPP_MAX_MODS) {
            pf_argparser_error(
                p, "Too many modifiers; maximum is %d.", HIDPP_MAX_MODS
            );
            return HIDPP_ENOMEM;
        }

        key = hidpp_input_key(tok);

        if (key < 0) {
            pf_argparser_error(p, "Unknown key code '%s'.", tok);
            return HIDPP_EINVAL;
        }

        if (count == -1)
            o->divert.items[i].key = key;
        else
            o->divert.items[i].mods[count] = key;

        count++;
    }

    o->divert.items[i].count = count;
    return HIDPP_OK;
}

/* "<button>[=<key>[+<modifier>]...]" */
static int parse_diversion(
    pf_argparser_t *p, struct hidppctl_opt *o, char *arg, int i
) {
    char *rest = arg;
    char *ctrl = pf_next_token(&rest, '=');

    if (parse_diversion_ctrl(p, ctrl, &o->divert.items[i].ctrl))
        return HIDPP_EINVAL;

    if (rest && parse_diversion_keys(p, o, rest, i))
        return HIDPP_EINVAL;

    return HIDPP_OK;
}

static int parse_poll_event_name(pf_argparser_t *p, const char *arg) {
    for (int i = 1; i < HIDPP__EVENT_MAX; i++)
        if (0 == strcmp(arg, hidppctl_event_names[i]))
            return i;

    pf_argparser_error(p, "Unknown event name '%s'.", arg);
    return 0;
}

/* "<port>", "<host>:<port>" or a path of a Unix domain socket. */
static int parse_socket_path(pf_argparser_t *p, struct hidppctl_opt *opt) {
    char *str = opt->socket.value;
    char *colon = strrchr(str, ':');
    const char *portStr = str;
    char *end;

    opt->socket.isPort = HIDPP_FALSE;
    opt->socket.path = str;

    /* A colon separates a host from a port unless it is part of a path. */
    if (colon && !strchr(str, '/') && colon[1]) {
        portStr = colon + 1;
        size_t length = (size_t)(colon - str);

        if (length == 0 || length >= sizeof(opt->socket.host)) {
            pf_argparser_error(p, "Invalid socket host in '%s'.", str);
            return HIDPP_EINVAL;
        }

        /* "[::1]:1234" */
        if (str[0] == '[' && colon[-1] == ']') {
            memcpy(opt->socket.host, str + 1, length - 2);
            opt->socket.host[length - 2] = '\0';
        } else {
            memcpy(opt->socket.host, str, length);
            opt->socket.host[length] = '\0';
        }
    } else {
        strcpy(opt->socket.host, "localhost");
    }

    errno = 0;
    long v = strtol(portStr, &end, 10);

    if (end == portStr || *end) {
        if (portStr != str) {
            pf_argparser_error(p, "Invalid socket port in '%s'.", str);
            return HIDPP_EINVAL;
        }

        return HIDPP_OK; /* a path */
    }

    if (errno == ERANGE || v < 1 || v > 65535) {
        pf_argparser_error(
            p, "Socket's port number must be between 1 and 65535."
        );
        return HIDPP_EINVAL;
    }

    opt->socket.isPort = HIDPP_TRUE;
    opt->socket.port = (int)v;
    return HIDPP_OK;
}

/* ------------------------------------------------------------------------ */
/* The callback: options and the subcommand name                             */
/* ------------------------------------------------------------------------ */

static int parse_args_start(struct pf_argparser *p, struct hidppctl_opt *o) {
    memset(o, 0, sizeof(*o));
    o->host = HIDPP_HOST_CURRENT;
    return HIDPP_OK;
}

static void parse_args_option(
    struct pf_argparser *p, struct hidppctl_opt *opt
) {
    int dev, swid, sensor, zone;
    char *host;

    /* --help, --verbose, --quiet, --color, --interactive */
    if (pf_option_common(p, COMMON_OPTIONS))
        return;

    if (pf_option_int(p, "interface", 0, &opt->interface))
        opt->setInterface = HIDPP_TRUE;
    if (pf_option_int(p, "timeout", 0, &opt->timeout))
        opt->setTimeout = HIDPP_TRUE;

    if (pf_option_string(p, "receiver", 'r', &opt->receiver)) {
        opt->setSocket = HIDPP_FALSE;
        opt->setReceiver = HIDPP_TRUE;
    }

    if (pf_option_int(p, "device", 'd', &dev)) {
        if (dev < 1 || (dev > 6 && dev != 0xFF)) {
            pf_argparser_error(p, "Invalid device index %d.", dev);
        } else {
            opt->device = dev;
            opt->setDevice = HIDPP_TRUE;
        }
    }

    if (pf_option_string(p, "host", 0, &host)) {
        char *end;
        long v = strtol(host, &end, 0);

        if (0 == strcmp(host, "current")) {
            opt->host = HIDPP_HOST_CURRENT;
        } else if (end == host || *end || v < 0 || v > 0xFE) {
            pf_argparser_error(
                p,
                "Host must be 'current' or between 0 and 254; got '%s'.",
                host
            );
        } else {
            opt->host = (uint8_t)v;
        }
    }

    if (pf_option_int_range(p, "sensor", 0, 0, 255, &sensor))
        opt->sensor = (uint8_t)sensor;
    if (pf_option_int_range(p, "zone", 0, 0, 255, &zone))
        opt->zone = (uint8_t)zone;

    if (pf_option_int_range(p, "swid", 0, 0, 15, &swid)) {
        opt->swid = swid;
        opt->setSwid = HIDPP_TRUE;
    }

    if (pf_option_string(p, "socket", 0, &opt->socket.value)) {
        opt->setReceiver = HIDPP_FALSE;
        opt->setSocket = HIDPP_TRUE;
    }

    pf_option_string(p, "alias", 0, &opt->aliasName);
    pf_option_string(p, "alias-file", 0, &opt->aliasFile);
    pf_option_string(p, "cache-path", 0, &opt->cache.path);
    pf_option_string(p, "log-path", 0, &opt->log.path);
    pf_option_bool(p, "no-cache", 0, &opt->cache.disabled);
    pf_option_bool(p, "no-log", 0, &opt->log.disabled);
}

static void parse_args_param(struct pf_argparser *p, struct hidppctl_opt *opt) {
    const struct hidppctl_command_spec *spec;

    /* The first parameter is the subcommand; it is not kept in paramv. */
    if (p->subcommandSeen || !pf_is_param(p, -1))
        return;

    p->subcommandSeen = PF_TRUE;

    if (!(spec = find_command(p->item.param))) {
        pf_argparser_error(p, "Unknown subcommand '%s'.", p->item.param);
        return;
    }

    opt->command = (int)(spec - hidppctl_commands);
    pf_argparser_consume(p);
}

static int parse_args_cb(struct pf_argparser *p, void *user) {
    struct hidppctl_opt *opt = user;

    if (pf_is_option(p))
        parse_args_option(p, opt);
    else
        parse_args_param(p, opt);

    return p->failed ? PF_ARGPARSE_EINTR : PF_ARGPARSE_OK;
}

/* ------------------------------------------------------------------------ */
/* Validation, once everything has been seen                                 */
/* ------------------------------------------------------------------------ */

/* Checks how many arguments the subcommand got. */
static pf_bool check_param_count(
    struct pf_argparser *p, const struct hidppctl_opt *opt
) {
    const struct hidppctl_command_spec *spec = &hidppctl_commands[opt->command];
    char what[64];

    snprintf(what, sizeof(what), "subcommand '%s'", spec->name);
    return pf_argparser_expect_params(p, spec->minArgs, spec->maxArgs, what);
}

static pf_bool check_device_selection(
    hidppctl_t *ctl, struct pf_argparser *p, struct hidppctl_opt *opt
) {
    if (opt->setSocket && opt->setInterface) {
        pf_argparser_error(
            p, "Option '--interface' has no effect when '--socket' is used\n"
        );

        return PF_FALSE;
    }

    if (!opt->setReceiver && opt->setInterface) {
        pf_argparser_error(
            p,
            "Option '--interface' cannot be used without a corresponding '-r' "
            "option.\n"
        );

        return PF_FALSE;
    }

    if (opt->setDevice && !opt->setReceiver && !opt->setSocket) {
        pf_argparser_error(
            p,
            "No receiver specified for wanted device.\n"
            "Use '-r' or '--socket' option for selecting a receiver.\n"
        );

        return PF_FALSE;
    }

    if (opt->setSocket && parse_socket_path(p, opt))
        return PF_FALSE;

    return PF_TRUE;
}

static int check_command_requirements(
    hidppctl_t *ctl, struct pf_argparser *p, struct hidppctl_opt *opt
) {
    int needs = hidppctl_commands[opt->command].needs;
    hidpp_bool_t cmdRequiresDevice = HIDPP_FALSE;
    hidpp_bool_t cmdRequiresReceiver = HIDPP_FALSE;

    switch (needs) {
    case HIDPPCTL_NEEDS_DEVICE:
        cmdRequiresDevice = HIDPP_TRUE;
        cmdRequiresReceiver = HIDPP_TRUE;
        break;
    case HIDPPCTL_NEEDS_RECEIVER:
        cmdRequiresReceiver = HIDPP_TRUE;
        cmdRequiresDevice = opt->setDevice;
        break;
    case HIDPPCTL_NEEDS_ANYTHING:
        cmdRequiresReceiver = opt->setReceiver || opt->setSocket;
        cmdRequiresDevice = opt->setDevice;
        break;
    default:
        break;
    }

    if (cmdRequiresDevice && !opt->setDevice) {
        pf_argparser_error(
            p, "A device must be specified for this subcommand."
        );

        return PF_FALSE;
    }

    if (cmdRequiresReceiver && !opt->setReceiver && !opt->setSocket) {
        pf_argparser_error(
            p, "A receiver must be specified for this subcommand."
        );

        return PF_FALSE;
    }

    return PF_TRUE;
}

static void parse_args_cache(struct pf_argparser *p, struct hidppctl_opt *opt) {
    if (p->paramc == 0)
        return;

    if (0 == strcmp(p->paramv[0], "collect"))
        opt->cache.collect = HIDPP_TRUE;
    else if (0 == strcmp(p->paramv[0], "clear"))
        opt->cache.clear = HIDPP_TRUE;
    else {
        pf_argparser_error(
            p,
            "Invalid argument '%s' for subcommand 'cache'; "
            "expected 'collect' or 'clear'.",
            p->paramv[0]
        );
    }
}

static void parse_args_remap(struct pf_argparser *p, struct hidppctl_opt *opt) {
    parse_u16(p, p->paramv[0], "control id", &opt->remap.ctrlId);
    parse_u16(p, p->paramv[1], "remap id", &opt->remap.remapId);
}

static void parse_args_poll(struct pf_argparser *p, struct hidppctl_opt *opt) {
    if (p->paramc == 0) {
        for (int i = 0; i < HIDPP__EVENT_MAX; i++)
            opt->poll.masks[i] = HIDPP_TRUE;
        return;
    }

    for (int i = 0; i < p->paramc; i++) {
        int j = parse_poll_event_name(p, p->paramv[i]);

        if (j == 0)
            return;
        opt->poll.masks[j] = HIDPP_TRUE;
    }
}

static void parse_args_divert(
    struct pf_argparser *p, struct hidppctl_opt *opt
) {
    opt->divert.needsInput = HIDPP_FALSE;

    /* The count was checked by check_param_count(), so this fits. */
    for (int i = 0; i < p->paramc; i++) {
        parse_diversion(p, opt, p->paramv[i], i);
        if (opt->divert.items[i].key)
            opt->divert.needsInput = HIDPP_TRUE;
    }
}

static void propagate_requirements(
    hidppctl_t *ctl, struct pf_argparser *p, struct hidppctl_opt *opt
) {
    if (opt->command == HIDPPCTL_DIVERT)
        ctl->requiresInput = opt->divert.needsInput;

    /* Subcommands like 'alias' only read the options. */
    if (hidppctl_commands[opt->command].needs == HIDPPCTL_NEEDS_NOTHING)
        return;

    if (opt->setDevice)
        ctl->requiresDevice = HIDPP_TRUE;

    if (opt->setReceiver)
        ctl->requiresReceiver = HIDPP_TRUE;

    if (opt->setSocket)
        ctl->requiresSocket = HIDPP_TRUE;

    if (ctl->requiresDevice && opt->command != HIDPPCTL_CACHE)
        ctl->requiresCache = !opt->cache.disabled;

    /* Logging is opt-in: use --log-path, or --verbose for a default path. */
    if (ctl->requiresDevice)
        ctl->requiresLog = !opt->log.disabled
            && (opt->log.path || opt->verbose);
}

/* Fills in the options which were not given with the ones of the alias. */
static int apply_alias(hidppctl_t *ctl, struct hidppctl_opt *opt) {
    static struct hidppctl_alias list[HIDPPCTL_MAX_ALIASES];
    struct hidppctl_alias *a = &opt->alias;
    int count, i, receiverFromAlias = 0;

    if (!opt->aliasName)
        return HIDPP_OK;

    if (hidppctl_alias_load(ctl, list, HIDPPCTL_MAX_ALIASES, &count))
        return HIDPP_EINVAL;

    for (i = 0; i < count; i++)
        if (0 == strcmp(list[i].name, opt->aliasName))
            break;

    if (i == count) {
        pf_cli_errorf(ctl->cli, "error: Unknown alias '%s'.\n", opt->aliasName);
        return HIDPP_EINVAL;
    }

    *a = list[i];

    if (!opt->setReceiver && !opt->setSocket) {
        if (a->receiver[0]) {
            opt->receiver = a->receiver;
            opt->setReceiver = receiverFromAlias = HIDPP_TRUE;
        } else if (a->socket[0]) {
            opt->socket.value = a->socket;
            opt->setSocket = HIDPP_TRUE;
        }
    }

    if (!opt->setDevice && a->device >= 0) {
        opt->device = (uint8_t)a->device;
        opt->setDevice = HIDPP_TRUE;
    }
    if (!opt->setInterface && receiverFromAlias && a->interface >= 0) {
        opt->interface = a->interface;
        opt->setInterface = HIDPP_TRUE;
    }
    if (!opt->setSwid && a->swid >= 0) {
        opt->swid = (uint8_t)a->swid;
        opt->setSwid = HIDPP_TRUE;
    }
    if (!opt->setTimeout && a->timeout >= 0) {
        opt->timeout = a->timeout;
        opt->setTimeout = HIDPP_TRUE;
    }

    return HIDPP_OK;
}

static int parse_args_end(
    hidppctl_t *ctl, struct pf_argparser *p, struct hidppctl_opt *opt
) {
    /* Everything the library collected for the common options. */
    opt->help = p->help;
    opt->verbose = p->verbose;
    opt->quiet = p->silent;

    opt->paramc = p->paramc;
    opt->paramv = p->paramv;

    if (p->version) {
        pf_argparser_print_version(p, "hidppctl", HIDPP_VERSION_STRING);
        opt->version = HIDPP_TRUE;
        return HIDPP_OK;
    }

    if (opt->command == HIDPPCTL_NONE)
        opt->help = HIDPP_TRUE;

    if (opt->help)
        return HIDPP_OK;

    if (!check_param_count(p, opt))
        return HIDPP_EINVAL;

    if (apply_alias(ctl, opt))
        return HIDPP_EINVAL;

    if (!check_command_requirements(ctl, p, opt))
        return HIDPP_EINVAL;

    if (!check_device_selection(ctl, p, opt))
        return HIDPP_EINVAL;

    switch (opt->command) {
        /* clang-format off */
    case HIDPPCTL_CACHE:  parse_args_cache(p, opt);  break;
    case HIDPPCTL_DIVERT: parse_args_divert(p, opt); break;
    case HIDPPCTL_REMAP:  parse_args_remap(p, opt);  break;
    case HIDPPCTL_POLL:   parse_args_poll(p, opt);   break;
    default:                                         break;
        /* clang-format on */
    }

    propagate_requirements(ctl, p, opt);

    return p->failed ? HIDPP_EINVAL : HIDPP_OK;
}

int hidppctl_parse_args(hidppctl_t *ctl) {
    int rc = parse_args_start(ctl->argparser, ctl->options);

    if (rc)
        return rc;

    ctl->argparser->errPrefix = "error: ";
    pf_argparser_set_cli(ctl->argparser, ctl->cli);

    rc = pf_argparser_run(ctl->argparser, parse_args_cb, ctl->options);
    if (rc)
        return rc == PF_ARGPARSE_ENOMEM ? HIDPP_ENOMEM : HIDPP_EINVAL;

    return parse_args_end(ctl, ctl->argparser, ctl->options);
}