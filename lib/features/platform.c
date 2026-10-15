/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

/** Section: HID++ feature documentation

    Dual platform (id: 0x4530)

    -----------------------
    Fn 0 - get platform
    -----------------------
    request:
    response:
        uint8_t platform (0 = iOS or macOS, 1 = Android or Windows)

    -----------------------
    Fn 1 - set platform
    -----------------------
    request:
        uint8_t platform
    response:
        uint8_t platform (echo)

    -----------------------
    Event 0 - platform change (by the OS selection button)
    -----------------------
        uint8_t platform

    Multi-platform (id: 0x4531, versions 0 and 1)
    Big endian integers are exchanged, except for the capability mask whose
    first byte holds the lowest bits (like the one of 0x1815).

    -----------------------
    Fn 0 - get feature infos
    -----------------------
    request:
    response:
        uint16_t capabilityMask
        - bit 0 - automatic OS detection
        - bit 1 - set host platform
        uint8_t platformCount
        uint8_t platformDescriptorCount
        uint8_t hostCount
        uint8_t currentHost
        uint8_t currentHostPlatform

    -----------------------
    Fn 1 - get platform descriptor
    -----------------------
    request:
        uint8_t descriptorIndex
    response:
        uint8_t platform
        uint8_t descriptorIndex
        uint16_t osMask
        - bit 0 - Tizen (version 1)
        - bit 8 - Windows
        - bit 9 - Windows embedded
        - bit 10 - Linux
        - bit 11 - Chrome OS
        - bit 12 - Android
        - bit 13 - macOS
        - bit 14 - iOS
        - bit 15 - WebOS (version 1)
        uint8_t fromVersion
        uint8_t fromRevision
        uint8_t toVersion (0.0 to 0.0 covers all versions)
        uint8_t toRevision

    -----------------------
    Fn 2 - get host platform
    -----------------------
    request:
        uint8_t host (0xFF is the current host)
    response:
        uint8_t host
        uint8_t status
        uint8_t platform
        uint8_t platformSource
        uint8_t autoPlatform
        uint8_t autoDescriptor

    -----------------------
    Fn 3 - set host platform
    -----------------------
    request:
        uint8_t host
        uint8_t platform
    response:

    -----------------------
    Event 0 - platform change (current host, changed by the user)
    -----------------------
        uint8_t host
        uint8_t platform
        uint8_t platformSource
*/

#define DUAL_PLATFORM_ID 0x4530
#define MULTI_PLATFORM_ID 0x4531

#define MAX_DESCRIPTORS 16
#define DUAL_PLATFORM_COUNT 2

/* clang-format off */

#define CAP_OS_DETECTION_FLAG  0x0001
#define CAP_SET_PLATFORM_FLAG  0x0002

/* clang-format on */

struct hidpp_feat_platform {
    struct hidpp_feat_base base;
    struct hidpp_platform_info info;
    struct hidpp_platform_descriptor descriptors[MAX_DESCRIPTORS];
};

static void unpack_flags(struct hidpp_platform_info *info) {
    if (!info->isMultiPlatform) {
        info->platformCount = DUAL_PLATFORM_COUNT;
        info->canSet = HIDPP_TRUE;
        return;
    }

    info->hasOsDetection = !!(info->capabilities & CAP_OS_DETECTION_FLAG);
    info->canSet = !!(info->capabilities & CAP_SET_PLATFORM_FLAG);
}

static int query_descriptor(
    hidpp_device_t *dev,
    struct hidpp_feat_platform *pl,
    uint8_t index,
    struct hidpp_platform_descriptor *out
) {
    hidpp_packet_t req, res;
    make_packet(&req, dev, pl->base.featIndex, 1);
    req.params[0] = index;

    int rc;

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    out->platform = res.params[0];
    out->index = index;
    out->osMask = HIDPP_WORD(res.params[2], res.params[3]);
    out->fromVersion = res.params[4];
    out->fromRevision = res.params[5];
    out->toVersion = res.params[6];
    out->toRevision = res.params[7];
    return HIDPP_OK;
}

static int query_info(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_platform *pl = feat;
    pl->info.isMultiPlatform = pl->base.which == 0; /* 0x4531 */
    struct hidpp_platform_info *info = &pl->info;

    if (info->isMultiPlatform) {
        hidpp_packet_t req, res;
        make_packet(&req, dev, pl->base.featIndex, 0);

        int rc;

        if ((rc = hidpp_device_request(dev, &req, &res)))
            return is_transient(dev, rc) ? rc : HIDPP_OK;

        info->capabilities = HIDPP_WORD(res.params[1], res.params[0]);
        info->platformCount = res.params[2];
        info->descriptorCount = res.params[3];
        info->hostCount = res.params[4];

        if (info->descriptorCount > MAX_DESCRIPTORS)
            info->descriptorCount = MAX_DESCRIPTORS;

        for (uint8_t i = 0; i < info->descriptorCount; i++) {
            rc = query_descriptor(dev, pl, i, &pl->descriptors[i]);

            if (rc && is_transient(dev, rc))
                return rc;

            if (rc) {
                info->descriptorCount = i; /* keep what was answered */
                break;
            }
        }
    }

    unpack_flags(info);
    info->initialized = HIDPP_TRUE;
    return HIDPP_OK;
}

static int ensure_init(struct hidpp_feat_platform **out, hidpp_device_t *dev) {
    void *feat;
    int rc = hidpp__feature_init(dev, HIDPP_FEAT_PLATFORM, &feat);
    *out = feat;
    return rc;
}

static size_t save_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_platform *pl = feat;
    uint8_t *start = b;
    (void)dev;

    SAVE_BYTE(b, pl->info.initialized);
    SAVE_WORD(b, pl->info.capabilities);
    SAVE_BYTE(b, pl->info.platformCount);
    SAVE_BYTE(b, pl->info.hostCount);
    SAVE_BYTE(b, pl->info.descriptorCount);

    for (uint8_t i = 0; i < pl->info.descriptorCount; i++) {
        struct hidpp_platform_descriptor *d = &pl->descriptors[i];
        SAVE_BYTE(b, d->platform);
        SAVE_WORD(b, d->osMask);
        SAVE_BYTE(b, d->fromVersion);
        SAVE_BYTE(b, d->fromRevision);
        SAVE_BYTE(b, d->toVersion);
        SAVE_BYTE(b, d->toRevision);
    }

    return b - start;
}

static size_t load_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_platform *pl = feat;
    uint8_t *start = b;
    (void)dev;

    pl->info.initialized = LOAD_BYTE(b);
    pl->info.isMultiPlatform = pl->base.which == 0; /* 0x4531 */
    pl->info.capabilities = LOAD_WORD(b);
    pl->info.platformCount = LOAD_BYTE(b);
    pl->info.hostCount = LOAD_BYTE(b);
    pl->info.descriptorCount = LOAD_BYTE(b);

    if (pl->info.descriptorCount > MAX_DESCRIPTORS)
        pl->info.descriptorCount = MAX_DESCRIPTORS;

    for (uint8_t i = 0; i < pl->info.descriptorCount; i++) {
        struct hidpp_platform_descriptor *d = &pl->descriptors[i];
        d->index = i;
        d->platform = LOAD_BYTE(b);
        d->osMask = LOAD_WORD(b);
        d->fromVersion = LOAD_BYTE(b);
        d->fromRevision = LOAD_BYTE(b);
        d->toVersion = LOAD_BYTE(b);
        d->toRevision = LOAD_BYTE(b);
    }

    unpack_flags(&pl->info);

    return b - start;
}

static int parse_event(
    hidpp_device_t *dev,
    void *feat,
    uint16_t id,
    uint8_t func,
    const hidpp_packet_t *pkt,
    struct hidpp_event *e
) {
    const uint8_t *p = pkt->params;
    (void)dev, (void)feat;

    if (func != 0)
        return HIDPP_ENOENT;

    if (id == DUAL_PLATFORM_ID) {
        e->type = HIDPP_EVENT_PLATFORM;
        e->as.platform.host = HIDPP_HOST_CURRENT;
        e->as.platform.platform = p[0];
        return HIDPP_OK;
    } else if (id == MULTI_PLATFORM_ID) {
        e->type = HIDPP_EVENT_PLATFORM;
        e->as.platform.host = p[0];
        e->as.platform.platform = p[1];
        e->as.platform.source = p[2];
        return HIDPP_OK;
    }

    return HIDPP_ENOENT;
}

const struct hidpp_feat_vt hidpp_feat_platform_vt = {
    FEAT_VT_SIZE(struct hidpp_feat_platform),
    .ids = { MULTI_PLATFORM_ID, DUAL_PLATFORM_ID },
    .query = query_info,
    .saveCache = save_cache,
    .loadCache = load_cache,
    .parseEvent = parse_event,
};

static int check_host(const struct hidpp_feat_platform *pl, uint8_t host) {
    if (host == HIDPP_HOST_CURRENT)
        return HIDPP_OK;

    /* the dual platform feature only knows the current host */
    if (!pl->info.isMultiPlatform)
        return HIDPP_EINVAL;

    if (pl->info.hostCount && host >= pl->info.hostCount)
        return HIDPP_EINVAL;

    return HIDPP_OK;
}

struct hidpp_platform_info *hidpp_platform_info(hidpp_device_t *dev) {
    struct hidpp_feat_platform *pl;

    if (!dev || ensure_init(&pl, dev))
        return NULL;

    return &pl->info;
}

int hidpp_platform_descriptor(
    hidpp_device_t *dev, uint8_t index, struct hidpp_platform_descriptor *out
) {
    struct hidpp_feat_platform *pl;
    int rc;

    if (!out)
        return HIDPP_EINVAL;

    if ((rc = ensure_init(&pl, dev)))
        return rc;

    if (!pl->info.isMultiPlatform)
        return HIDPP_ENOSYS;

    if (index >= pl->info.descriptorCount)
        return HIDPP_EINVAL;

    *out = pl->descriptors[index];
    return HIDPP_OK;
}

int hidpp_platform_get(
    hidpp_device_t *dev, uint8_t host, struct hidpp_host_platform *out
) {
    struct hidpp_feat_platform *pl;
    int rc;

    if (!out)
        return HIDPP_EINVAL;

    if ((rc = ensure_init(&pl, dev)))
        return rc;

    if ((rc = check_host(pl, host)))
        return rc;

    hidpp_packet_t req, res;
    memset(out, 0, sizeof(*out));

    if (!pl->info.isMultiPlatform) {
        make_packet(&req, dev, pl->base.featIndex, 0);

        if ((rc = hidpp_device_request(dev, &req, &res)))
            return rc;

        out->host = HIDPP_HOST_CURRENT;
        out->platform = res.params[0];
        return HIDPP_OK;
    }

    make_packet(&req, dev, pl->base.featIndex, 2);
    req.params[0] = host;

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    if (host != HIDPP_HOST_CURRENT && res.params[0] != host)
        return HIDPP_EIO;

    out->host = res.params[0];
    out->status = res.params[1];
    out->platform = res.params[2];
    out->source = res.params[3];
    out->autoPlatform = res.params[4];
    out->autoDescriptor = res.params[5];
    return HIDPP_OK;
}

int hidpp_platform_set(hidpp_device_t *dev, uint8_t host, uint8_t platform) {
    struct hidpp_feat_platform *pl;
    int rc;

    if ((rc = ensure_init(&pl, dev)))
        return rc;

    if (!pl->info.canSet)
        return HIDPP_ENOSYS;

    if ((rc = check_host(pl, host)))
        return rc;

    if (pl->info.platformCount && platform >= pl->info.platformCount)
        return HIDPP_EINVAL;

    hidpp_packet_t req, res;

    if (!pl->info.isMultiPlatform) {
        make_packet(&req, dev, pl->base.featIndex, 1);
        req.params[0] = platform;
    } else {
        make_packet(&req, dev, pl->base.featIndex, 3);
        req.params[0] = host;
        req.params[1] = platform;
    }

    return hidpp_device_request(dev, &req, &res);
}
