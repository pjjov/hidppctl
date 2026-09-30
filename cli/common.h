/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#ifndef HIDPPCTL_COMMON_H
#define HIDPPCTL_COMMON_H

#include <hidpp.h>
#include <pf_cli.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HIDPP_MAX_DIVERT 32
#define HIDPP_MAX_MODS 8

typedef struct pf_argparser pf_argparser_t;

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

struct diversion {
    uint16_t ctrl;
    char state;
    int key;
    int count;
    int mods[HIDPP_MAX_MODS];
};

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

extern const char *hidppctl_event_names[HIDPP__EVENT_MAX];

int hidppctl_print_help(hidppctl_t *ctl);
int hidppctl_parse_args(hidppctl_t *ctl);

#ifdef __cplusplus
}
#endif

#endif /* HIDPPCTL_COMMON_H */