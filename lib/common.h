/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#ifndef HIDPP_COMMON_H
#define HIDPP_COMMON_H

#include <hidpp.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HIDPP_MAX_ERROR 256

#define SAVE_BYTE(b, val) *b++ = val
#define SAVE_WORD(b, val) (save_word(b, val), b += 2)
#define SAVE_DWORD(b, val) (save_dword(b, val), b += 4)
#define SAVE_QWORD(b, val) (save_qword(b, val), b += 8)
#define SAVE_BYTES(b, arr, count) (save_bytes(b, arr, count), b += count)

#define LOAD_BYTE(b) (*b++)
#define LOAD_WORD(b) (b += 2, load_word(&b[-2]))
#define LOAD_DWORD(b) (b += 4, load_dword(&b[-4]))
#define LOAD_QWORD(b) (b += 8, load_qword(&b[-8]))
#define LOAD_BYTES(b, out, count) (load_bytes(b, out, count), b += count)

/* Forward declarations */
typedef struct hid_device_ hid_device;
typedef struct allocator_t allocator_t;

enum {
    RCV_HIDAPI,
    RCV_SOCKET,
    RCV_CUSTOM,
};

enum {
    HIDPP_FEAT_ROOT,
    HIDPP_FEAT_KEYMAP,
    HIDPP_FEAT_FIRMWARE_INFO,
    HIDPP_FEAT_DEVICE_NAME,
    HIDPP_FEAT_BATTERY,
    hidpp_feat_max,
    hidpp_feat_sentinel = 2,
};

struct hidpp_feat_vt {
    size_t size;
    size_t alignment;
    size_t minCacheSize;
    size_t maxCacheSize;
    int (*init)(hidpp_device_t *dev, void *feat);
    void (*collectCache)(hidpp_device_t *dev, void *feat);
    size_t (*saveCache)(hidpp_device_t *dev, void *feat, uint8_t *buffer);
    size_t (*loadCache)(hidpp_device_t *dev, void *feat, uint8_t *buffer);
    void (*clearCache)(hidpp_device_t *dev, void *feat);
    void (*free)(hidpp_device_t *dev, void *feat);
};

struct hidpp_device_t {
    hidpp_receiver_t *receiver;
    uint16_t version;
    uint8_t index;
    uint8_t swid;

    void *allocBuffer;
    size_t allocSize;

    void *features[hidpp_feat_max];
};

struct hidpp_receiver_t {
    int type;
    int socket;
    hid_device *handle;
    hidpp_receiver_send_fn *customSend;
    hidpp_receiver_recv_fn *customRecv;
    void *customUser;

    int timeout;
    uint8_t swid;
    uint8_t retries;
    hidpp_bool_t nonblocking;

    hidpp_packet_t *currentRequest;
    hidpp_protocol_hook_fn *hook;
    void *hookUser;

    hidpp_device_t *devices[7];
    wchar_t error[HIDPP_MAX_ERROR];
};

extern allocator_t *hidpp_allocator;
extern const struct hidpp_feat_vt *hidpp_feat_vtables[hidpp_feat_max];

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

static inline void make_packet(
    hidpp_packet_t *out, hidpp_device_t *dev, uint8_t feat, uint8_t func
) {
    hidpp_make(out, dev->index, feat, HIDPP_BYTE(func, dev->swid), NULL, 0);
    memset(out->params, 0, sizeof(out->params));
}

static inline void save_word(uint8_t *b, uint16_t val) {
    b[1] = (val >> 0) & 0xFF;
    b[0] = (val >> 8) & 0xFF;
}

static inline void save_dword(uint8_t *b, uint32_t val) {
    b[3] = (val >> 0) & 0xFF;
    b[2] = (val >> 8) & 0xFF;
    b[1] = (val >> 16) & 0xFF;
    b[0] = (val >> 24) & 0xFF;
}

static inline void save_qword(uint8_t *b, uint64_t val) {
    b[7] = (val >> 0) & 0xFF;
    b[6] = (val >> 8) & 0xFF;
    b[5] = (val >> 16) & 0xFF;
    b[4] = (val >> 24) & 0xFF;
    b[3] = (val >> 32) & 0xFF;
    b[2] = (val >> 40) & 0xFF;
    b[1] = (val >> 48) & 0xFF;
    b[0] = (val >> 56) & 0xFF;
}

static inline void save_bytes(uint8_t *b, const char *arr, size_t count) {
    for (size_t i = 0; i < count; i++)
        *b++ = arr[i];
}

static inline uint16_t load_word(const uint8_t *b) {
    return HIDPP_WORD(b[0], b[1]);
}

static inline uint32_t load_dword(const uint8_t *b) {
    return HIDPP_DWORD(b[0], b[1], b[2], b[3]);
}

static inline uint64_t load_qword(const uint8_t *b) {
    uint64_t hi = HIDPP_DWORD(b[0], b[1], b[2], b[3]);
    uint64_t lo = HIDPP_DWORD(b[4], b[5], b[6], b[7]);
    return (hi << 32) | lo;
}

static inline void load_bytes(const uint8_t *b, char *out, size_t count) {
    for (size_t i = 0; i < count; i++)
        out[i] = *b++;
}

#ifdef __cplusplus
}
#endif

#endif /* HIDPP_COMMON_H */