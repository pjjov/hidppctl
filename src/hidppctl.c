/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include <hidpp.h>
#include <pf_argparse.h>
#include <pf_cli.h>

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static volatile sig_atomic_t terminate = 0;

#define HIDPP_MAX_DIVERT 32
#define HIDPP_MAX_MODS 8

struct diversion {
    uint16_t ctrl;
    char state;
    int key;
    int count;
    int mods[HIDPP_MAX_MODS];
};

typedef struct pf_argparser pf_argparser_t;

typedef struct hidppctl_t {
    struct hidppctl_opt *options;
    pf_argparser_t *argparser;
    pf_cli_t *cli;
    hidpp_input_t *input;
} hidppctl_t;

struct hidppctl_opt {
    pf_bool help;
    pf_bool quiet;
    pf_bool verbose;

    int command;
    int subject;

    char **paramv;
    int paramc;
    char *receiver;

    uint8_t device;
    uint8_t swid;
    int timeout;
    int interface;

    pf_bool setSwid;
    pf_bool setTimeout;
    pf_bool setInterface;

    struct {
        uint16_t ctrlId;
        uint16_t remapId;
    } remap;

    struct {
        char masks[HIDPP__EVENT_MAX];
    } poll;

    struct {
        struct diversion items[HIDPP_MAX_DIVERT];
    } divert;

    pf_bool requiresInput;
};

enum hidppctl_command {
    HIDPPCTL_NONE = 0,
    HIDPPCTL_INFO,
    HIDPPCTL_POLL,
    HIDPPCTL_DIVERT,
    HIDPPCTL_REMAP,
};

enum hidppctl_subject {
    HIDPPCTL_DEVICE,
    HIDPPCTL_RECEIVER,
    HIDPPCTL_ALL,
};

static pf_option_enum_t hidppctl_command_enum[] = {
    { "info", HIDPPCTL_INFO },
    { "poll", HIDPPCTL_POLL },
    { "divert", HIDPPCTL_DIVERT },
    { "remap", HIDPPCTL_REMAP },
    { 0 },
};

static const char *eventNames[HIDPP__EVENT_MAX] = {
    "NONE",
    "UNKNOWN",
    "BUTTON",
    "MOUSE",
    "BATTERY",
    "WHEEL",
    "RATCHET",
    "RATCHET_SWITCH",
    "TOUCH_PAD_POINTS",
    "TOUCH_MOUSE_POINTS",
    "TOUCH_MOUSE_STATUS",
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

static int print_help(hidppctl_t *ctl) {
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

static void signal_handler(int sig) { terminate = 1; }

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
        if (0 == strcmp(arg, eventNames[i]))
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

static int parse_args(hidppctl_t *ctl) {
    return parse_args_start(ctl->argparser, ctl->options)
        || pf_argparser_run(ctl->argparser, parse_args_cb, ctl->options)
        || parse_args_end(ctl, ctl->argparser, ctl->options);
}

hidpp_receiver_t *open_receiver(hidppctl_t *ctl, const char *name) {
    struct hidppctl_opt *opt = ctl->options;

    if (!name) {
        pf_cli_errorf(ctl->cli, "The HID++ receiver must be specified!");
        return NULL;
    }

    hidpp_receiver_t *rcv;

    char *colon, *end;
    long vid = strtol(name, &colon, 16);
    long pid = strtol(colon + 1, &end, 16);

    if (vid < 0 || pid < 0 || vid > UINT16_MAX || pid > UINT16_MAX) {
        pf_cli_errorf(
            ctl->cli,
            "Vendor and product ids must be between 0 and %u.",
            UINT16_MAX
        );
        return NULL;
    }

    if (*colon == ':' && (*end == ':' || *end == '\0')) {
        if (opt->setInterface)
            rcv = hidpp_open_interface(vid, pid, opt->interface);
        else
            rcv = hidpp_open(vid, pid, NULL);
    } else {
        rcv = hidpp_open_path(name);
    }

    if (rcv == NULL)
        pf_cli_errorf(ctl->cli, "Unable to open receiver '%s'.", name);

    if (opt->setTimeout)
        hidpp_set_timeout(rcv, opt->timeout);
    if (opt->setSwid)
        hidpp_set_swid(rcv, opt->swid);
    return rcv;
}

static hidpp_device_t *open_device(
    hidppctl_t *ctl, hidpp_receiver_t *rcv, uint8_t index
) {
    hidpp_device_t *dev;

    if (index == 0) {
        pf_cli_errorf(ctl->cli, "The HID++ device must be specified!");
        return NULL;
    }

    if (!(dev = hidpp_device_open(rcv, index)))
        pf_cli_errorf(ctl->cli, "Unable to open device: %ls", hidpp_error(rcv));
    return dev;
}

int cmd_info_all(hidppctl_t *ctl) {
    pf_cli_t *cli = ctl->cli;

    struct hidpp_receiver_info *info, all[32];
    size_t len = hidpp_enumerate(0, 0, all, 32);

    if (len == 0) {
        pf_cli_printf(cli, "No HID++ receivers found!\n");
        return HIDPP_OK;
    }

    for (size_t i = 0; i < len; i++) {
        info = &all[i];

        pf_cli_printf(
            cli,
            "HID++ receiver '%ls' from '%ls'\n",
            info->product,
            info->manufacturer
        );
        pf_cli_printf(
            cli,
            "  ID: %.4x:%.4x (%s)\n",
            info->vendorId,
            info->productId,
            info->path
        );

        pf_cli_printf(
            cli,
            "  Interface and usage: %d %u/%u\n",
            info->interfaceNumber,
            info->usage,
            info->usagePage
        );

        hidpp_free_info(info);
    }

    return HIDPP_OK;
}

static int cmd_info_rcv(hidppctl_t *ctl) {
    pf_cli_t *cli = ctl->cli;

    hidpp_receiver_t *rcv = open_receiver(ctl, ctl->options->receiver);

    hidpp_device_t *dev;
    struct hidpp_receiver_info info;
    struct hidpp_device_info dinfo;

    if (!rcv || hidpp_receiver_info(rcv, &info))
        return HIDPP_EIO;

    pf_cli_printf(cli, "HID++ receiver '%ls'\n", info.product);
    pf_cli_printf(
        cli, "  ID: %.4x:%.4x (%s)\n", info.vendorId, info.productId, info.path
    );
    pf_cli_printf(cli, "  Serial number: %ls\n", info.serial);
    pf_cli_printf(cli, "  Manufacturer: %ls\n", info.manufacturer);
    pf_cli_printf(cli, "  Release number %u\n", info.releaseNumber);
    pf_cli_printf(
        cli, "  Usage and page: %u, %u\n", info.usage, info.usagePage
    );
    pf_cli_printf(cli, "  Interface: %d\n", info.interfaceNumber);

    for (int i = 1; i < 7; i++) {
        if (!(dev = hidpp_device_open(rcv, i))) {
            pf_cli_printf(cli, "  Device %d disconnected\n", i);
            continue;
        }

        hidpp_device_info(dev, &dinfo);
        pf_cli_printf(cli, "  Device %d connected\n", i);
        pf_cli_printf(cli, "    Version: %u.%u\n", dinfo.major, dinfo.minor);
        hidpp_device_close(dev);
    }

    hidpp_free_info(&info);
    hidpp_close(rcv);
    return HIDPP_OK;
}

static int cmd_info_dev(hidppctl_t *ctl) {
    pf_cli_t *cli = ctl->cli;

    hidpp_receiver_t *rcv = open_receiver(ctl, ctl->options->receiver);
    hidpp_device_t *dev = open_device(ctl, rcv, ctl->options->device);
    struct hidpp_device_info info;

    if (!rcv || !dev)
        return HIDPP_EIO;

    if (hidpp_device_info(dev, &info)) {
        pf_cli_errorf(
            cli, "Unable to read device information: %ls", hidpp_error(rcv)
        );
        hidpp_device_close(dev);
        hidpp_close(rcv);
        return HIDPP_EIO;
    }

    pf_cli_printf(cli, "Device %d connected '%s'\n", info.index);
    pf_cli_printf(cli, "  Version: %u.%u\n", info.major, info.minor);

    hidpp_device_close(dev);
    hidpp_close(rcv);
    return HIDPP_OK;
}

static int cmd_info_features(hidppctl_t *ctl) {
    pf_cli_t *cli = ctl->cli;

    hidpp_receiver_t *rcv = open_receiver(ctl, ctl->options->receiver);
    hidpp_device_t *dev = open_device(ctl, rcv, ctl->options->device);
    struct hidpp_device_info info;

    if (!rcv || !dev)
        return HIDPP_EIO;

    if (hidpp_device_info(dev, &info)) {
        pf_cli_errorf(
            cli, "Unable to read device information: %ls", hidpp_error(rcv)
        );
        hidpp_device_close(dev);
        hidpp_close(rcv);
        return HIDPP_EIO;
    }

    uint16_t features[256];
    size_t featCount = hidpp_feature_list(dev, features, 256);

    pf_cli_printf(cli, "Device supports %u HID++ features:\n", featCount);

    for (int i = 0; i < featCount; i++) {
        uint16_t feat = hidpp_feature_id(dev, i);
        pf_cli_printf(cli, "  [0x%.4x] %s\n", feat, hidpp_feature_name(feat));
    }

    hidpp_device_close(dev);
    hidpp_close(rcv);
    return HIDPP_OK;
}

static int cmd_info_keymap(hidppctl_t *ctl) {
    pf_cli_t *cli = ctl->cli;

    hidpp_receiver_t *rcv = open_receiver(ctl, ctl->options->receiver);
    hidpp_device_t *dev = open_device(ctl, rcv, ctl->options->device);
    hidpp_keymap_t *map = hidpp_keymap(dev);

    if (!rcv || !dev)
        return HIDPP_EIO;

    if (!map) {
        pf_cli_printf(
            cli, "Selected device doesn't support keymap features!\n"
        );
        return HIDPP_OK;
    }

    uint16_t controls[256];
    size_t ctrlCount = hidpp_keymap_list(map, controls, 256);

    pf_cli_printf(cli, "Device has %u remappable controls:\n", ctrlCount);

    for (int i = 0; i < ctrlCount; i++) {
        uint16_t ctrl = hidpp_keymap_id(map, i);
        pf_cli_printf(cli, "  [0x%.4x] %s\n", ctrl, hidpp_keymap_name(ctrl));
    }

    hidpp_device_close(dev);
    hidpp_close(rcv);
    return HIDPP_OK;
}

extern struct {
    const char name[12];
    int code;
} hidpp_input_table[];

static int cmd_info_keycodes(hidppctl_t *ctl) {
    pf_cli_t *cli = ctl->cli;
    pf_cli_printf(
        cli, "Aside from numeric values, key codes also have following aliases:"
    );

    size_t max = cli->out.columns;
    size_t length = max;

    for (size_t i = 0; hidpp_input_table[i].code; i++) {
        size_t curr = strlen(hidpp_input_table[i].name) + 4;
        length += curr;

        if (length >= max) {
            pf_cli_printf(cli, "\n  ");
            length = 2 + curr;
        }

        pf_cli_printf(cli, "'%s', ", hidpp_input_table[i].name);
    }

    pf_cli_printf(cli, "\n");
    return HIDPP_OK;
}

static int cmd_info_events(hidppctl_t *ctl) {
    pf_cli_t *cli = ctl->cli;
    pf_cli_printf(cli, "Supported event types:");

    size_t max = cli->out.columns;
    size_t length = max;

    for (int i = 0; i < HIDPP__EVENT_MAX; i++) {
        size_t curr = strlen(eventNames[i]) + 4;
        length += curr;

        if (length >= max) {
            pf_cli_printf(cli, "\n  ");
            length = 2 + curr;
        }

        pf_cli_printf(cli, "'%s', ", eventNames[i]);
    }

    pf_cli_printf(cli, "\n");
    return HIDPP_OK;
}

static int cmd_info(hidppctl_t *ctl) {
    struct hidppctl_opt *opt = ctl->options;

    if (opt->paramc == 1) {
        if (0 == strcmp(opt->paramv[0], "keymap"))
            return cmd_info_keymap(ctl);
        if (0 == strcmp(opt->paramv[0], "features"))
            return cmd_info_features(ctl);
        if (0 == strcmp(opt->paramv[0], "keycodes"))
            return cmd_info_keycodes(ctl);
        if (0 == strcmp(opt->paramv[0], "events"))
            return cmd_info_events(ctl);

        pf_cli_errorf(
            ctl->cli,
            "Unsupported argument '%s'; Valid values are:\n"
            "'keymap', 'features', 'keycodes', 'events'.",
            opt->paramv[0]
        );
        return HIDPP_EINVAL;
    } else if (opt->paramc > 0) {
        pf_cli_errorf(
            ctl->cli,
            "Subcommand 'info' takes 1 argument; found %d",
            opt->paramc
        );
        return HIDPP_EINVAL;
    }

    switch (opt->subject) {
        /* clang-format off */
    case HIDPPCTL_RECEIVER: return cmd_info_rcv(ctl);
    case HIDPPCTL_DEVICE:   return cmd_info_dev(ctl);
    case HIDPPCTL_ALL:
    default:                return cmd_info_all(ctl);
        /* clang-format on */
    }
}

static int cmd_divert_init(hidppctl_t *ctl, hidpp_keymap_t *map) {
    struct diversion *diversions = ctl->options->divert.items;

    if (!map) {
        pf_cli_errorf(ctl->cli, "Specified device doesn't support diversion!");
        return HIDPP_EIO;
    }

    for (int i = 0; i < ctl->options->paramc; i++) {
        uint16_t ctrl = diversions[i].ctrl;

        if (hidpp_keymap_divert(map, ctrl, HIDPP_TRUE)) {
            pf_cli_errorf(
                ctl->cli, "Unable to divert the control with id 0x%.4x!", ctrl
            );
            return HIDPP_EIO;
        }
    }

    return HIDPP_OK;
}

static int cmd_divert_term(hidppctl_t *ctl, hidpp_keymap_t *map) {
    struct diversion *diversions = ctl->options->divert.items;

    for (int i = 0; i < ctl->options->paramc; i++) {
        uint16_t ctrl = diversions[i].ctrl;

        if (hidpp_keymap_divert(map, ctrl, HIDPP_FALSE)) {
            pf_cli_errorf(
                ctl->cli, "Unable to undivert the control with id 0x%.4x!", ctrl
            );
        }
    }

    return HIDPP_OK;
}

static void cmd_divert_set(
    struct diversion *div, hidpp_input_t *input, int value
) {
    if (div->state == value)
        return;

    hidpp_input_set(input, div->key, div->mods, div->count, value);
    div->state = value;
}

static int cmd_divert_poll(
    hidppctl_t *ctl, hidpp_device_t *dev, hidpp_keymap_t *map
) {
    struct diversion *diversions = ctl->options->divert.items;
    struct hidpp_event e;

    if (hidpp_device_poll(dev, &e))
        return HIDPP_EIO;

    if (e.type != HIDPP_EVENT_BUTTON)
        return HIDPP_ENOSYS;

    for (int i = 0; i < ctl->options->paramc; i++) {
        struct diversion *div = &diversions[i];
        int value = 0;

        for (int i = 0; i < 4; i++)
            if (e.as.buttons[i] == div->ctrl)
                value = 1;

        if (value != div->state)
            cmd_divert_set(div, ctl->input, value);
    }

    return HIDPP_OK;
}

static int cmd_divert(hidppctl_t *ctl) {
    hidpp_receiver_t *rcv = open_receiver(ctl, ctl->options->receiver);
    hidpp_device_t *dev = open_device(ctl, rcv, ctl->options->device);
    hidpp_keymap_t *map = hidpp_keymap(dev);

    if (!dev || !rcv || cmd_divert_init(ctl, map))
        return HIDPP_EIO;

    signal(SIGTERM, signal_handler);
    pf_cli_cprintf(ctl->cli, PF_CLI_BOLD, "Type Ctrl+C to stop the program.\n");

    while (!terminate)
        cmd_divert_poll(ctl, dev, map);

    cmd_divert_term(ctl, map);
    hidpp_device_close(dev);
    hidpp_close(rcv);

    return HIDPP_OK;
}

static int cmd_poll_handle(
    hidppctl_t *ctl, hidpp_device_t *dev, struct hidpp_event *e
) {
    pf_cli_t *cli = ctl->cli;
    pf_cli_cprintf(cli, PF_CLI_BOLD, "%s ", eventNames[e->type]);

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

static int cmd_poll(hidppctl_t *ctl) {
    char *masks = ctl->options->poll.masks;

    hidpp_receiver_t *rcv = open_receiver(ctl, ctl->options->receiver);
    hidpp_device_t *dev = open_device(ctl, rcv, ctl->options->device);
    struct hidpp_event e;

    if (!dev || !rcv)
        return HIDPP_EIO;

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

    hidpp_device_close(dev);
    hidpp_close(rcv);
    return HIDPP_OK;
}

static int cmd_remap(hidppctl_t *ctl) {
    uint16_t ctrl = ctl->options->remap.ctrlId;
    uint16_t remap = ctl->options->remap.remapId;

    hidpp_receiver_t *rcv = open_receiver(ctl, ctl->options->receiver);
    hidpp_device_t *dev = open_device(ctl, rcv, ctl->options->device);
    hidpp_keymap_t *map;

    if (!dev || !rcv)
        return HIDPP_EIO;

    if (!(map = hidpp_keymap(dev))) {
        pf_cli_errorf(
            ctl->cli, "Specified device doesn't support control remapping."
        );
        return HIDPP_EIO;
    }

    if (hidpp_keymap_remap(map, ctrl, remap)) {
        pf_cli_errorf(
            ctl->cli, "Unable to remap control: %ls", hidpp_error(rcv)
        );
        return HIDPP_EIO;
    }

    hidpp_device_close(dev);
    hidpp_close(rcv);
    return HIDPP_OK;
}

static int hidppctl_init(hidppctl_t *ctl) {
    if (hidpp_init(NULL)) {
        pf_cli_errorf(ctl->cli, "Unable to initialize the hidpp library!");
        return HIDPP_EIO;
    }

    if (ctl->options->requiresInput) {
        if (!(ctl->input = hidpp_input_new(NULL))) {
            pf_cli_errorf(ctl->cli, "Unable to simulate input!");
            hidpp_exit();
            return HIDPP_EIO;
        }
    }

    return HIDPP_OK;
}

static void hidppctl_free(hidppctl_t *ctl) {
    if (ctl->options->requiresInput)
        hidpp_input_free(ctl->input);
    hidpp_exit();
}

static int hidppctl_run(hidppctl_t *ctl) {
    int rc;

    if ((rc = parse_args(ctl)))
        return rc;

    if (ctl->options->help)
        return print_help(ctl);

    if (hidppctl_init(ctl))
        return HIDPP_EIO;

    int result = HIDPP_ENOSYS;

    switch (ctl->options->command) {
        /* clang-format off */
    case HIDPPCTL_INFO:   result = cmd_info(ctl);   break;
    case HIDPPCTL_DIVERT: result = cmd_divert(ctl); break;
    case HIDPPCTL_POLL:   result = cmd_poll(ctl);   break;
    case HIDPPCTL_REMAP:  result = cmd_remap(ctl);  break;
    default:              result = HIDPP_ENOSYS;    break;
    /* clang-format off */
    }

    hidppctl_free(ctl);
    return result;
}

int main(int argc, char *argv[]) {
    struct hidppctl_opt options;
    pf_argparser_t argparser;
    pf_cli_t cli;

    hidppctl_t state;
    state.options = &options;
    state.argparser = &argparser;
    state.cli = &cli;

    pf_cli_init(state.cli, NULL);
    pf_argparser_init(state.argparser, argc, argv);

    int rc = hidppctl_run(&state);

    pf_argparser_free(state.argparser);
    return rc;
}