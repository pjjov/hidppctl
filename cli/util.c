/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "common.h"

#include <ctype.h>
#include <errno.h>
#include <pf_argparse.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>

int cli_fail(hidppctl_t *ctl, int rc, const char *what) {
    int code = ctl->receiver ? hidpp_error_code(ctl->receiver) : 0;

    if (rc == HIDPP_ENOSYS) {
        pf_cli_errorf(
            ctl->cli, "Unable to %s: the device does not support it.\n", what
        );
    } else if (code) {
        pf_cli_errorf(
            ctl->cli,
            "Unable to %s: %s (%s).\n",
            what,
            hidpp_error_str(rc),
            hidpp_protocol_error_str(code)
        );
    } else {
        pf_cli_errorf(
            ctl->cli, "Unable to %s: %s.\n", what, hidpp_error_str(rc)
        );
    }

    return rc;
}

int cli_unsupported(hidppctl_t *ctl, const char *what) {
    pf_cli_errorf(ctl->cli, "The device does not support %s.\n", what);
    return HIDPP_ENOSYS;
}

int cli_usage(hidppctl_t *ctl, const char *fmt, ...) {
    va_list args;
    char buf[256];

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    pf_cli_errorf(ctl->cli, "error: %s\n", buf);
    return HIDPP_EINVAL;
}

int cli_is(const char *a, const char *b) {
    if (!a || !b)
        return 0;

    for (; *a && *b; a++, b++)
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return 0;

    return *a == *b;
}

const char *cli_str(const char *s) { return s ? s : "unknown"; }
const char *cli_yesno(int value) { return value ? "yes" : "no"; }
const char *cli_onoff(int value) { return value ? "on" : "off"; }

const char *cli_name(const struct name_value *table, int value) {
    for (; table->name; table++)
        if (table->value == value)
            return table->name;

    return NULL;
}

void cli_flag_names(
    const struct name_value *table, unsigned value, char *buf, size_t size
) {
    size_t used = 0;
    buf[0] = '\0';

    for (; table->name; table++) {
        if (!table->value || (value & table->value) != (unsigned)table->value)
            continue;

        int n = snprintf(
            buf + used, size - used, "%s%s", used ? "," : "", table->name
        );

        if (n < 0 || (size_t)n >= size - used)
            break;
        used += n;
    }

    if (!used)
        snprintf(buf, size, "none");
}

char *cli_split(char *arg, char **value) {
    char *eq = strchr(arg, '=');

    if (eq) {
        *eq = '\0';
        *value = eq + 1;
    } else {
        *value = NULL;
    }

    return arg;
}

int cli_long(
    hidppctl_t *ctl,
    const char *s,
    const char *what,
    long min,
    long max,
    long *out
) {
    char *end;

    errno = 0;
    long v = strtol(s, &end, 0);

    if (end == s || *end || errno == ERANGE || v < min || v > max) {
        pf_cli_errorf(
            ctl->cli,
            "error: %s must be a number between %ld and %ld; got '%s'.\n",
            what,
            min,
            max,
            s
        );
        return HIDPP_EINVAL;
    }

    *out = v;
    return HIDPP_OK;
}

int cli_bool(hidppctl_t *ctl, const char *s, const char *what, int *out) {
    static const struct name_value values[] = {
        { "on", 1 },   { "off", 0 },   { "yes", 1 },    { "no", 0 },
        { "true", 1 }, { "false", 0 }, { "enable", 1 }, { "disable", 0 },
        { "1", 1 },    { "0", 0 },     { NULL, 0 },
    };

    for (const struct name_value *v = values; v->name; v++) {
        if (cli_is(s, v->name)) {
            *out = v->value;
            return HIDPP_OK;
        }
    }

    return cli_usage(ctl, "%s must be 'on' or 'off'; got '%s'.", what, s);
}

int cli_named(
    hidppctl_t *ctl,
    const char *s,
    const char *what,
    const struct name_value *table,
    int *out
) {
    char *end;

    for (const struct name_value *v = table; v->name; v++) {
        if (cli_is(s, v->name)) {
            *out = v->value;
            return HIDPP_OK;
        }
    }

    long v = strtol(s, &end, 0);

    if (end != s && !*end && v >= 0 && v <= 0xFFFF) {
        *out = (int)v;
        return HIDPP_OK;
    }

    char names[256] = "";
    size_t used = 0;

    for (const struct name_value *t = table; t->name; t++) {
        int n = snprintf(
            names + used,
            sizeof(names) - used,
            "%s%s",
            used ? ", " : "",
            t->name
        );

        if (n < 0 || (size_t)n >= sizeof(names) - used)
            break;
        used += n;
    }

    return cli_usage(ctl, "Unknown %s '%s'; expected %s.", what, s, names);
}

/* "a,b,c", "none" or a number. */
int cli_flags(
    hidppctl_t *ctl,
    const char *s,
    const char *what,
    const struct name_value *table,
    int *out
) {
    char buf[256];
    char *cursor = buf, *tok;
    int acc = 0;

    if (cli_is(s, "none")) {
        *out = 0;
        return HIDPP_OK;
    }

    if (strlen(s) >= sizeof(buf))
        return cli_usage(ctl, "%s is too long.", what);

    strcpy(buf, s);

    while ((tok = pf_next_token(&cursor, ','))) {
        int v;

        if (!*tok)
            continue;
        if (cli_named(ctl, tok, what, table, &v))
            return HIDPP_EINVAL;
        acc |= v;
    }

    *out = acc;
    return HIDPP_OK;
}

/* "RRGGBB" or "#RRGGBB". */
int cli_color(hidppctl_t *ctl, const char *s, struct hidpp_color *out) {
    const char *hex = s[0] == '#' ? s + 1 : s;
    char *end;

    if (strlen(hex) != 6)
        return cli_usage(ctl, "Color '%s' must be written as RRGGBB.", s);

    unsigned long v = strtoul(hex, &end, 16);

    if (*end)
        return cli_usage(ctl, "Color '%s' must be written as RRGGBB.", s);

    out->red = (v >> 16) & 0xFF;
    out->green = (v >> 8) & 0xFF;
    out->blue = v & 0xFF;
    return HIDPP_OK;
}

/* A control is either a number or a name known to the keymap. */
int cli_control(hidppctl_t *ctl, const char *s, uint16_t *out) {
    char *end;
    long id = strtol(s, &end, 0);

    if (end == s || *end) {
        id = hidpp_keymap_from_name(s);

        if (id <= 0)
            return cli_usage(ctl, "Unknown control '%s'.", s);
    }

    if (id < 0 || id > UINT16_MAX)
        return cli_usage(ctl, "Control ids must be below 65536.");

    *out = (uint16_t)id;
    return HIDPP_OK;
}

int cli_host(hidppctl_t *ctl, const char *s, uint8_t *out) {
    long v;

    if (cli_is(s, "current")) {
        *out = HIDPP_HOST_CURRENT;
        return HIDPP_OK;
    }

    if (cli_long(ctl, s, "host", 0, 0xFE, &v))
        return HIDPP_EINVAL;

    *out = (uint8_t)v;
    return HIDPP_OK;
}

void cli_hex(hidppctl_t *ctl, const uint8_t *data, size_t size) {
    for (size_t i = 0; i < size; i++)
        pf_cli_printf(ctl->cli, "%s%.2x", i ? " " : "", data[i]);
}

static volatile sig_atomic_t terminated = 0;

static void signal_handler(int sig) {
    (void)sig;
    terminated = 1;
}

void cli_catch_signals(void) {
    terminated = 0;
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
}

int cli_terminated(void) { return terminated; }
