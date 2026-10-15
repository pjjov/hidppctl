/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

/*  Aliases are stored in a plain text file, one alias per line:

        # comment
        mouse receiver=046d:c548 device=1 timeout=500
        remote socket=192.168.1.5:4000 device=2

    Values cannot contain white space.
*/

#include "common.h"

#include <ctype.h>
#include <limits.h>
#include <pf_filesystem.h>
#include <pf_io.h>
#include <stdio.h>

#ifdef PF_FS_WIN32
    #define ALIAS_FILE "%s\\hidppctl-aliases.conf"
#else
    #define ALIAS_FILE "%s/hidppctl-aliases.conf"
#endif

void hidppctl_alias_init(struct hidppctl_alias *alias, const char *name) {
    memset(alias, 0, sizeof(*alias));
    alias->device = alias->interface = alias->swid = alias->timeout = -1;

    if (name)
        snprintf(alias->name, sizeof(alias->name), "%s", name);
}

static int parse_int(const char *s, long min, long max, int *out) {
    char *end;
    long v = strtol(s, &end, 0);

    if (end == s || *end || v < min || v > max)
        return -1;

    *out = (int)v;
    return 0;
}

static int copy_value(char *dst, size_t size, const char *value) {
    if (!*value || strlen(value) >= size)
        return -1;

    strcpy(dst, value);
    return 0;
}

/* Returns NULL on success or a description of the problem. */
const char *hidppctl_alias_set(
    struct hidppctl_alias *a, const char *key, const char *value
) {
    if (0 == strcmp(key, "receiver")) {
        if (copy_value(a->receiver, sizeof(a->receiver), value))
            return "invalid receiver";
        a->socket[0] = '\0';
    } else if (0 == strcmp(key, "socket")) {
        if (copy_value(a->socket, sizeof(a->socket), value))
            return "invalid socket";
        a->receiver[0] = '\0';
        a->interface = -1;
    } else if (0 == strcmp(key, "device")) {
        if (parse_int(value, 1, 255, &a->device)
            || (a->device > 6 && a->device != 0xFF))
            return "device must be between 1 and 6, or 255";
    } else if (0 == strcmp(key, "interface")) {
        if (parse_int(value, 0, INT_MAX, &a->interface))
            return "invalid interface";
    } else if (0 == strcmp(key, "swid")) {
        if (parse_int(value, 0, 15, &a->swid))
            return "swid must be between 0 and 15";
    } else if (0 == strcmp(key, "timeout")) {
        if (parse_int(value, 1, INT_MAX, &a->timeout))
            return "invalid timeout";
    } else {
        return "unknown key (expected receiver, socket, device, interface, "
               "swid or timeout)";
    }

    return NULL;
}

void hidppctl_alias_format(
    const struct hidppctl_alias *a, char *buf, size_t size
) {
    size_t n = 0;

#define ADD(...)                                              \
    do {                                                      \
        if (n < size) {                                       \
            int w = snprintf(buf + n, size - n, __VA_ARGS__); \
            n += w > 0 ? (size_t)w : 0;                       \
        }                                                     \
    } while (0)

    buf[0] = '\0';
    if (a->receiver[0])
        ADD(" receiver=%s", a->receiver);
    if (a->socket[0])
        ADD(" socket=%s", a->socket);
    if (a->device >= 0)
        ADD(" device=%d", a->device);
    if (a->interface >= 0)
        ADD(" interface=%d", a->interface);
    if (a->swid >= 0)
        ADD(" swid=%d", a->swid);
    if (a->timeout >= 0)
        ADD(" timeout=%d", a->timeout);
#undef ADD
}

static int alias_path(hidppctl_t *ctl, char *buf, size_t size) {
    char dir[PATH_MAX];

    if (ctl->options->aliasFile) {
        if (strlen(ctl->options->aliasFile) >= size)
            return HIDPP_ENOMEM;
        strcpy(buf, ctl->options->aliasFile);
        return HIDPP_OK;
    }

    if (pf_confdir(dir, sizeof(dir)) < 0)
        return HIDPP_EIO;

    int n = snprintf(buf, size, ALIAS_FILE, dir);
    return n < 0 || (size_t)n >= size ? HIDPP_ENOMEM : HIDPP_OK;
}

int hidppctl_alias_load(
    hidppctl_t *ctl, struct hidppctl_alias *list, int max, int *count
) {
    char path[PATH_MAX];
    char *data, *line, *next;
    size_t size;
    int lineNo = 0;

    *count = 0;

    if (alias_path(ctl, path, sizeof(path))) {
        pf_cli_errorf(ctl->cli, "Unable to find the aliases file.\n");
        return HIDPP_EIO;
    }

    /* A missing file just means that there are no aliases. */
    if (!pf_exists(path))
        return HIDPP_OK;

    if (pf_readall(path, (void **)&data, &size)) {
        pf_cli_errorf(ctl->cli, "Unable to read '%s'.\n", path);
        return HIDPP_EIO;
    }

    for (line = data; line; line = next) {
        char *tok, *save;
        struct hidppctl_alias a;

        if ((next = strchr(line, '\n')))
            *next++ = '\0';
        lineNo++;

        while (isspace((unsigned char)*line))
            line++;
        if (!*line || *line == '#')
            continue;

        /* split the line in place */
        tok = line;
        while (*tok && !isspace((unsigned char)*tok))
            tok++;
        save = tok;
        if (*tok)
            *tok++ = '\0';

        if (strlen(line) >= sizeof(a.name)) {
            pf_cli_errorf(
                ctl->cli, "%s:%d: alias name is too long.\n", path, lineNo
            );
            continue;
        }

        hidppctl_alias_init(&a, line);
        (void)save;

        while (*tok) {
            char *val, *end;
            const char *err;

            while (isspace((unsigned char)*tok))
                tok++;
            if (!*tok)
                break;

            end = tok;
            while (*end && !isspace((unsigned char)*end))
                end++;
            if (*end)
                *end++ = '\0';

            if (!(val = strchr(tok, '='))) {
                pf_cli_errorf(
                    ctl->cli,
                    "%s:%d: expected key=value, got '%s'.\n",
                    path,
                    lineNo,
                    tok
                );
            } else {
                *val++ = '\0';
                if ((err = hidppctl_alias_set(&a, tok, val)))
                    pf_cli_errorf(
                        ctl->cli, "%s:%d: %s: %s.\n", path, lineNo, tok, err
                    );
            }

            tok = end;
        }

        if (*count >= max) {
            pf_cli_errorf(ctl->cli, "%s: too many aliases.\n", path);
            break;
        }

        list[(*count)++] = a;
    }

    free(data);
    return HIDPP_OK;
}

int hidppctl_alias_save(
    hidppctl_t *ctl, const struct hidppctl_alias *list, int count
) {
    char path[PATH_MAX];
    char line[1024];
    FILE *fp;

    if (alias_path(ctl, path, sizeof(path))) {
        pf_cli_errorf(ctl->cli, "Unable to find the aliases file.\n");
        return HIDPP_EIO;
    }

    if (!(fp = fopen(path, "w"))) {
        pf_cli_errorf(ctl->cli, "Unable to write '%s'.\n", path);
        return HIDPP_EIO;
    }

    fputs("# hidppctl aliases: <name> <key>=<value>...\n", fp);

    for (int i = 0; i < count; i++) {
        hidppctl_alias_format(&list[i], line, sizeof(line));
        fprintf(fp, "%s%s\n", list[i].name, line);
    }

    if (fclose(fp)) {
        pf_cli_errorf(ctl->cli, "Unable to write '%s'.\n", path);
        return HIDPP_EIO;
    }

    return HIDPP_OK;
}
