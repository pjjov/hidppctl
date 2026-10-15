/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

#include <string.h>
#include <time.h>

/** Section: HID++ feature documentation

    Friendly name (id: 0x0007)
    Big endian integers are exchanged.

    -----------------------
    Fn 0 - get name length
    -----------------------
    request:
    response:
        uint8_t nameLength
        uint8_t maxLength
        uint8_t defaultLength

    -----------------------
    Fn 1 - get name slice
    -----------------------
    request:
        uint8_t index
    response:
        uint8_t index (echo)
        uint8_t[15] chars

    -----------------------
    Fn 2 - get default name slice
    -----------------------
    request:
        uint8_t index
    response:
        uint8_t index (echo)
        uint8_t[15] chars

    -----------------------
    Fn 3 - set name slice
    -----------------------
    request:
        uint8_t index
        uint8_t[15] chars
    response:
        uint8_t nameLength

    -----------------------
    Fn 4 - reset name to default
    -----------------------
    request:
    response:
        uint8_t nameLength
*/

#define MAX_NAME_LEN 256
#define DEFAULT_CYCLE_DURATION (5 * 60)

struct hidpp_feat_nickname {
    struct hidpp_feat_base base;
    uint8_t maxNameLength;

    time_t cycleDuration;
    time_t lastUpdated;
    uint8_t nameLength;
    char name[MAX_NAME_LEN + 1];

    uint8_t defaultNameLength;
    char defaultName[MAX_NAME_LEN + 1];
};

static int query_name_length(
    hidpp_device_t *dev, struct hidpp_feat_nickname *nick
) {
    hidpp_packet_t req, res;
    make_packet(&req, dev, nick->base.featIndex, 0);

    int rc;

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    nick->nameLength = res.params[0];
    nick->maxNameLength = res.params[1];
    nick->defaultNameLength = res.params[2];

    return HIDPP_OK;
}

static int query_name(
    hidpp_device_t *dev,
    uint8_t featIndex,
    uint8_t func,
    size_t length,
    char *out
) {
    hidpp_packet_t req, res;
    make_packet(&req, dev, featIndex, func);

    size_t size = 15;
    size_t read = 0;
    int rc;

    while (read < length) {
        req.params[0] = (uint8_t)read;

        if ((rc = hidpp_device_request(dev, &req, &res)))
            return rc;

        if (req.params[0] != res.params[0])
            return HIDPP_EINVAL;

        memcpy(
            &out[read],
            &res.params[1],
            length - read < size ? length - read : size
        );
        read += size;
    }

    out[length] = '\0';
    return HIDPP_OK;
}

static int set_name(
    hidpp_device_t *dev, uint8_t featIndex, size_t length, const char *buf
) {
    hidpp_packet_t req, res;
    make_packet(&req, dev, featIndex, 3);
    req.kind = HIDPP_KIND_LONG; /* index + 15 characters need a long report */

    size_t size = 15;
    size_t written = 0;
    int rc;

    while (written < length) {
        memset(req.params, 0, sizeof(req.params));
        req.params[0] = (uint8_t)written;

        memcpy(
            &req.params[1],
            &buf[written],
            length - written < size ? length - written : size
        );

        if ((rc = hidpp_device_request(dev, &req, &res)))
            return rc;

        written += size;
    }

    return HIDPP_OK;
}

static int reset_name(hidpp_device_t *dev, uint8_t featIndex) {
    hidpp_packet_t req, res;
    make_packet(&req, dev, featIndex, 4);

    return hidpp_device_request(dev, &req, &res);
}

/* Name could be changed by another app, so we don't cache it permanently. */
static void query_uncached_name(
    hidpp_device_t *dev, struct hidpp_feat_nickname *nick
) {
    if (time(NULL) - nick->lastUpdated < nick->cycleDuration)
        return;

    if (query_name_length(dev, nick)
        || query_name(
            dev, nick->base.featIndex, 1, nick->nameLength, nick->name
        )) {
        nick->name[0] = '\0';
        nick->nameLength = 0;
    }

    nick->lastUpdated = time(NULL);
}

static int query_info(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_nickname *nick = feat;
    int rc;

    /* a device that refuses even the lengths has no usable nickname */
    if ((rc = query_name_length(dev, nick))) {
        if (is_transient(dev, rc))
            return rc;

        nick->base.unsupported = HIDPP_TRUE;
        return HIDPP_OK;
    }

    rc = query_name(
        dev, nick->base.featIndex, 2, nick->defaultNameLength, nick->defaultName
    );

    if (rc) {
        if (is_transient(dev, rc))
            return rc;

        nick->defaultName[0] = '\0';
        nick->defaultNameLength = 0;
    }

    nick->cycleDuration = DEFAULT_CYCLE_DURATION;
    nick->lastUpdated = 0;
    nick->name[0] = '\0';
    return HIDPP_OK;
}

static int ensure_init(struct hidpp_feat_nickname **out, hidpp_device_t *dev) {
    void *feat;
    int rc = hidpp__feature_init(dev, HIDPP_FEAT_NICKNAME, &feat);
    *out = feat;
    return rc;
}

static size_t save_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_nickname *nick = feat;
    uint8_t *start = b;
    (void)dev;

    SAVE_BYTE(b, nick->maxNameLength);
    SAVE_BYTE(b, nick->defaultNameLength);
    SAVE_BYTES(b, nick->defaultName, MAX_NAME_LEN);

    return b - start;
}

static size_t load_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_nickname *nick = feat;
    uint8_t *start = b;
    (void)dev;

    nick->maxNameLength = LOAD_BYTE(b);
    nick->defaultNameLength = LOAD_BYTE(b);
    LOAD_BYTES(b, nick->defaultName, MAX_NAME_LEN);

    nick->defaultName[nick->defaultNameLength] = '\0';
    nick->name[0] = '\0';
    nick->nameLength = 0;

    nick->cycleDuration = DEFAULT_CYCLE_DURATION;
    nick->lastUpdated = 0;

    return b - start;
}

const struct hidpp_feat_vt hidpp_feat_nickname_vt = {
    FEAT_VT_SIZE(struct hidpp_feat_nickname),
    .ids = { 0x0007 },
    .query = query_info,
    .saveCache = save_cache,
    .loadCache = load_cache,
};

const char *hidpp_nickname_get(hidpp_device_t *dev) {
    struct hidpp_feat_nickname *nick;

    if (!dev || ensure_init(&nick, dev))
        return NULL;

    query_uncached_name(dev, nick);
    return nick->name;
}

int hidpp_nickname_set(hidpp_device_t *dev, const char *nickname) {
    if (!dev || !nickname)
        return HIDPP_EINVAL;

    struct hidpp_feat_nickname *nick;
    int rc;

    if ((rc = ensure_init(&nick, dev)))
        return rc;

    size_t len = strnlen(nickname, nick->maxNameLength);

    if (len >= nick->maxNameLength)
        return HIDPP_ENOMEM;

    if ((rc = set_name(dev, nick->base.featIndex, len + 1, nickname)))
        return rc;

    memcpy(nick->name, nickname, len + 1);
    nick->nameLength = len;
    nick->lastUpdated = time(NULL);
    return HIDPP_OK;
}

int hidpp_nickname_reset(hidpp_device_t *dev) {
    if (!dev)
        return HIDPP_EINVAL;

    struct hidpp_feat_nickname *nick;
    int rc;

    if ((rc = ensure_init(&nick, dev)))
        return rc;

    if ((rc = reset_name(dev, nick->base.featIndex)))
        return rc;

    nick->lastUpdated = 0; /* force the next getter to re-read the name */
    return HIDPP_OK;
}

const char *hidpp_nickname_default(hidpp_device_t *dev) {
    struct hidpp_feat_nickname *nick;

    if (!dev || ensure_init(&nick, dev))
        return NULL;

    return nick->defaultName;
}