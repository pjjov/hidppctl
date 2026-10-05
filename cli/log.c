/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "common.h"

#include <limits.h>
#include <pf_filesystem.h>
#include <stdio.h>
#include <time.h>

#ifdef PF_FS_WIN32
    #define HIDPPCTL_LOG_FMT "%s\\hidppctl-%llu.log"
#else
    #define HIDPPCTL_LOG_FMT "%s/hidppctl-%llu.log"
#endif

static int fallback_log_path(hidpp_device_t *dev, char *buf, size_t size) {
    char dir[PATH_MAX];

    if (pf_runtimedir(dir, PATH_MAX) < 0)
        return HIDPP_EIO;

    time_t now = time(NULL);

    int res = snprintf(
        buf, size, HIDPPCTL_LOG_FMT, dir, (unsigned long long)now
    );

    if (res >= PATH_MAX || res < 0)
        return HIDPP_ENOMEM;

    return HIDPP_OK;
}

static int build_log_path(hidppctl_t *ctl) {
    const char *path = ctl->options->log.path;

    if (!path) {
        ctl->logPath = malloc(PATH_MAX);

        if (fallback_log_path(ctl->device, ctl->logPath, PATH_MAX)) {
            free(ctl->logPath);
            ctl->logPath = NULL;
            return HIDPP_EINVAL;
        }
    } else {
        ctl->logPath = strdup(path);
    }

    return HIDPP_OK;
}

static inline size_t packet_length(int kind) {
    switch (kind) {
    case HIDPP_KIND_SHORT:
        return HIDPP_LEN_SHORT;
    case HIDPP_KIND_LONG:
        return HIDPP_LEN_LONG;
    case HIDPP_KIND_XLONG:
        return HIDPP_LEN_XLONG;
    default:
        return 0;
    }
}

static int log_hook(
    hidpp_receiver_t *rcv, hidpp_packet_t *req, hidpp_packet_t *res, void *user
) {
    hidppctl_t *ctl = user;
    hidpp_packet_t *pkt = res ? res : req;

    if (!pkt)
        return HIDPP_EINVAL;

    fprintf(
        ctl->log,
        "%c%d(%d,%d,%d):",
        res ? 'R' : 'S',
        pkt->device,
        pkt->kind,
        pkt->feat,
        pkt->func
    );

    for (size_t i = 0; i < packet_length(pkt->kind); i++)
        fprintf(ctl->log, " %.2x", pkt->params[i]);
    fputc('\n', ctl->log);

    return HIDPP_OK;
}

void hidppctl_open_log(hidppctl_t *ctl) {
    if (build_log_path(ctl))
        return;

    if (!(ctl->log = fopen(ctl->logPath, "wb")))
        return;

    hidpp_set_protocol_hook(ctl->receiver, log_hook, ctl);
    fputs(
        "# S - send request, R - receive response, followed by device index\n"
        "# (kind, featureIndex, function): packet params\n",
        ctl->log
    );

    pf_cli_verbosef(
        ctl->cli, 1, "Logging device interaction to '%s'\n", ctl->logPath
    );
}

void hidppctl_close_log(hidppctl_t *ctl) {
    if (ctl->log) {
        fclose(ctl->log);
        ctl->log = NULL;
        hidpp_set_protocol_hook(ctl->receiver, NULL, NULL);
    }
}