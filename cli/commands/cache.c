/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

#include <limits.h>
#include <linux/limits.h>
#include <pf_io.h>
#include <stdio.h>
#include <string.h>

static int cmd_cache_collect(hidppctl_t *ctl, const char *path) {
    char buffer[CACHE_BUFFER_SIZE];
    size_t size = CACHE_BUFFER_SIZE;
    int rc;

    memset(buffer, 0, size);
    hidpp_cache_collect(ctl->device);

    if ((rc = hidpp_cache_save(ctl->device, buffer, &size))) {
        pf_cli_errorf(ctl->cli, "Failed to retrieve the cache contents.\n");
        return rc;
    }

    if ((rc = pf_writeall(path, buffer, size))) {
        pf_cli_errorf(
            ctl->cli, "Failed to write the cache contents to '%s'.\n", path
        );
        return rc;
    }

    pf_cli_printf(ctl->cli, "Saved cache contents to '%s'.\n", path);
    return HIDPP_OK;
}

static int cmd_cache_clear(hidppctl_t *ctl, const char *path) {
    return remove(path);
}

int cmd_cache(hidppctl_t *ctl) {
    char path[PATH_MAX];
    int rc;

    if ((rc = hidppctl_cache_path(ctl->device, path, PATH_MAX))) {
        pf_cli_errorf(ctl->cli, "Unable to compute the cache file path.\n");
        return rc;
    }

    if (ctl->options->cache.collect)
        return cmd_cache_collect(ctl, path);
    if (ctl->options->cache.clear)
        return cmd_cache_clear(ctl, path);
    return HIDPP_ENOSYS;
}