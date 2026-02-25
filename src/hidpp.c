/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include <hidpp.h>

#include <hidapi/hidapi.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define MAX_ERROR 256
#define MAX_RETRY 16
#define DEFAULT_TIMEOUT 2000

#define HIDPP_WORD(msb, lsb) (((uint16_t)(msb) << 8) | (uint16_t)(lsb))
#define HIDPP_MSB(word) ((uint8_t)(((word) >> 8) & 0xFF))
#define HIDPP_LSB(word) ((uint8_t)((word) & 0xFF))

#define HIDPP_BYTE(msn, lsn) (((msn) << 4) | ((lsn) & 0xF))
#define HIDPP_MSN(word) ((uint8_t)(((word) >> 4) & 0xF))
#define HIDPP_LSN(word) ((uint8_t)((word) & 0xF))

struct hidpp_keymap_t {
    hidpp_device_t *device;
    uint8_t initialized;
    uint8_t feature;
    uint8_t numControls;
    uint16_t controls[UINT8_MAX];
};

struct hidpp_device_t {
    hidpp_receiver_t *receiver;
    uint16_t version;
    uint8_t index;
    uint8_t numFeatures;
    uint8_t lenName;

    uint16_t features[UINT8_MAX];
    char name[UINT8_MAX];

    hidpp_keymap_t keymap;
};

struct hidpp_receiver_t {
    hid_device *handle;
    uint8_t swid;
    uint8_t retries;
    int timeout;

    hidpp_device_t devices[7];
    wchar_t error[MAX_ERROR];
};

static void set_error(hidpp_receiver_t *rcv, const wchar_t *fmt, ...) {
    if (!rcv)
        return;

    va_list ap;
    va_start(ap, fmt);
    vswprintf(rcv->error, MAX_ERROR, fmt, ap);
    va_end(ap);
}

static void propagate_error(hidpp_receiver_t *rcv) {
    if (!rcv)
        return;

    const wchar_t *e = hid_error(rcv->handle);

    if (e)
        wcsncpy(rcv->error, e, 255);
    else
        wcsncpy(rcv->error, L"Unknown hidapi error", 255);
    rcv->error[255] = L'\0';
}

static size_t packet_length(int kind) {
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

static void make_packet(
    hidpp_packet_t *out, hidpp_device_t *dev, uint8_t feat, uint8_t func
) {
    hidpp_receiver_t *rcv = dev->receiver;
    hidpp_make(out, dev->index, feat, HIDPP_BYTE(func, rcv->swid), NULL, 0);
    memset(out->params, 0, sizeof(out->params));
}

int hidpp_make(
    hidpp_packet_t *out,
    uint8_t device,
    uint8_t feat,
    uint8_t func,
    uint8_t *data,
    size_t length
) {
    if (length <= HIDPP_LEN_SHORT - 4)
        out->kind = HIDPP_KIND_SHORT;
    else if (length <= HIDPP_LEN_LONG - 4)
        out->kind = HIDPP_KIND_LONG;
    else if (length <= HIDPP_LEN_XLONG - 4)
        out->kind = HIDPP_KIND_XLONG;
    else
        return HIDPP_EINVAL;

    out->device = device;
    out->feat = feat;
    out->func = func;
    if (length > 0 && data)
        memcpy(out->params, data, length);
    return HIDPP_OK;
}

int hidpp_init(void) { return hid_init(); }
void hidpp_exit(void) { hid_exit(); }

static hidpp_receiver_t *create_receiver(hid_device *handle) {
    hidpp_receiver_t *rcv;

    if (!(rcv = calloc(1, sizeof(*rcv)))) {
        hid_close(handle);
        return NULL;
    }

    rcv->handle = handle;
    rcv->retries = MAX_RETRY;
    rcv->timeout = DEFAULT_TIMEOUT;
    return rcv;
}

hidpp_receiver_t *hidpp_open(
    unsigned short vid, unsigned short pid, const wchar_t *serial
) {
    hid_device *handle = hid_open(vid, pid, serial);
    return handle ? create_receiver(handle) : NULL;
}

hidpp_receiver_t *hidpp_open_path(const char *path) {
    hid_device *handle = hid_open_path(path);
    return handle ? create_receiver(handle) : NULL;
}

void hidpp_close(hidpp_receiver_t *rcv) {
    if (!rcv)
        return;

    hid_close(rcv->handle);
    free(rcv);
}

static int convert_receiver_info(
    struct hidpp_receiver_info *out, struct hid_device_info *info
) {
    out->path = info->path;
    out->serial = info->serial_number;
    out->manufacturer = info->manufacturer_string;
    out->product = info->product_string;
    out->vendorId = info->vendor_id;
    out->productId = info->product_id;
    out->releaseNumber = info->release_number;
    out->usagePage = info->usage_page;
    out->usage = info->usage;
    out->interfaceNumber = info->interface_number;
    out->busType = info->bus_type;

    return HIDPP_OK;
}

int hidpp_receiver_info(
    hidpp_receiver_t *rcv, struct hidpp_receiver_info *out
) {
    if (!rcv || !out)
        return HIDPP_EINVAL;

    struct hid_device_info *info;
    if (!(info = hid_get_device_info(rcv->handle)))
        return HIDPP_EIO;

    convert_receiver_info(out, info);
    return HIDPP_OK;
}

int hidpp_send(hidpp_receiver_t *rcv, hidpp_packet_t *pkt) {
    unsigned char buf[HIDPP_LEN_XLONG];
    size_t len = packet_length(pkt->kind);

    if (len <= 0) {
        set_error(rcv, L"Cannot send packet; invalid packet kind");
        return HIDPP_EINVAL;
    }

    buf[0] = pkt->kind;
    buf[1] = pkt->device;
    buf[2] = pkt->feat;
    buf[3] = pkt->func;
    memcpy(&buf[4], pkt->params, len - 4);

    if (hid_write(rcv->handle, buf, len) < 0) {
        set_error(rcv, L"tried to write %lu bytes.", len);
        // propagate_error(rcv);
        return HIDPP_EIO;
    }

    return HIDPP_OK;
}

int hidpp_receive(hidpp_receiver_t *rcv, hidpp_packet_t *out) {
    unsigned char buf[HIDPP_LEN_XLONG];

    int ret = hid_read_timeout(rcv->handle, buf, HIDPP_LEN_XLONG, rcv->timeout);

    if (ret == 0) {
        set_error(rcv, L"Packet reading timed out");
        return HIDPP_ETIMEDOUT;
    } else if (ret < 0) {
        propagate_error(rcv);
        return HIDPP_EIO;
    } else if (ret < 4) {
        set_error(rcv, L"Cannot receive invalid packet");
        return HIDPP_EIO;
    }

    out->kind = buf[0];
    out->device = buf[1];
    out->feat = buf[2];
    out->func = buf[3];
    memcpy(out->params, &buf[4], HIDPP_LEN_XLONG - 4);
    return HIDPP_OK;
}

static int is_error_packet(
    const hidpp_packet_t *req, const hidpp_packet_t *res
) {
    return res->device == req->device && res->feat == 0xFF
        && res->params[0] == req->feat && res->params[1] == req->func;
}

static int is_good_packet(
    const hidpp_packet_t *req, const hidpp_packet_t *res
) {
    return res->device == req->device && res->feat == req->feat
        && (res->func & 0xF0) == (req->func & 0xF0);
}

static const char *hidpp_strerror(uint8_t code) {
    switch (code) {
        /* clang-format off */
    case 0x00: return "No error";
    case 0x01: return "Unknown error";
    case 0x02: return "Invalid argument";
    case 0x03: return "Out of range";
    case 0x04: return "Hardware error";
    case 0x05: return "Logitech internal error";
    case 0x06: return "Invalid feature index";
    case 0x07: return "Invalid function ID";
    case 0x08: return "Device busy";
    case 0x09: return "Unsupported";
    default:   return "Unrecognized error code";
        /* clang-format on */
    }
}

int hidpp_request(
    hidpp_receiver_t *rcv, hidpp_packet_t *request, hidpp_packet_t *response
) {
    int ret = hidpp_send(rcv, request);

    if (ret < 0)
        return ret;

    for (int attempts = 0; attempts < rcv->retries; ++attempts) {
        ret = hidpp_receive(rcv, response);

        if (ret < 0)
            return ret;

        if (is_error_packet(request, response)) {
            const char *error = hidpp_strerror(response->params[2]);
            set_error(rcv, L"HID++ error: %hs", error);
            return HIDPP_EIO;
        }

        if (is_good_packet(request, response))
            return HIDPP_OK;
    }

    set_error(rcv, L"No response after %d reads", MAX_RETRY);
    return -1;
}

const wchar_t *hidpp_error(hidpp_receiver_t *rcv) {
    if (!rcv)
        return hid_error(NULL);
    if (rcv->error[0] != L'\0')
        return rcv->error;
    return hid_error(rcv->handle);
}

static int protocol_version(hidpp_device_t *dev) {
    hidpp_receiver_t *rcv = dev->receiver;

    hidpp_packet_t req, res;
    make_packet(&req, dev, 0, 1);
    req.params[2] = 0xAA;

    if (hidpp_request(rcv, &req, &res))
        return HIDPP_EIO;

    dev->version = HIDPP_WORD(res.params[0], res.params[1]);

    if (dev->version < HIDPP_WORD(2, 0))
        return HIDPP_EINVAL;

    return HIDPP_OK;
}

static uint8_t feature_index(hidpp_device_t *dev, uint16_t feat) {
    hidpp_receiver_t *rcv = dev->receiver;

    hidpp_packet_t req, res;
    make_packet(&req, dev, 0, 0);
    req.params[0] = HIDPP_MSB(feat);
    req.params[1] = HIDPP_LSB(feat);

    if (hidpp_request(rcv, &req, &res))
        return 0;
    return res.params[0];
}

static uint8_t feature_count(hidpp_device_t *dev) {
    hidpp_receiver_t *rcv = dev->receiver;
    uint8_t featIndex = feature_index(dev, 0x0001);

    if (featIndex == 0)
        return HIDPP_EIO;

    hidpp_packet_t req, res;
    make_packet(&req, dev, featIndex, 0);

    if (hidpp_request(rcv, &req, &res))
        return 0;

    return res.params[0];
}

static int find_features(hidpp_device_t *dev) {
    hidpp_receiver_t *rcv = dev->receiver;
    uint8_t featIndex = feature_index(dev, 0x0001);
    dev->numFeatures = feature_count(dev);

    hidpp_packet_t req, res;
    make_packet(&req, dev, featIndex, 1);

    for (int i = 0; i < dev->numFeatures; i++) {
        req.params[0] = (uint8_t)i;
        dev->features[i] = 0;

        if (HIDPP_OK == hidpp_request(rcv, &req, &res))
            dev->features[i] = HIDPP_WORD(res.params[0], res.params[1]);
    }

    return HIDPP_OK;
}

static int find_device_name(hidpp_device_t *dev) {
    hidpp_receiver_t *rcv = dev->receiver;
    uint8_t feat = hidpp_feature_id(dev, 0x0005);

    if (feat == 0)
        return HIDPP_EINVAL;

    hidpp_packet_t req, res;
    make_packet(&req, dev, feat, 0);
    size_t len = hidpp_request(rcv, &req, &res) ? 0 : res.params[0];

    make_packet(&req, dev, feat, 1);
    size_t read = 0;

    while (read < len) {
        req.params[0] = read;

        if (hidpp_request(rcv, &req, &res))
            break;

        size_t size = len - read < 16 ? len - read : 16;
        memcpy(&dev->name[read], res.params, size);
        read += size;
    }

    dev->lenName = read;
    dev->name[read + 1] = '\0';
    return HIDPP_OK;
}

static int find_device_type(hidpp_device_t *dev) {
    hidpp_receiver_t *rcv = dev->receiver;
    uint8_t feat = hidpp_feature_id(dev, 0x0005);

    if (feat == 0)
        return HIDPP_EINVAL;

    hidpp_packet_t req, res;
    make_packet(&req, dev, feat, 2);
    return hidpp_request(rcv, &req, &res) ? -1 : res.params[0];
}

hidpp_device_t *hidpp_device_open(hidpp_receiver_t *rcv, uint8_t device) {
    if (!rcv || device == 0 || (device > 6 && device != 0xFF))
        return NULL;

    hidpp_device_t *dev = &rcv->devices[device != 0xFF ? device : 0];

    if (dev->version != 0)
        return dev;

    dev->receiver = rcv;
    dev->index = device;

    if (protocol_version(dev)) {
        dev->version = 0;
        return NULL;
    }

    find_features(dev);
    return dev;
}

int hidpp_device_close(hidpp_device_t *dev) {
    if (!dev)
        return HIDPP_EINVAL;

    dev->version = 0;
    return HIDPP_OK;
}

int hidpp_feature_index(hidpp_device_t *dev, uint16_t feature) {
    if (!dev)
        return HIDPP_EINVAL;

    for (int i = 0; i < dev->numFeatures; i++)
        if (dev->features[i] == feature)
            return i;

    return HIDPP_ENOENT;
}

int hidpp_feature_id(hidpp_device_t *dev, uint8_t index) {
    if (!dev)
        return HIDPP_EINVAL;
    return index < dev->numFeatures ? dev->features[index] : HIDPP_ENOENT;
}

int hidpp_device_info(hidpp_device_t *dev, struct hidpp_device_info *out) {
    if (!dev || !out)
        return HIDPP_EINVAL;

    if (dev->lenName == 0)
        find_device_name(dev);

    out->major = HIDPP_MSB(dev->version);
    out->minor = HIDPP_LSB(dev->version);
    out->index = dev->index;
    out->type = find_device_type(dev);
    out->numFeatures = dev->numFeatures;
    out->name = dev->name;
    return HIDPP_OK;
}

int hidpp_ping(hidpp_device_t *dev, uint8_t data) {
    if (!dev)
        return HIDPP_EINVAL;

    hidpp_receiver_t *rcv = dev->receiver;

    hidpp_packet_t req, res;
    make_packet(&req, dev, 0, 1);
    req.params[0] = data;

    if (hidpp_request(rcv, &req, &res) || res.params[2] != req.params[2])
        return HIDPP_EIO;

    return HIDPP_OK;
}

static uint8_t keymap_index(hidpp_device_t *dev) {
    return hidpp_feature_index(dev, 0x1B04);
}

static uint8_t control_count(hidpp_device_t *dev) {
    hidpp_receiver_t *rcv = dev->receiver;
    uint8_t feat = dev->keymap.feature;

    hidpp_packet_t req, res;
    make_packet(&req, dev, feat, 0);

    if (feat == 0 || hidpp_request(rcv, &req, &res))
        return 0;

    return res.params[0];
}

static void find_controls(hidpp_device_t *dev) {
    hidpp_receiver_t *rcv = dev->receiver;
    hidpp_keymap_t *map = &dev->keymap;
    uint8_t feat = map->feature;

    hidpp_packet_t req, res;
    make_packet(&req, dev, feat, 1);

    for (int i = 0; i < map->numControls; i++) {
        req.params[0] = (uint8_t)i;
        map->controls[i] = 0;

        if (HIDPP_OK == hidpp_request(rcv, &req, &res))
            map->controls[i] = HIDPP_WORD(res.params[0], res.params[1]);
    }
}

static hidpp_keymap_t *init_keymap(hidpp_device_t *dev) {
    hidpp_keymap_t *map = &dev->keymap;
    map->device = dev;
    map->numControls = control_count(dev);
    find_controls(dev);
    map->initialized = HIDPP_TRUE;
    return map;
}

hidpp_keymap_t *hidpp_keymap(hidpp_device_t *dev) {
    if (!dev || keymap_index(dev))
        return NULL;

    if (dev->keymap.initialized)
        return &dev->keymap;

    dev->keymap.feature = keymap_index(dev);
    return init_keymap(dev);
}

int hidpp_keymap_index(hidpp_keymap_t *map, uint16_t control) {
    if (!map)
        return HIDPP_EINVAL;

    for (int i = 0; i < map->numControls; i++)
        if (map->controls[i] == control)
            return i;

    return HIDPP_ENOENT;
}

int hidpp_keymap_id(hidpp_keymap_t *map, uint8_t index) {
    if (!map)
        return HIDPP_EINVAL;
    return index < map->numControls ? map->controls[index] : HIDPP_ENOENT;
}

int hidpp_keymap_divert(hidpp_keymap_t *map, uint16_t id, int value) {
    if (!map)
        return HIDPP_EINVAL;

    hidpp_device_t *dev = map->device;
    hidpp_receiver_t *rcv = dev->receiver;

    hidpp_packet_t req = { 0 };
    make_packet(&req, dev, map->feature, 3);
    req.params[0] = HIDPP_MSB(id);
    req.params[1] = HIDPP_LSB(id);
    req.params[2] = value ? 3 : 0;

    return hidpp_send(rcv, &req);
}

int hidpp_keymap_remap(hidpp_keymap_t *map, uint16_t id, uint16_t remap) {
    if (!map)
        return HIDPP_EINVAL;

    hidpp_device_t *dev = map->device;
    hidpp_receiver_t *rcv = dev->receiver;

    hidpp_packet_t req = { 0 };
    make_packet(&req, dev, map->feature, 3);
    req.params[0] = HIDPP_MSB(id);
    req.params[1] = HIDPP_LSB(id);
    req.params[2] = 0;
    req.params[3] = HIDPP_MSB(remap);
    req.params[4] = HIDPP_LSB(remap);

    return hidpp_send(rcv, &req);
}

HIDPP_API size_t hidpp_enumerate(
    unsigned short vid,
    unsigned short pid,
    struct hidpp_receiver_info *out,
    size_t max
) {
    if (!out || max == 0)
        return 0;

    struct hid_device_info *head = hid_enumerate(vid, pid);
    struct hid_device_info *info;
    size_t count = 0;

    for (info = head; info && count < max; info = info->next) {
        if (info->usage_page != 0xFF43)
            continue;

        convert_receiver_info(&out[count++], info);
    }

    hid_free_enumeration(head);
    return count;
}
