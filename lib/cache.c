/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "common.h"
#include "hidpp.h"

#include <assert.h>
#include <pf_macro.h>

/*  Header layout (big endian):
        0  magic "hidppctl"
        8  library version
       12  maximum size of a cache with the current feature set
       16  cache id of the device
       24  number of bytes actually used by this cache, header included
       28  reserved (zero)
*/
#define CACHE_HEADER_SIZE 32
#define CACHE_USED_OFFSET 24
#define CACHE_MAGIC "hidppctl"
#define STACK_BUFFER_SIZE 65536

static inline uint64_t build_uint64_t(uint8_t *a, size_t count) {
    uint64_t out = 0;
    for (size_t i = 0; i < count; i++)
        out = (out << 8) | (uint64_t)a[i];
    return out;
}

static size_t required_cache_size(void) {
    size_t req = CACHE_HEADER_SIZE;
    for (size_t i = 0; i < hidpp_feat_max; i++)
        req += hidpp_feat_vtables[i]->maxCacheSize;
    return req;
}

static void write_header(hidpp_device_t *dev, uint8_t *b) {
    memset(b, 0, CACHE_HEADER_SIZE);
    SAVE_BYTES(b, CACHE_MAGIC, sizeof(CACHE_MAGIC) - 1);
    SAVE_DWORD(b, HIDPP_VERSION);
    SAVE_DWORD(b, required_cache_size());
    SAVE_QWORD(b, hidpp_cache_id(dev));
}

static int check_header(hidpp_device_t *dev, const uint8_t *b, size_t size) {
    char buf[sizeof(CACHE_MAGIC) - 1];

    LOAD_BYTES(b, buf, sizeof(buf));
    uint32_t version = LOAD_DWORD(b);
    uint32_t req = LOAD_DWORD(b);
    uint64_t cacheId = LOAD_QWORD(b);
    uint32_t used = LOAD_DWORD(b);

    if (memcmp(buf, CACHE_MAGIC, sizeof(buf)))
        return HIDPP_EINVAL;

    if (version != HIDPP_VERSION || req != required_cache_size())
        return HIDPP_ENOSYS;

    /*  Loaders read missing bytes as zeros, which would silently turn a
        truncated file into a cache full of half-initialised entries. */
    if (used < CACHE_HEADER_SIZE || used > req || used > size)
        return HIDPP_EINVAL;

    uint64_t current = hidpp_cache_id(dev);

    if (current == 0 || cacheId != current)
        return HIDPP_EINVAL;

    return HIDPP_OK;
}

uint64_t hidpp_cache_id(hidpp_device_t *dev) {
    if (!dev)
        return 0;

    hidpp_packet_t req, res;

    /* Cached feature index is avoided */
    make_packet(&req, dev, 0, 0);
    req.params[0] = HIDPP_MSB(0x0003);
    req.params[1] = HIDPP_LSB(0x0003);

    if (hidpp_device_request(dev, &req, &res))
        return 0;

    uint8_t featIndex = res.params[0];

    /* Index 0 is the root feature, meaning 0x0003 is not implemented. */
    if (featIndex == 0)
        return 0;

    make_packet(&req, dev, featIndex, 1);
    req.params[0] = 0;

    if (hidpp_device_request(dev, &req, &res))
        return 0;

    /* 0x00 + Prefix + Version + Build number */
    uint64_t cacheId = build_uint64_t(&res.params[1], 7);
    return cacheId;
}

void hidpp_cache_collect(hidpp_device_t *dev) {
    if (!dev)
        return;

    for (int i = 0; i < hidpp_feat_max; i++)
        hidpp__feature_collect(dev, i);
}

int hidpp_cache_save(hidpp_device_t *dev, void *buffer, size_t *size) {
    if (!dev || !buffer || !size || *size == 0)
        return HIDPP_EINVAL;

    size_t req = required_cache_size();
    size_t max = *size;

    *size = req;

    if (max < req)
        return HIDPP_ENOMEM;

    size_t offset = CACHE_HEADER_SIZE;

    write_header(dev, buffer);

    for (int i = 0; i < hidpp_feat_max; i++)
        offset += hidpp__feature_save(dev, i, PF_OFFSET(buffer, offset));

    save_dword((uint8_t *)buffer + CACHE_USED_OFFSET, (uint32_t)offset);
    *size = offset;
    return HIDPP_OK;
}

int hidpp_cache_load(hidpp_device_t *dev, const void *buffer, size_t size) {
    if (!dev || !buffer || size == 0)
        return HIDPP_EINVAL;

    size_t req = required_cache_size();

    if (req > STACK_BUFFER_SIZE)
        return HIDPP_ENOSYS;

    /*  This prevents loaders from reading out of bounds since they won't
        oversteep their maximum reserved cache size. This is a hacky solution,
        but enables very simple deserialization.

        This means that corrupted cache files could load garbage data. Guarding
        against that would require a solid serialization solution which seems
        unnecessary as it's just simple cache files.
    */
    char stackBuffer[STACK_BUFFER_SIZE];
    memset(stackBuffer, 0, req);
    memcpy(stackBuffer, buffer, req > size ? size : req);
    buffer = stackBuffer;

    int rc;

    if ((rc = check_header(dev, buffer, size)))
        return rc;

    size_t offset = CACHE_HEADER_SIZE;

    for (int i = 0; i < hidpp_feat_max; i++)
        offset += hidpp__feature_load(dev, i, PF_OFFSET(buffer, offset));

    return HIDPP_OK;
}

void hidpp_cache_clear(hidpp_device_t *dev) {
    if (!dev)
        return;

    for (int i = 0; i < hidpp_feat_max; i++)
        hidpp__feature_clear(dev, i);
}
