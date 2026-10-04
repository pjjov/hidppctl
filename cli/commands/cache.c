/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

#include <limits.h>
#include <pf_filesystem.h>
#include <pf_io.h>
#include <stdio.h>
#include <string.h>

#define BUFFER_SIZE 8192

static int get_cache_path(hidpp_device_t *dev, char *path) {
    uint64_t cacheId = hidpp_cache_id(dev);
    char dir[PATH_MAX];

    if (pf_cachedir(dir, PATH_MAX) < 0)
        return HIDPP_EIO;

    int res = snprintf(
        path,
        PATH_MAX,
#ifdef PF_FS_WIN32
        "%s\\hidppctl\\%s-%llu.bin",
#else
        "%s/hidppctl/%s-%llu.bin",
#endif
        dir,
        HIDPP_VERSION_STRING,
        (unsigned long long)cacheId
    );

    if (res >= PATH_MAX || res < 0)
        return HIDPP_ENOMEM;

    return HIDPP_OK;
}

static int cmd_cache_collect(hidppctl_t *ctl, const char *path) {
    char buffer[BUFFER_SIZE];
    size_t size = BUFFER_SIZE;
    int rc;

    hidpp_cache_collect(ctl->device);

    rc = hidpp_cache_save(ctl->device, buffer, &size);

    if ((rc = hidpp_cache_save(ctl->device, buffer, &size))) {
        pf_cli_errorf(ctl->cli, "Failed to retrieve the cache contents.");
        return rc;
    }

    if ((rc = pf_writeall(path, buffer, size))) {
        pf_cli_errorf(
            ctl->cli, "Failed to write the cache contents to '%s'.", path
        );
        return rc;
    }

    return HIDPP_OK;
}

static int cmd_cache_clear(hidppctl_t *ctl, const char *path) {
    return HIDPP_ENOSYS;
}

int cmd_cache(hidppctl_t *ctl) {
    char path[PATH_MAX];
    int rc;

    if ((rc = get_cache_path(ctl->device, path))) {
        pf_cli_errorf(ctl->cli, "Unable to compute the cache file path.");
        return rc;
    }

    if (ctl->options->cache.collect)
        return cmd_cache_collect(ctl, path);
    if (ctl->options->cache.clear)
        return cmd_cache_clear(ctl, path);
    return HIDPP_ENOSYS;
}