/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

#include <string.h>

/** Section: HID++ feature documentation

    Change host (id: 0x1814)
    Big endian integers are exchanged.

    -----------------------
    Fn 0 - get host info
    -----------------------
    request:
    response:
        uint8_t hostCount
        uint8_t currentHost
        uint8_t flags
        - bit 0 - enhanced host switch

    -----------------------
    Fn 1 - set current host
    -----------------------
    request:
        uint8_t host
    response:
        (none, the device usually resets or disconnects)

    -----------------------
    Fn 2 - get cookies
    -----------------------
    request:
    response:
        uint8_t cookies[hostCount]

    -----------------------
    Fn 3 - set cookie
    -----------------------
    request:
        uint8_t host
        uint8_t cookie
    response:

    Hosts information (id: 0x1815, version 2)
    Big endian integers are exchanged, except for the capability mask whose
    first byte holds the lowest bits.

    -----------------------
    Fn 0 - get feature info
    -----------------------
    request:
    response:
        uint16_t capabilityMask
        - bit 0 - get name
        - bit 1 - set name
        - bit 2 - move host
        - bit 3 - delete host
        - bit 4 - OS version
        - bit 5 - eQuad host descriptor
        - bit 6 - USB host descriptor
        - bit 7 - BT host descriptor
        - bit 8 - BLE host descriptor
        uint8_t hostCount
        uint8_t currentHost

    -----------------------
    Fn 1 - get host info
    -----------------------
    request:
        uint8_t host (0xFF is the current host)
    response:
        uint8_t host
        uint8_t status (0 = empty, 1 = paired)
        uint8_t busType
        uint8_t pageCount
        uint8_t nameLength
        uint8_t nameMaxLength

    -----------------------
    Fn 2 - get host descriptor
    -----------------------
    request:
        uint8_t host
        uint8_t page
    response:
        uint8_t host
        uint8_t busType
        uint8_t data[14]

    -----------------------
    Fn 3 - get host name chunk
    -----------------------
    request:
        uint8_t host
        uint8_t byteIndex
    response:
        uint8_t host (echo)
        uint8_t byteIndex (echo)
        uint8_t chars[14]

    -----------------------
    Fn 4 - set host name chunk
    -----------------------
    request:
        uint8_t host
        uint8_t byteIndex
        uint8_t chars[14]
    response:
        uint8_t host
        uint8_t nameLength

    -----------------------
    Fn 5 - move host
    -----------------------
    request:
        uint8_t host
        uint8_t newIndex
    response:

    -----------------------
    Fn 6 - delete host
    -----------------------
    request:
        uint8_t host
    response:

    -----------------------
    Fn 7 - get host OS
    -----------------------
    request:
        uint8_t host
    response:
        uint8_t host
        uint8_t type
        uint8_t version
        uint16_t revision
        uint16_t build

    -----------------------
    Fn 8 - set host OS
    -----------------------
    request:
        uint8_t host
        uint8_t type
        uint8_t version
        uint16_t revision
        uint16_t build
    response:
*/

#define CHANGE_HOST_ID 0x1814
#define HOSTS_INFO_ID 0x1815

#define MAX_COOKIES 16
#define NAME_CHUNK_SIZE 14
#define MAX_NAME_LEN 255

/* clang-format off */

#define CAP_GET_NAME    (1u << 0)
#define CAP_SET_NAME    (1u << 1)
#define CAP_MOVE        (1u << 2)
#define CAP_DELETE      (1u << 3)
#define CAP_OS_VERSION  (1u << 4)
#define CAP_EQUAD_DESC  (1u << 5)
#define CAP_USB_DESC    (1u << 6)
#define CAP_BT_DESC     (1u << 7)
#define CAP_BLE_DESC    (1u << 8)

#define PAIRED_STATUS 1

/* clang-format on */

struct hidpp_feat_hosts {
    hidpp_bool_t initialized;
    hidpp_bool_t unsupported;

    uint8_t changeIndex; /* 0 if 0x1814 is not implemented */
    uint8_t infoIndex;   /* 0 if 0x1815 is not implemented */
    struct hidpp_host_info info;
};

static const char *bus_name(uint8_t type) {
    switch (type) {
        /* clang-format off */
    case HIDPP_HOST_BUS_NONE:    return "None";
    case HIDPP_HOST_BUS_EQUAD:   return "eQuad";
    case HIDPP_HOST_BUS_USB:     return "USB";
    case HIDPP_HOST_BUS_BT:      return "Bluetooth";
    case HIDPP_HOST_BUS_BLE:     return "Bluetooth Low Energy";
    case HIDPP_HOST_BUS_BLE_PRO: return "Bluetooth Low Energy Pro";
    default:                     return "Unknown";
        /* clang-format on */
    }
}

static void unpack_flags(struct hidpp_feat_hosts *hosts) {
    struct hidpp_host_info *info = &hosts->info;
    uint16_t caps = info->capabilities;

    info->canSwitch = hosts->changeIndex != 0;
    info->hasHostInfo = hosts->infoIndex != 0;
    info->canGetName = !!(caps & CAP_GET_NAME);
    info->canSetName = !!(caps & CAP_SET_NAME);
    info->canMove = !!(caps & CAP_MOVE);
    info->canDelete = !!(caps & CAP_DELETE);
    info->hasOsVersion = !!(caps & CAP_OS_VERSION);
    info->hasEQuadDescriptor = !!(caps & CAP_EQUAD_DESC);
    info->hasUSBDescriptor = !!(caps & CAP_USB_DESC);
    info->hasBTDescriptor = !!(caps & CAP_BT_DESC);
    info->hasBLEDescriptor = !!(caps & CAP_BLE_DESC);
}

static int query_info(hidpp_device_t *dev, struct hidpp_feat_hosts *hosts) {
    struct hidpp_host_info *info = &hosts->info;
    hidpp_packet_t req, res;
    hidpp_bool_t answered = HIDPP_FALSE;
    int rc;

    if (hosts->infoIndex) {
        make_packet(&req, dev, hosts->infoIndex, 0);

        if (!(rc = hidpp_device_request(dev, &req, &res))) {
            info->capabilities = HIDPP_WORD(res.params[1], res.params[0]);
            info->count = res.params[2];
            answered = HIDPP_TRUE;
        } else if (is_transient(dev, rc)) {
            return rc;
        }
    }

    /* the older feature is only needed for what the newer one lacks */
    if (hosts->changeIndex && (!answered || info->count == 0)) {
        make_packet(&req, dev, hosts->changeIndex, 0);

        if (!(rc = hidpp_device_request(dev, &req, &res))) {
            info->count = res.params[0];
            answered = HIDPP_TRUE;
        } else if (is_transient(dev, rc)) {
            return rc;
        }
    }

    unpack_flags(hosts);
    info->initialized = answered;
    return HIDPP_OK;
}

static int ensure_init(struct hidpp_feat_hosts **out, hidpp_device_t *dev) {
    struct hidpp_feat_hosts *hosts;

    if (!out || !dev)
        return HIDPP_EINVAL;

    hosts = dev->features[HIDPP_FEAT_HOSTS];
    *out = hosts;

    if (hosts->initialized)
        return hosts->unsupported ? HIDPP_ENOSYS : HIDPP_OK;

    memset(hosts, 0, sizeof(*hosts));

    int change = feature_lookup(dev, CHANGE_HOST_ID);
    int info = feature_lookup(dev, HOSTS_INFO_ID);
    int rc;

    if (change < 0 || info < 0)
        return change < 0 ? change : info; /* ask again next time */

    hosts->changeIndex = (uint8_t)change;
    hosts->infoIndex = (uint8_t)info;
    hosts->unsupported = !hosts->changeIndex && !hosts->infoIndex;

    if (!hosts->unsupported && (rc = query_info(dev, hosts)))
        return rc;

    hosts->initialized = HIDPP_TRUE;

    return hosts->unsupported ? HIDPP_ENOSYS : HIDPP_OK;
}

static void clear_cache(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_hosts *hosts = feat;
    (void)dev;
    hosts->initialized = HIDPP_FALSE;
}

static void collect_cache(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_hosts *hosts;
    (void)feat;
    ensure_init(&hosts, dev);
}

static size_t save_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_hosts *hosts = feat;
    uint8_t *start = b;
    (void)dev;

    SAVE_BYTE(b, hosts->initialized);
    SAVE_BYTE(b, hosts->unsupported);

    if (!hosts->initialized || hosts->unsupported)
        return b - start;

    SAVE_BYTE(b, hosts->changeIndex);
    SAVE_BYTE(b, hosts->infoIndex);
    SAVE_BYTE(b, hosts->info.initialized);
    SAVE_BYTE(b, hosts->info.count);
    SAVE_WORD(b, hosts->info.capabilities);

    return b - start;
}

static size_t load_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_hosts *hosts = feat;
    uint8_t *start = b;
    (void)dev;

    hosts->initialized = LOAD_BYTE(b);
    hosts->unsupported = LOAD_BYTE(b);

    if (!hosts->initialized || hosts->unsupported)
        return b - start;

    hosts->changeIndex = LOAD_BYTE(b);
    hosts->infoIndex = LOAD_BYTE(b);
    hosts->info.initialized = LOAD_BYTE(b);
    hosts->info.count = LOAD_BYTE(b);
    hosts->info.capabilities = LOAD_WORD(b);
    unpack_flags(hosts);

    return b - start;
}

const struct hidpp_feat_vt hidpp_feat_hosts_vt = {
    .size = sizeof(struct hidpp_feat_hosts),
    .alignment = _Alignof(struct hidpp_feat_hosts),
    .maxCacheSize = sizeof(struct hidpp_feat_hosts),
    .minCacheSize = 2 * sizeof(uint8_t),
    .collectCache = collect_cache,
    .saveCache = save_cache,
    .loadCache = load_cache,
    .clearCache = clear_cache,
};

/*  Feature 0x1815 is needed and the device must announce `cap`.
    A zero `cap` only requires the feature. */
static int require_info(struct hidpp_feat_hosts *hosts, uint16_t cap) {
    if (!hosts->infoIndex)
        return HIDPP_ENOSYS;

    if (cap && !(hosts->info.capabilities & cap))
        return HIDPP_ENOSYS;

    return HIDPP_OK;
}

static int check_host(
    const struct hidpp_feat_hosts *hosts, uint8_t host, hidpp_bool_t current
) {
    if (host == HIDPP_HOST_CURRENT)
        return current ? HIDPP_OK : HIDPP_EINVAL;

    return host < hosts->info.count ? HIDPP_OK : HIDPP_EINVAL;
}

static int query_host_status(
    hidpp_device_t *dev,
    struct hidpp_feat_hosts *hosts,
    uint8_t host,
    struct hidpp_host_status *out
) {
    hidpp_packet_t req, res;
    make_packet(&req, dev, hosts->infoIndex, 1);
    req.params[0] = host;

    int rc;

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    if (host != HIDPP_HOST_CURRENT && res.params[0] != host)
        return HIDPP_EIO;

    out->index = res.params[0];
    out->status = res.params[1];
    out->busType = res.params[2];
    out->numPages = res.params[3];
    out->nameLength = res.params[4];
    out->nameMaxLength = res.params[5];
    out->isPaired = out->status == PAIRED_STATUS;
    out->busName = bus_name(out->busType);

    return HIDPP_OK;
}

struct hidpp_host_info *hidpp_host_info(hidpp_device_t *dev) {
    struct hidpp_feat_hosts *hosts;

    if (!dev || ensure_init(&hosts, dev))
        return NULL;

    return &hosts->info;
}

int hidpp_host_current(hidpp_device_t *dev) {
    struct hidpp_feat_hosts *hosts;
    hidpp_packet_t req, res;
    int rc;

    if ((rc = ensure_init(&hosts, dev)))
        return rc;

    if (hosts->changeIndex) {
        make_packet(&req, dev, hosts->changeIndex, 0);

        if (!(rc = hidpp_device_request(dev, &req, &res)))
            return res.params[1];

        if (!hosts->infoIndex)
            return rc;
    }

    make_packet(&req, dev, hosts->infoIndex, 0);

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    return res.params[3];
}

int hidpp_host_switch(hidpp_device_t *dev, uint8_t host) {
    struct hidpp_feat_hosts *hosts;
    int rc;

    if ((rc = ensure_init(&hosts, dev)))
        return rc;

    if (!hosts->changeIndex)
        return HIDPP_ENOSYS;

    if ((rc = check_host(hosts, host, HIDPP_FALSE)))
        return rc;

    hidpp_packet_t req;
    make_packet(&req, dev, hosts->changeIndex, 1);
    req.params[0] = host;

    /*  The device resets or drops the connection instead of answering,
        so waiting for a response would only end in a timeout. */
    return hidpp_device_send(dev, &req);
}

int hidpp_host_status(
    hidpp_device_t *dev, uint8_t host, struct hidpp_host_status *out
) {
    struct hidpp_feat_hosts *hosts;
    int rc;

    if (!out)
        return HIDPP_EINVAL;

    if ((rc = ensure_init(&hosts, dev)) || (rc = require_info(hosts, 0))
        || (rc = check_host(hosts, host, HIDPP_TRUE)))
        return rc;

    return query_host_status(dev, hosts, host, out);
}

int hidpp_host_name(hidpp_device_t *dev, uint8_t host, char *out, size_t size) {
    struct hidpp_feat_hosts *hosts;
    struct hidpp_host_status status;
    int rc;

    if (!out || size == 0)
        return HIDPP_EINVAL;

    out[0] = '\0';

    if ((rc = ensure_init(&hosts, dev))
        || (rc = require_info(hosts, CAP_GET_NAME))
        || (rc = check_host(hosts, host, HIDPP_TRUE))
        || (rc = query_host_status(dev, hosts, host, &status)))
        return rc;

    size_t length = status.nameLength;

    if (length > size - 1)
        length = size - 1;

    hidpp_packet_t req, res;
    make_packet(&req, dev, hosts->infoIndex, 3);
    req.params[0] = host;

    size_t read = 0;

    while (read < length) {
        req.params[1] = (uint8_t)read;

        if ((rc = hidpp_device_request(dev, &req, &res)))
            return rc;

        if ((host != HIDPP_HOST_CURRENT && res.params[0] != host)
            || res.params[1] != (uint8_t)read)
            return HIDPP_EIO;

        size_t chunk = length - read;

        if (chunk > NAME_CHUNK_SIZE)
            chunk = NAME_CHUNK_SIZE;

        memcpy(&out[read], &res.params[2], chunk);
        read += chunk;
    }

    out[length] = '\0';

    /* the name ends at the first null byte, whatever the device claims */
    return (int)strlen(out);
}

int hidpp_host_set_name(hidpp_device_t *dev, uint8_t host, const char *name) {
    struct hidpp_feat_hosts *hosts;
    struct hidpp_host_status status;
    int rc;

    if (!name)
        return HIDPP_EINVAL;

    size_t length = strlen(name);

    if (length > MAX_NAME_LEN)
        return HIDPP_ENOMEM;

    for (size_t i = 0; i < length; i++)
        if ((uint8_t)name[i] >= 0x80)
            return HIDPP_EINVAL; /* friendly names are ASCII */

    if ((rc = ensure_init(&hosts, dev))
        || (rc = require_info(hosts, CAP_SET_NAME))
        || (rc = check_host(hosts, host, HIDPP_TRUE))
        || (rc = query_host_status(dev, hosts, host, &status)))
        return rc;

    if (length > status.nameMaxLength)
        return HIDPP_ENOMEM;

    hidpp_packet_t req, res;
    size_t written = 0;

    /* an empty name still needs one (empty) chunk to truncate the old one */
    do {
        size_t chunk = length - written;

        if (chunk > NAME_CHUNK_SIZE)
            chunk = NAME_CHUNK_SIZE;

        make_packet(&req, dev, hosts->infoIndex, 4);
        req.kind = HIDPP_KIND_LONG;
        req.params[0] = host;
        req.params[1] = (uint8_t)written;
        memcpy(&req.params[2], &name[written], chunk);

        if ((rc = hidpp_device_request(dev, &req, &res)))
            return rc;

        written += chunk;
    } while (written < length);

    return HIDPP_OK;
}

int hidpp_host_move(hidpp_device_t *dev, uint8_t host, uint8_t newIndex) {
    struct hidpp_feat_hosts *hosts;
    int rc;

    if ((rc = ensure_init(&hosts, dev)) || (rc = require_info(hosts, CAP_MOVE))
        || (rc = check_host(hosts, host, HIDPP_FALSE))
        || (rc = check_host(hosts, newIndex, HIDPP_FALSE)))
        return rc;

    hidpp_packet_t req, res;
    make_packet(&req, dev, hosts->infoIndex, 5);
    req.params[0] = host;
    req.params[1] = newIndex;

    return hidpp_device_request(dev, &req, &res);
}

int hidpp_host_delete(hidpp_device_t *dev, uint8_t host) {
    struct hidpp_feat_hosts *hosts;
    int rc;

    if ((rc = ensure_init(&hosts, dev))
        || (rc = require_info(hosts, CAP_DELETE))
        || (rc = check_host(hosts, host, HIDPP_FALSE)))
        return rc;

    hidpp_packet_t req, res;
    make_packet(&req, dev, hosts->infoIndex, 6);
    req.params[0] = host;

    return hidpp_device_request(dev, &req, &res);
}

int hidpp_host_os(
    hidpp_device_t *dev, uint8_t host, struct hidpp_host_os *out
) {
    struct hidpp_feat_hosts *hosts;
    int rc;

    if (!out)
        return HIDPP_EINVAL;

    if ((rc = ensure_init(&hosts, dev))
        || (rc = require_info(hosts, CAP_OS_VERSION))
        || (rc = check_host(hosts, host, HIDPP_FALSE)))
        return rc;

    hidpp_packet_t req, res;
    make_packet(&req, dev, hosts->infoIndex, 7);
    req.params[0] = host;

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    if (res.params[0] != host)
        return HIDPP_EIO;

    out->type = res.params[1];
    out->version = res.params[2];
    out->revision = HIDPP_WORD(res.params[3], res.params[4]);
    out->build = HIDPP_WORD(res.params[5], res.params[6]);

    return HIDPP_OK;
}

int hidpp_host_set_os(
    hidpp_device_t *dev, uint8_t host, const struct hidpp_host_os *os
) {
    struct hidpp_feat_hosts *hosts;
    int rc;

    if (!os)
        return HIDPP_EINVAL;

    if ((rc = ensure_init(&hosts, dev))
        || (rc = require_info(hosts, CAP_OS_VERSION))
        || (rc = check_host(hosts, host, HIDPP_FALSE)))
        return rc;

    hidpp_packet_t req, res;
    make_packet(&req, dev, hosts->infoIndex, 8);
    req.kind = HIDPP_KIND_LONG; /* 7 parameters */
    req.params[0] = host;
    req.params[1] = os->type;
    req.params[2] = os->version;
    req.params[3] = HIDPP_MSB(os->revision);
    req.params[4] = HIDPP_LSB(os->revision);
    req.params[5] = HIDPP_MSB(os->build);
    req.params[6] = HIDPP_LSB(os->build);

    return hidpp_device_request(dev, &req, &res);
}

int hidpp_host_descriptor(
    hidpp_device_t *dev,
    uint8_t host,
    uint8_t page,
    struct hidpp_host_descriptor *out
) {
    struct hidpp_feat_hosts *hosts;
    int rc;

    if (!out)
        return HIDPP_EINVAL;

    uint16_t anyDescriptor = CAP_EQUAD_DESC | CAP_USB_DESC | CAP_BT_DESC
        | CAP_BLE_DESC;

    if ((rc = ensure_init(&hosts, dev)) || (rc = require_info(hosts, 0))
        || (rc = check_host(hosts, host, HIDPP_TRUE)))
        return rc;

    if (!(hosts->info.capabilities & anyDescriptor))
        return HIDPP_ENOSYS;

    hidpp_packet_t req, res;
    make_packet(&req, dev, hosts->infoIndex, 2);
    req.params[0] = host;
    req.params[1] = page;

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    if (host != HIDPP_HOST_CURRENT && res.params[0] != host)
        return HIDPP_EIO;

    out->index = res.params[0];
    out->busType = res.params[1];
    memcpy(out->data, &res.params[2], sizeof(out->data));

    return HIDPP_OK;
}

/* Cookies are only addressed by real indexes, never by 0xFF. */
static int resolve_cookie_host(
    hidpp_device_t *dev, struct hidpp_feat_hosts *hosts, uint8_t host
) {
    if (!hosts->changeIndex)
        return HIDPP_ENOSYS;

    if (host == HIDPP_HOST_CURRENT) {
        int current = hidpp_host_current(dev);

        if (current < 0)
            return current;

        host = (uint8_t)current;
    }

    if (host >= hosts->info.count || host >= MAX_COOKIES)
        return HIDPP_EINVAL;

    return host;
}

int hidpp_host_cookie(hidpp_device_t *dev, uint8_t host) {
    struct hidpp_feat_hosts *hosts;
    int rc;

    if ((rc = ensure_init(&hosts, dev))
        || (rc = resolve_cookie_host(dev, hosts, host)) < 0)
        return rc;

    hidpp_packet_t req, res;
    make_packet(&req, dev, hosts->changeIndex, 2);

    int index = rc;

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    return res.params[index];
}

int hidpp_host_set_cookie(hidpp_device_t *dev, uint8_t host, uint8_t cookie) {
    struct hidpp_feat_hosts *hosts;
    int rc;

    if ((rc = ensure_init(&hosts, dev))
        || (rc = resolve_cookie_host(dev, hosts, host)) < 0)
        return rc;

    hidpp_packet_t req, res;
    make_packet(&req, dev, hosts->changeIndex, 3);
    req.params[0] = (uint8_t)rc;
    req.params[1] = cookie;

    return hidpp_device_request(dev, &req, &res);
}
