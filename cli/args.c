/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "common.h"

#include <pf_argparse.h>

static pf_option_enum_t hidppctl_command_enum[] = {
    { "info", HIDPPCTL_INFO },
    { "poll", HIDPPCTL_POLL },
    { "divert", HIDPPCTL_DIVERT },
    { "remap", HIDPPCTL_REMAP },
    { 0 },
};

struct definition {
    const char *name;
    const char *description;
};

static const char *help_epilog = "For more information, run `man hidppctl.1'.";

static const char *help_usage_list[] = {
    [HIDPPCTL_NONE] = "hidppctl [OPTIONS]... <command>",
    [HIDPPCTL_INFO] = "hidppctl [OPTIONS]... info",
    [HIDPPCTL_POLL] = "hidppctl [OPTIONS]... poll",
    [HIDPPCTL_DIVERT] = "hidppctl [OPTIONS]... divert [buttons...]",
    [HIDPPCTL_REMAP] = "hidppctl [OPTIONS]... remap <control-id> <remap-id>",
};

static const char *help_desc_list[] = {
    [HIDPPCTL_NONE] = "Configure HID++ compatible devices.",
    [HIDPPCTL_INFO] = "Shows information about HID++ devices.",
    [HIDPPCTL_POLL] = "Polls specified device for incoming events.",
    [HIDPPCTL_REMAP] = "Remaps device's specified control to a different one.",
    [HIDPPCTL_DIVERT]
    = "Diverts specified device's buttons and prints associated events."
      "\nYou can also rebind device's buttons using an argument like this:"
      "\n    '<button code>=<key code>[+<modifier>]'"
      "\nFor example: '0x0104=home+lshift'.",
};

static struct definition help_options[] = {
    { "-?, --help", "Shows this information." },
    { "-v, --verbose", "Print more verbose messages." },
    { "--quiet", "Disables output and error printing." },
    { "-r, --receiver", "Specifies which HID++ receiver to use." },
    { "-d, --device", "Specifies which HID++ device index to use." },
    { "--interface", "Specifies which HID interface to use." },
    { "--timeout", "Sets the timeout for IO operations in milliseconds." },
    { "--swid", "Sets the software ID for interacting with devices." },
    { 0 },
};

static struct definition help_subcommands[] = {
    { "info", "shows information about HID++ devices." },
    { "poll", "polls specified devices for incoming events." },
    { "divert", "diverts events of reprogrammable buttons." },
    { "remap", "remaps device's control to a different one." },
    { 0 },
};

static void print_definitions(pf_cli_t *cli, struct definition *def) {
    for (; def->name; def++)
        pf_cli_help_definition(cli, def->name, def->description);
}

int hidppctl_print_help(hidppctl_t *ctl) {
    pf_cli_t *cli = ctl->cli;
    int cmd = ctl->options->command;

    pf_cli_printf(
        cli, "usage: %s\n\n%s\n", help_usage_list[cmd], help_desc_list[cmd]
    );

    pf_cli_help_section(cli, "Options:");
    print_definitions(cli, help_options);

    if (cmd == HIDPPCTL_NONE) {
        pf_cli_help_section(cli, "Subcommands:");
        print_definitions(cli, help_subcommands);
    }

    pf_cli_printf(cli, "\n%s\n", help_epilog);
    return HIDPP_OK;
}

static char *find_next_chr(char *str, char chr) {
    char *out = strchr(str, chr);
    if (out)
        *out = '\0';
    return out;
}

static int parse_diversion_ctrl(pf_argparser_t *p, char *str, uint16_t *out) {
    char *end;
    long ctrl = strtol(str, &end, 0);

    if (end == str) {
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
            UINT16_MAX,
            ctrl
        );
        return HIDPP_EINVAL;
    }

    *out = ctrl;
    return HIDPP_OK;
}

static int parse_diversion_keys(
    pf_argparser_t *p, struct hidppctl_opt *o, char *str, int i
) {
    char *start = str;
    char *end;
    int count = -1;

    do {
        if (count > HIDPP_MAX_MODS) {
            pf_argparser_error(p, "Too many modifiers!");
            return HIDPP_ENOMEM;
        }

        end = find_next_chr(start, '+');
        int key = hidpp_input_key(start);

        if (key < 0) {
            pf_argparser_error(p, "Unknown key code '%s'.", start);
            return HIDPP_EINVAL;
        }

        if (count == -1)
            o->divert.items[i].key = key;
        else
            o->divert.items[i].mods[count] = key;

        count++;
        start = end + 1;
    } while (end);

    o->divert.items[i].count = count;
    return HIDPP_OK;
}

static int parse_diversion(
    pf_argparser_t *p, struct hidppctl_opt *o, char *arg, int i
) {
    char *ctrlEnd = find_next_chr(arg, '=');

    if (parse_diversion_ctrl(p, arg, &o->divert.items[i].ctrl))
        return HIDPP_EINVAL;

    if (ctrlEnd && parse_diversion_keys(p, o, &ctrlEnd[1], i))
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

static int parse_remap_param(
    pf_argparser_t *p, const char *arg, uint16_t *out
) {
    char *end;
    long value = strtol(arg, &end, 0);

    if (end == arg) {
        pf_argparser_error(p, "Expected a number; got '%s' instead.", arg);
        return HIDPP_EINVAL;
    }

    if (value < 0 || value > UINT16_MAX) {
        pf_argparser_error(
            p,
            "Control id must be between 0 and %u; got %ld instead.",
            UINT16_MAX,
            value
        );
        return HIDPP_EINVAL;
    }

    *out = value;
    return HIDPP_OK;
}

static int parse_args_start(struct pf_argparser *p, struct hidppctl_opt *o) {
    memset(o, 0, sizeof(*o));
    return HIDPP_OK;
}

static void parse_args_remap(struct pf_argparser *p, struct hidppctl_opt *opt) {
    if (p->paramc > 2) {
        pf_argparser_error(p, "Too many arguments for 'divert' subcommand.");
        return;
    }

    parse_remap_param(p, opt->paramv[0], &opt->remap.ctrlId);
    parse_remap_param(p, opt->paramv[1], &opt->remap.remapId);
}

static void parse_args_divert(
    struct pf_argparser *p, struct hidppctl_opt *opt
) { }

static void parse_args_other(struct pf_argparser *p, struct hidppctl_opt *opt) {
    if (pf_is_param(p, -1)) {
        pf_argparser_error(
            p, "subcommand '%s' takes no arguments.", p->paramv[0]
        );
    }
}

static void parse_args_none(struct pf_argparser *p, struct hidppctl_opt *opt) {
    if (pf_is_param(p, 0)) {
        if (!pf_match_enum(p->item.param, hidppctl_command_enum, &opt->command))
            pf_argparser_error(p, "unknown subcommand '%s'", p->item.param);
    }
}

static int parse_args_cb(struct pf_argparser *p, void *user) {
    struct hidppctl_opt *opt = user;
    int swid, dev;

    if (pf_option_toggle(p, "help", '?', &opt->help))
        p->help = opt->help;
    if (pf_option_toggle(p, "verbose", 0, &opt->verbose))
        p->verbose = opt->verbose;
    if (pf_option_toggle(p, "quiet", 0, &opt->quiet))
        p->silent = opt->quiet;

    if (pf_option_int(p, "interface", 0, &opt->interface))
        opt->setInterface = HIDPP_TRUE;
    if (pf_option_int(p, "timeout", 0, &opt->timeout))
        opt->setTimeout = HIDPP_TRUE;

    if (pf_option_string(p, "receiver", 0, &opt->receiver)) {
        if (opt->subject == HIDPPCTL_ALL)
            opt->subject = HIDPPCTL_RECEIVER;
    }

    if (pf_option_int(p, "device", 0, &dev)) {
        if (dev < 1 || (dev > 6 && dev != 0xFF))
            pf_argparser_error(p, "Invalid device index %d.", dev);
        else
            opt->device = dev;

        opt->subject = HIDPPCTL_DEVICE;
    }

    if (pf_option_int(p, "swid", 0, &swid)) {
        if (swid < 0 || swid > 15) {
            pf_argparser_error(
                p,
                "Software id must be between 0 and 15 (inclusive), got %d.",
                swid
            );
        } else {
            opt->swid = swid;
            opt->setSwid = HIDPP_TRUE;
        }
    }

    switch (opt->command) {
        /* clang-format off */
    case HIDPPCTL_DIVERT: parse_args_divert(p, opt); break;
    case HIDPPCTL_REMAP:  parse_args_remap(p, opt);  break;
    case HIDPPCTL_NONE:   parse_args_none(p, opt);   break;
    case HIDPPCTL_POLL:   break;
    case HIDPPCTL_INFO:
    default:              parse_args_other(p, opt);  break;
        /* clang-format on */
    }

    p->help = opt->help;

    return p->failed ? PF_ARGPARSE_EINTR : PF_ARGPARSE_OK;
}

static int parse_args_end(
    hidppctl_t *ctl, struct pf_argparser *p, struct hidppctl_opt *opt
) {
    opt->paramc = p->paramc;
    opt->paramv = p->paramv;
    ctl->cli->silent = opt->quiet;

    if (opt->paramc == 0)
        opt->help = PF_TRUE;

    if (opt->help)
        return HIDPP_OK;

    if (opt->subject == HIDPPCTL_DEVICE && !opt->receiver) {
        pf_argparser_error(
            p, "Selecting a device requires specifying the receiver."
        );
    }

    if (opt->command == HIDPPCTL_REMAP && p->paramc != 2) {
        pf_argparser_error(
            p,
            "Subcommand 'remap' requires exactly 2 arguments; got %d instead",
            p->paramc
        );
    }

    if (opt->command == HIDPPCTL_POLL) {
        if (opt->paramc == 0) {
            for (int i = 0; i < HIDPP__EVENT_MAX; i++)
                opt->poll.masks[i] = HIDPP_TRUE;
        } else {
            for (int j, i = 0; i < opt->paramc; i++) {
                if (0 == (j = parse_poll_event_name(p, opt->paramv[i])))
                    return HIDPP_EINVAL;
                opt->poll.masks[j] = HIDPP_TRUE;
            }
        }
    }

    if (opt->command == HIDPPCTL_DIVERT) {
        if (p->paramc == 0) {
            pf_argparser_error(
                p, "Subcommand 'divert' requires at least 1 argument."
            );
        }

        if (p->paramc > HIDPP_MAX_DIVERT) {
            pf_argparser_error(
                p,
                "Too many diversions specified; maximum is %d, got %d.",
                HIDPP_MAX_DIVERT,
                p->paramc
            );
        }

        if (opt->subject != HIDPPCTL_DEVICE) {
            pf_argparser_error(
                p, "A device must be specified for 'divert' subcommand!"
            );
        }

        opt->requiresInput = HIDPP_FALSE;

        for (int i = 0; i < opt->paramc; i++) {
            parse_diversion(p, opt, opt->paramv[i], i);
            if (opt->divert.items[i].key)
                opt->requiresInput = HIDPP_TRUE;
        }
    }

    return p->failed ? HIDPP_EINVAL : HIDPP_OK;
}

int hidppctl_parse_args(hidppctl_t *ctl) {
    return parse_args_start(ctl->argparser, ctl->options)
        || pf_argparser_run(ctl->argparser, parse_args_cb, ctl->options)
        || parse_args_end(ctl, ctl->argparser, ctl->options);
}