/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

#include <limits.h>
#include <pf_io.h>
#include <stdio.h>
#include <string.h>

static int cache_collect(
    hidppctl_t *ctl, hidpp_device_t *dev, const char *path
) {
    char buffer[CACHE_BUFFER_SIZE];
    size_t size = CACHE_BUFFER_SIZE;
    int rc;

    memset(buffer, 0, size);
    hidpp_cache_collect(dev);

    if ((rc = hidpp_cache_save(dev, buffer, &size))) {
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

int hidppctl_cache_collect(hidppctl_t *ctl, hidpp_device_t *dev) {
    char path[PATH_MAX];
    int rc;

    if ((rc = hidppctl_cache_path(dev, path, PATH_MAX))) {
        pf_cli_errorf(ctl->cli, "Unable to compute the cache file path.\n");
        return rc;
    }

    return cache_collect(ctl, dev, path);
}

static int cmd_cache_clear(hidppctl_t *ctl, const char *path) {
    if (remove(path)) {
        pf_cli_errorf(ctl->cli, "Unable to remove '%s'.\n", path);
        return HIDPP_ENOENT;
    }

    pf_cli_printf(ctl->cli, "Removed '%s'.\n", path);
    return HIDPP_OK;
}

int cmd_cache(hidppctl_t *ctl) {
    char path[PATH_MAX];
    int rc;

    if ((rc = hidppctl_cache_path(ctl->device, path, PATH_MAX))) {
        pf_cli_errorf(ctl->cli, "Unable to compute the cache file path.\n");
        return rc;
    }

    if (ctl->options->cache.collect)
        return cache_collect(ctl, ctl->device, path);
    if (ctl->options->cache.clear)
        return cmd_cache_clear(ctl, path);

    /* No action: tell where the cache is. */
    pf_cli_printf(ctl->cli, "%s\n", path);
    return HIDPP_OK;
}