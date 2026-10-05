/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "common.h"

#include <limits.h>
#include <pf_filesystem.h>
#include <pf_io.h>
#include <time.h>

#ifdef PF_FS_WIN32
    #define HIDPPCTL_CACHE_FMT "%s\\hidppctl-%s-%llu.bin"
#else
    #define HIDPPCTL_CACHE_FMT "%s/hidppctl-%s-%llu.bin"
#endif

int hidppctl_cache_path(hidpp_device_t *dev, char *buf, size_t size) {
    uint64_t cacheId = hidpp_cache_id(dev);
    char dir[PATH_MAX];

    if (pf_cachedir(dir, PATH_MAX) < 0)
        return HIDPP_EIO;

    int res = snprintf(
        buf,
        size,
        HIDPPCTL_CACHE_FMT,
        dir,
        HIDPP_VERSION_STRING,
        (unsigned long long)cacheId
    );

    if (res >= PATH_MAX || res < 0)
        return HIDPP_ENOMEM;

    return HIDPP_OK;
}

static int build_cache_path(hidppctl_t *ctl) {
    const char *path = ctl->options->cache.path;

    if (!path) {
        ctl->cachePath = malloc(PATH_MAX);

        if (hidppctl_cache_path(ctl->device, ctl->cachePath, PATH_MAX)) {
            free(ctl->cachePath);
            ctl->cachePath = NULL;
            return HIDPP_EINVAL;
        }
    } else {
        ctl->cachePath = strdup(path);
    }

    return HIDPP_OK;
}

void hidppctl_load_cache(hidppctl_t *ctl) {
    if (build_cache_path(ctl))
        return;

    void *buffer;
    size_t size;

    if (pf_readall(ctl->cachePath, &buffer, &size))
        return;

    hidpp_cache_load(ctl->device, buffer, size);
    free(buffer);

    pf_cli_verbosef(ctl->cli, 1, "Loaded cache from '%s'\n", ctl->cachePath);
}

void hidppctl_save_cache(hidppctl_t *ctl) {
    char buffer[CACHE_BUFFER_SIZE];
    size_t size = CACHE_BUFFER_SIZE;

    memset(buffer, 0, size);

    if (hidpp_cache_save(ctl->device, buffer, &size))
        return;

    pf_writeall(ctl->cachePath, buffer, size);

    pf_cli_verbosef(ctl->cli, 1, "Saved cache to '%s'\n", ctl->cachePath);
}