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
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CACHE_BUFFER_SIZE 65535
#define HIDPP_MAX_DIVERT 32
#define HIDPP_MAX_MODS 8

typedef struct pf_argparser pf_argparser_t;

enum hidppctl_command {
    HIDPPCTL_NONE = 0,
    HIDPPCTL_ACTION,
    HIDPPCTL_BACKLIGHT,
    HIDPPCTL_BATTERY,
    HIDPPCTL_BRIGHTNESS,
    HIDPPCTL_CACHE,
    HIDPPCTL_CROWN,
    HIDPPCTL_DISABLE_KEYS,
    HIDPPCTL_DIVERT,
    HIDPPCTL_DPI,
    HIDPPCTL_EQ,
    HIDPPCTL_FN_INVERSION,
    HIDPPCTL_HOST,
    HIDPPCTL_ILLUMINATION,
    HIDPPCTL_KEYMAP,
    HIDPPCTL_LED,
    HIDPPCTL_LIST_EVENTS,
    HIDPPCTL_LIST_FEATURES,
    HIDPPCTL_LIST_KEYCODES,
    HIDPPCTL_MODE,
    HIDPPCTL_NICKNAME,
    HIDPPCTL_PERKEY,
    HIDPPCTL_PLATFORM,
    HIDPPCTL_POINTER,
    HIDPPCTL_POLL,
    HIDPPCTL_REMAP,
    HIDPPCTL_REPORT_RATE,
    HIDPPCTL_SCROLL,
    HIDPPCTL_SIDETONE,
    HIDPPCTL_SMARTSHIFT,
    HIDPPCTL_SOLAR,
    HIDPPCTL_STATUS,
    HIDPPCTL_THUMBWHEEL,
    HIDPPCTL_TOUCH_MOUSE,
    HIDPPCTL_TOUCHPAD,
    HIDPPCTL_WHEEL,

    hidppctl_command_max,
};

/** What a subcommand needs to be able to run. */
enum hidppctl_need {
    HIDPPCTL_NEEDS_NOTHING,
    HIDPPCTL_NEEDS_ANYTHING, /* receiver and device are both optional */
    HIDPPCTL_NEEDS_RECEIVER, /* device is optional */
    HIDPPCTL_NEEDS_DEVICE,   /* receiver and device */
};

struct hidppctl_t;

struct hidppctl_command_spec {
    const char *name;
    int (*run)(struct hidppctl_t *ctl);
    int needs;            /* enum hidppctl_need */
    int minArgs, maxArgs; /* maxArgs < 0 means unlimited */
    const char *args;     /* usage of the arguments */
    const char *summary;  /* one line */
    const char *help;     /* long description */
};

extern const struct hidppctl_command_spec hidppctl_commands[];

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

    hidpp_bool_t requiresInput;
    hidpp_bool_t requiresDevice;
    hidpp_bool_t requiresReceiver;
    hidpp_bool_t requiresSocket;
    hidpp_bool_t requiresCache;
    hidpp_bool_t requiresLog;

    hidpp_input_t *input;
    hidpp_receiver_t *receiver;
    hidpp_device_t *device;
    char *cachePath;
    char *logPath;
    FILE *log;
    int socket;
} hidppctl_t;

struct hidppctl_opt {
    pf_bool help;
    pf_bool version;
    pf_bool quiet;
    pf_bool verbose;

    int command;

    char **paramv;
    int paramc;

    char *receiver;
    uint8_t device;
    uint8_t swid;
    uint8_t host;   /* --host, HIDPP_HOST_CURRENT by default */
    uint8_t sensor; /* --sensor */
    uint8_t zone;   /* --zone */
    int timeout;
    int interface;

    hidpp_bool_t setSwid;
    hidpp_bool_t setTimeout;
    hidpp_bool_t setInterface;
    hidpp_bool_t setReceiver;
    hidpp_bool_t setDevice;
    hidpp_bool_t setSocket;

    struct {
        hidpp_bool_t isPort; /* TCP: connect to `host`:`port` */
        int port;
        char host[256];
        char *path;
        char *value;
    } socket;

    struct {
        uint16_t ctrlId;
        uint16_t remapId;
    } remap;

    struct {
        char masks[HIDPP__EVENT_MAX];
    } poll;

    struct {
        hidpp_bool_t needsInput;
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
};

extern const char *hidppctl_event_names[HIDPP__EVENT_MAX];

int hidppctl_print_help(hidppctl_t *ctl);
int hidppctl_parse_args(hidppctl_t *ctl);
int hidppctl_cache_path(hidpp_device_t *dev, char *buf, size_t size);

/* ------------------------------------------------------------------------ */
/* Helpers shared by the subcommands (util.c)                                */
/* ------------------------------------------------------------------------ */

/** A name which stands for a number. */
struct name_value {
    const char *name;
    int value;
};

/** Reports that `what` failed with the error `rc`; returns `rc`. */
int cli_fail(hidppctl_t *ctl, int rc, const char *what);

/** Reports that the device lacks a feature; returns HIDPP_ENOSYS. */
int cli_unsupported(hidppctl_t *ctl, const char *what);

/** Reports a usage error; returns HIDPP_EINVAL. */
int cli_usage(hidppctl_t *ctl, const char *fmt, ...);

/** Parsers. They print a message and return HIDPP_EINVAL on a bad input. */
int cli_long(
    hidppctl_t *ctl,
    const char *s,
    const char *what,
    long min,
    long max,
    long *out
);
int cli_bool(hidppctl_t *ctl, const char *s, const char *what, int *out);
int cli_named(
    hidppctl_t *ctl,
    const char *s,
    const char *what,
    const struct name_value *table,
    int *out
);
int cli_flags(
    hidppctl_t *ctl,
    const char *s,
    const char *what,
    const struct name_value *table,
    int *out
);
int cli_color(hidppctl_t *ctl, const char *s, struct hidpp_color *out);
int cli_control(hidppctl_t *ctl, const char *s, uint16_t *out);
int cli_host(hidppctl_t *ctl, const char *s, uint8_t *out);

/** Splits "key=value" in place; the value is NULL if there is no '='. */
char *cli_split(char *arg, char **value);

/** Case-insensitive comparison, 1 if equal. */
int cli_is(const char *a, const char *b);

/** Name of `value` in the table or NULL. */
const char *cli_name(const struct name_value *table, int value);

/** Writes the names of the bits set in `value` ("none" if there are none). */
void cli_flag_names(
    const struct name_value *table, unsigned value, char *buf, size_t size
);

const char *cli_str(const char *s);
const char *cli_yesno(int value);
const char *cli_onoff(int value);

/** Prints `size` bytes of `data` in hexadecimal. */
void cli_hex(hidppctl_t *ctl, const uint8_t *data, size_t size);

/** Installs the SIGINT and SIGTERM handlers; see cli_terminated(). */
void cli_catch_signals(void);
int cli_terminated(void);

#if defined(_WIN32) || defined(_MSC_VER)
    /* Windows environment fallback to the secure CRT name */
    #define strdup _strdup
#else
    /* Check if the system is running a POSIX compliant environment */
    #if !defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)
/* Pure fallback for systems without built-in strdup */
static inline char *fallback_strdup(const char *s) {
    if (s == NULL)
        return NULL;
    size_t len = strlen(s) + 1;
    char *p = (char *)malloc(len);
    if (p != NULL) {
        memcpy(p, s, len);
    }
    return p;
}
        #define strdup fallback_strdup
    #endif
#endif

#ifdef __cplusplus
}
#endif

#endif /* HIDPPCTL_COMMON_H */