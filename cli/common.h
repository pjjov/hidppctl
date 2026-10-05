/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#ifndef HIDPPCTL_COMMON_H
#define HIDPPCTL_COMMON_H

#include <assert.h>
#include <hidpp.h>
#include <pf_cli.h>
#include <pf_macro.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CACHE_BUFFER_SIZE 65535
#define HIDPP_MAX_DIVERT 32
#define HIDPP_MAX_MODS 8

typedef struct pf_argparser pf_argparser_t;

enum hidppctl_command {
    HIDPPCTL_NONE = 0,
    HIDPPCTL_CACHE,
    HIDPPCTL_DIVERT,
    HIDPPCTL_KEYMAP,
    HIDPPCTL_LIST_EVENTS,
    HIDPPCTL_LIST_FEATURES,
    HIDPPCTL_LIST_KEYCODES,
    HIDPPCTL_POLL,
    HIDPPCTL_REMAP,
    HIDPPCTL_STATUS,

    hidppctl_command_max,
    hidppctl_command_sentinel = 2,
};

enum hidppctl_subject {
    HIDPPCTL_ALL,
    HIDPPCTL_RECEIVER,
    HIDPPCTL_DEVICE,
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
    hidpp_receiver_t *receiver;
    hidpp_device_t *device;
    char *cachePath;
    char *logPath;
    FILE *log;
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

    hidpp_bool_t setSwid;
    hidpp_bool_t setTimeout;
    hidpp_bool_t setInterface;

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

    struct {
        hidpp_bool_t clear;
        hidpp_bool_t collect;
        pf_bool disabled;
        char *path;
    } cache;

    struct {
        pf_bool disabled;
        char *path;
    } log;

    /* used by main.c */
    hidpp_bool_t requiresInput;
    hidpp_bool_t requiresDevice;
    hidpp_bool_t requiresReceiver;
    hidpp_bool_t requiresCache;
    hidpp_bool_t requiresLog;
};

extern const char *hidppctl_event_names[HIDPP__EVENT_MAX];

int hidppctl_print_help(hidppctl_t *ctl);
int hidppctl_parse_args(hidppctl_t *ctl);
int hidppctl_cache_path(hidpp_device_t *dev, char *buf, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* HIDPPCTL_COMMON_H */