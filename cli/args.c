/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "common.h"

#include <pf_cli.h>

#define PF_ARGPARSE_QUIET_SHORT 0
#include <pf_argparse.h>

#define COMMON_OPTIONS                                                      \
    (PF_COMMON_HELP | PF_COMMON_VERBOSE | PF_COMMON_QUIET | PF_COMMON_COLOR \
     | PF_COMMON_INTERACTIVE)

PF_ENUM_GUARD(hidppctl_command, 1);
static const pf_option_enum_t hidppctl_command_enum[] = {
    { "none", HIDPPCTL_NONE },
    { "cache", HIDPPCTL_CACHE },
    { "divert", HIDPPCTL_DIVERT },
    { "list-events", HIDPPCTL_LIST_EVENTS },
    { "list-keycodes", HIDPPCTL_LIST_KEYCODES },
    { "poll", HIDPPCTL_POLL },
    { "remap", HIDPPCTL_REMAP },
    { "show-features", HIDPPCTL_SHOW_FEATURES },
    { "show-keymap", HIDPPCTL_SHOW_KEYMAP },
    { "status", HIDPPCTL_STATUS },
    { 0 },
};

static const char *help_epilog = "For more information, run `man hidppctl.1'.";

PF_ENUM_GUARD(hidppctl_command, 1);
static const char *help_usage_list[] = {
    [HIDPPCTL_NONE] = "hidppctl [OPTIONS]... <command>",
    [HIDPPCTL_CACHE] = "hidppctl [OPTIONS]... cache <collect|clear>",
    [HIDPPCTL_DIVERT] = "hidppctl [OPTIONS]... divert [buttons...]",
    [HIDPPCTL_LIST_EVENTS] = "hidppctl [OPTIONS]... list-events",
    [HIDPPCTL_LIST_KEYCODES] = "hidppctl [OPTIONS]... list-keycodes",
    [HIDPPCTL_POLL] = "hidppctl [OPTIONS]... poll",
    [HIDPPCTL_REMAP] = "hidppctl [OPTIONS]... remap <control-id> <remap-id>",
    [HIDPPCTL_SHOW_FEATURES] = "hidppctl [OPTIONS]... show-features",
    [HIDPPCTL_SHOW_KEYMAP] = "hidppctl [OPTIONS]... show-keymap",
    [HIDPPCTL_STATUS] = "hidppctl [OPTIONS]... status",
};

PF_ENUM_GUARD(hidppctl_command, 1);
static const char *help_desc_list[] = {
    /* clang-format off */
    [HIDPPCTL_NONE] = "Configure HID++ compatible devices.",
    [HIDPPCTL_CACHE] = (
"Manipulate the selected device's cache file."
"\nThe program caches device information automatically both at runtime and"
"\nbetween sessions. This can significantly improve the speed of most operations"
"\nand in most cases shouldn't produce problems. However, the user can turn off"
"\nfile-based hashing by using the '--no-cache' option. Runtime caching cannot"
"\nbe disabled."
"\n"
"\nPassing 'collect' as a parameter will cache all information from all features"
"\nthat the given device supports."
    ),
    [HIDPPCTL_DIVERT] = (
"Diverts specified device's buttons and prints associated events."
"\nYou can also rebind device's buttons using an argument like this:"
"\n    '<button code>=<key code>[+<modifier>]'"
"\nFor example: '0x0104=home+lshift'."
    ),
    [HIDPPCTL_LIST_EVENTS] = "Lists supported events and their identifiers.",
    [HIDPPCTL_LIST_KEYCODES] = "Lists supported OS key identifiers.",
    [HIDPPCTL_POLL] = "Polls specified device for incoming events.",
    [HIDPPCTL_REMAP] = "Remaps device's specified control to a different one.",
    [HIDPPCTL_SHOW_FEATURES] = "Shows supported features for specified device.",
    [HIDPPCTL_SHOW_KEYMAP] = "Shows keymap information for specified device.",
    [HIDPPCTL_STATUS] = "Shows status information about HID++ devices.",
    /* clang-format on */
};

/* The common options (help, verbose, ...) are listed by the library. */
static const pf_cli_definition_t help_options[] = {
    { "-r, --receiver", "Specifies which HID++ receiver to use." },
    { "-d, --device", "Specifies which HID++ device index to use." },
    { "--interface", "Specifies which HID interface to use." },
    { "--timeout", "Sets the timeout for IO operations in milliseconds." },
    { "--swid", "Sets the software ID for interacting with devices." },
    { 0 },
};

PF_ENUM_GUARD(hidppctl_command, 1);
static const pf_cli_definition_t help_subcommands[] = {
    { "none", "" },
    { "cache", "manipulates cache files for specified device." },
    { "divert", "diverts events of reprogrammable buttons." },
    { "list-events", "lists event identifiers" },
    { "list-keycodes", "lists program's supported keycodes" },
    { "poll", "polls specified devices for incoming events." },
    { "remap", "remaps device's control to a different one." },
    { "show-features", "shows supported features for specified device" },
    { "show-keymap", "shows keymap information for specified device" },
    { "status", "shows status information about HID++ devices." },
    { 0 },
};

int hidppctl_print_help(hidppctl_t *ctl) {
    pf_cli_t *cli = ctl->cli;
    int cmd = ctl->options->command;

    pf_cli_printf(
        cli, "usage: %s\n\n%s\n", help_usage_list[cmd], help_desc_list[cmd]
    );

    pf_cli_help_section(cli, "Options:");
    pf_argparser_help_options(ctl->argparser, COMMON_OPTIONS);
    pf_cli_help_definitions(cli, help_options);

    if (cmd == HIDPPCTL_NONE) {
        pf_cli_help_section(cli, "Subcommands:");
        pf_cli_help_definitions(cli, help_subcommands);
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

/* ------------------------------------------------------------------------ */
/* The callback: options and the subcommand name                             */
/* ------------------------------------------------------------------------ */

static int parse_args_start(struct pf_argparser *p, struct hidppctl_opt *o) {
    memset(o, 0, sizeof(*o));
    return HIDPP_OK;
}

static void parse_args_option(
    struct pf_argparser *p, struct hidppctl_opt *opt
) {
    int dev, swid;

    /* --help, --verbose, --quiet, --color, --interactive */
    if (pf_option_common(p, COMMON_OPTIONS))
        return;

    if (pf_option_int(p, "interface", 0, &opt->interface))
        opt->setInterface = HIDPP_TRUE;
    if (pf_option_int(p, "timeout", 0, &opt->timeout))
        opt->setTimeout = HIDPP_TRUE;

    if (pf_option_string(p, "receiver", 'r', &opt->receiver)) {
        if (opt->subject == HIDPPCTL_ALL)
            opt->subject = HIDPPCTL_RECEIVER;
    }

    if (pf_option_int(p, "device", 'd', &dev)) {
        if (dev < 1 || (dev > 6 && dev != 0xFF)) {
            pf_argparser_error(p, "Invalid device index %d.", dev);
        } else {
            opt->device = dev;
            opt->subject = HIDPPCTL_DEVICE;
        }
    }

    if (pf_option_int_range(p, "swid", 0, 0, 15, &swid)) {
        opt->swid = swid;
        opt->setSwid = HIDPP_TRUE;
    }
}

static void parse_args_param(struct pf_argparser *p, struct hidppctl_opt *opt) {
    int cmd;

    /* The first parameter is the subcommand; it is not kept in paramv. */
    if (pf_param_subcommand(p, "subcommand", hidppctl_command_enum, &cmd))
        opt->command = cmd;
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
    char what[64];

    switch (opt->command) {
        /* clang-format off */
        PF_ENUM_GUARD(hidppctl_command, 1);
    case HIDPPCTL_CACHE:
        return pf_argparser_expect_params(p, 0, 1, "subcommand 'cache'");
    case HIDPPCTL_DIVERT:
        return pf_argparser_expect_params(
            p, 1, HIDPP_MAX_DIVERT, "subcommand 'divert'"
        );
    case HIDPPCTL_REMAP:
        return pf_argparser_expect_params(p, 2, 2, "subcommand 'remap'");
    case HIDPPCTL_POLL:
        return PF_TRUE; /* any number of event names */
    default:
        snprintf(
            what, sizeof(what), "subcommand '%s'",
            help_subcommands[opt->command].name
        );
        return pf_argparser_expect_params(p, 0, 0, what);
        /* clang-format on */
    }
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
    if (opt->subject != HIDPPCTL_DEVICE) {
        pf_argparser_error(
            p, "A device must be specified for 'divert' subcommand!"
        );
    }

    opt->requiresInput = HIDPP_FALSE;

    /* The count was checked by check_param_count(), so this fits. */
    for (int i = 0; i < p->paramc; i++) {
        parse_diversion(p, opt, p->paramv[i], i);
        if (opt->divert.items[i].key)
            opt->requiresInput = HIDPP_TRUE;
    }
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

    if (opt->command == HIDPPCTL_NONE)
        opt->help = HIDPP_TRUE;

    if (opt->help)
        return HIDPP_OK;

    if (opt->subject == HIDPPCTL_DEVICE)
        opt->requiresDevice = HIDPP_TRUE;
    if (opt->subject == HIDPPCTL_DEVICE || opt->subject == HIDPPCTL_RECEIVER)
        opt->requiresReceiver = HIDPP_TRUE;

    if (opt->subject == HIDPPCTL_DEVICE && !opt->receiver) {
        pf_argparser_error(
            p, "Selecting a device requires specifying the receiver."
        );
    }

    if (!check_param_count(p, opt))
        return HIDPP_EINVAL;

    switch (opt->command) {
        /* clang-format off */
        PF_ENUM_GUARD(hidppctl_command, 1);
    case HIDPPCTL_CACHE:  parse_args_cache(p, opt);  break;
    case HIDPPCTL_DIVERT: parse_args_divert(p, opt); break;
    case HIDPPCTL_REMAP:  parse_args_remap(p, opt);  break;
    case HIDPPCTL_POLL:   parse_args_poll(p, opt);   break;
    default:                                         break;
        /* clang-format on */
    }

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