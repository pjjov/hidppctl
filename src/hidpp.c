/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include <hidpp.h>

#include <allocator.h>
#include <allocator_std.h>
#include <hidapi/hidapi.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define MAX_ERROR 256
#define MAX_RETRY 16
#define DEFAULT_TIMEOUT 2000
#define DEFAULT_SWID 14

#define HIDPP_WORD(msb, lsb) (((uint16_t)(msb) << 8) | (uint16_t)(lsb))
#define HIDPP_MSB(word) ((uint8_t)(((word) >> 8) & 0xFF))
#define HIDPP_LSB(word) ((uint8_t)((word) & 0xFF))

#define HIDPP_BYTE(msn, lsn) (((msn) << 4) | ((lsn) & 0xF))
#define HIDPP_MSN(word) ((uint8_t)(((word) >> 4) & 0xF))
#define HIDPP_LSN(word) ((uint8_t)((word) & 0xF))

static allocator_t *allocator = NULL;

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
    uint8_t hasInvertFn : 1;

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

const char *hidpp_version(void) { return HIDPP_VERSION_STRING; }

int hidpp_init(allocator_t *alloc) {
    allocator = alloc ? alloc : &standard_allocator;
    return hid_init();
}

void hidpp_exit(void) { hid_exit(); }

static hidpp_receiver_t *create_receiver(hid_device *handle) {
    hidpp_receiver_t *rcv;

    if (!(rcv = allocate(allocator, sizeof(*rcv)))) {
        hid_close(handle);
        return NULL;
    }

    memset(rcv, 0, sizeof(*rcv));
    rcv->handle = handle;
    rcv->retries = MAX_RETRY;
    rcv->timeout = DEFAULT_TIMEOUT;
    rcv->swid = DEFAULT_SWID;
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

HIDPP_API hidpp_receiver_t *hidpp_open_interface(
    unsigned short vid, unsigned short pid, int interfaceNumber
) {
    struct hid_device_info *info, *head = hid_enumerate(vid, pid);
    hidpp_receiver_t *rcv = NULL;

    for (info = head; !rcv && info; info = info->next)
        if (info->interface_number == interfaceNumber)
            rcv = hidpp_open_path(info->path);

    hid_free_enumeration(head);
    return rcv;
}

void hidpp_close(hidpp_receiver_t *rcv) {
    if (!rcv)
        return;

    hid_close(rcv->handle);
    deallocate(allocator, rcv, sizeof(*rcv));
}

static const char *copy_string(const char *str) {
    size_t len = strlen(str) + 1;
    char *out;

    if (!(out = allocate(allocator, len * sizeof(char))))
        return NULL;

    memcpy(out, str, len);
    return out;
}

static const wchar_t *copy_wide_string(const wchar_t *str) {
    size_t len = wcslen(str) + 1;
    wchar_t *out;

    if (!(out = allocate(allocator, len * sizeof(wchar_t))))
        return NULL;

    wmemcpy(out, str, len);
    return out;
}

static void free_wide_string(const wchar_t *str) {
    deallocate(allocator, (void *)str, (wcslen(str) + 1) * sizeof(wchar_t));
}

void hidpp_free_info(struct hidpp_receiver_info *info) {
    deallocate(allocator, (void *)info->path, strlen(info->path) + 1);
    free_wide_string(info->serial);
    free_wide_string(info->manufacturer);
    free_wide_string(info->product);
}

static int convert_receiver_info(
    struct hidpp_receiver_info *out, struct hid_device_info *info
) {
    out->path = copy_string(info->path);
    out->serial = copy_wide_string(info->serial_number);
    out->manufacturer = copy_wide_string(info->manufacturer_string);
    out->product = copy_wide_string(info->product_string);
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

int hidpp_poll(hidpp_receiver_t *rcv, struct hidpp_event *out) {
    if (!rcv || !out)
        return HIDPP_EINVAL;

    hidpp_packet_t pkt;
    int res;

    do {
        if ((res = hidpp_receive(rcv, &pkt)))
            return res;
    } while (pkt.func & 0xF);

    out->type = HIDPP_EVENT_UNKNOWN;
    memcpy(&out->as.unknown, &pkt, sizeof(pkt));
    return HIDPP_OK;
}

/** Sets the timeout in milliseconds for IO operations. **/
int hidpp_set_timeout(hidpp_receiver_t *rcv, int timeout) {
    if (!rcv)
        return HIDPP_EINVAL;

    rcv->timeout = timeout;
    return HIDPP_OK;
}

/** Sets the software id of the HID++ requests. **/
int hidpp_set_swid(hidpp_receiver_t *rcv, uint8_t swid) {
    if (!rcv)
        return HIDPP_EINVAL;

    rcv->swid = swid;
    return HIDPP_OK;
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

static int parse_event(
    hidpp_device_t *dev, struct hidpp_event *e, hidpp_packet_t *pkt
) {
    uint16_t feat = hidpp_feature_id(dev, pkt->feat);
    uint8_t func = pkt->func >> 8;

    switch (feat) {

    case 0x1000: /* Battery status */
        if (func == 0) {
            e->type = HIDPP_EVENT_BATTERY;
            e->as.battery.chargeState = pkt->params[0];
            e->as.battery.batteryLevel = pkt->params[1];
            e->as.battery.chargeStatus = pkt->params[2];
            e->as.battery.externalPower = pkt->params[3];
            return HIDPP_OK;
        }

        break;

    case 0x1b00: /* Remappable buttons */
    case 0x1b02:
    case 0x1b04:
        if (func == 0) {
            e->type = HIDPP_EVENT_BUTTON;
            e->as.buttons[0] = HIDPP_WORD(pkt->params[0], pkt->params[1]);
            e->as.buttons[1] = HIDPP_WORD(pkt->params[2], pkt->params[3]);
            e->as.buttons[2] = HIDPP_WORD(pkt->params[4], pkt->params[5]);
            e->as.buttons[3] = HIDPP_WORD(pkt->params[6], pkt->params[7]);
            return HIDPP_OK;
        } else if (func == 1) {
            e->type = HIDPP_EVENT_MOUSE;
            e->as.mouse[0] = HIDPP_WORD(pkt->params[0], pkt->params[1]);
            e->as.mouse[1] = HIDPP_WORD(pkt->params[2], pkt->params[3]);
            return HIDPP_OK;
        }

        break;

    case 0x2120: /* High resolution wheel */
        if (func == 0) {
            e->type = HIDPP_EVENT_WHEEL;
            e->as.wheel.flags = pkt->params[0];
            e->as.wheel.delta = HIDPP_WORD(pkt->params[1], pkt->params[2]);
            return HIDPP_OK;
        } else if (func == 1) {
            e->type = HIDPP_EVENT_RATCHET_SWITCH;
            e->as.ratchetSwitch = pkt->params[0];
            return HIDPP_OK;
        }

        break;

    case 0x2130: /* Ratchet */
        if (func == 0) {
            e->type = HIDPP_EVENT_RATCHET;
            e->as.ratchet.deltaV = pkt->params[0];
            e->as.ratchet.deltaH = pkt->params[1];
            return HIDPP_OK;
        }

        break;

    case 0x6110:
        if (func == 0) {
            e->type = HIDPP_EVENT_TOUCH_MOUSE_POINTS;
            struct hidpp_touch_mouse_point *p;

            for (int i = 0; i < 4; i++) {
                p = &e->as.touchMousePoints[i];
                uint8_t *params = &pkt->params[i * 4];

                uint16_t xHi = ((uint16_t)params[0]) << 4;
                uint16_t xLo = HIDPP_LSN(params[3]);
                uint16_t yHi = ((uint16_t)params[1]) << 4;
                uint16_t yLo = HIDPP_MSN(params[3]);

                p->x = xHi | xLo;
                p->y = yHi | yLo;
                p->wx = HIDPP_LSN(params[4]);
                p->wx = HIDPP_MSN(params[4]);
            }

            return HIDPP_OK;
        } else if (func == 1) {
            e->type = HIDPP_EVENT_TOUCH_MOUSE_STATUS;
            e->as.touchMouseStatus.flags = pkt->params[0];
            e->as.touchMouseStatus.mouseLifted = pkt->params[0] & 2;
            e->as.touchMouseStatus.buttonDown = pkt->params[0] & 1;
            return HIDPP_OK;
        }

        break;

    case 0x6100:
        if (func == 0) {
            e->type = HIDPP_EVENT_TOUCH_PAD_POINTS;
            e->as.touchPadPoints.timestamp = HIDPP_WORD(
                pkt->params[0], pkt->params[1]
            );

            struct hidpp_touch_pad_point *p;

            for (int i = 0; i < 2; i++) {
                p = &e->as.touchPadPoints.data[i];
                uint8_t *params = &pkt->params[i * 7 + 2];

                uint8_t type = (params[0] & 0xC0) >> 6;
                uint16_t xHi = (uint16_t)params[0] & 0x3F;
                uint16_t xLo = params[1];
                uint8_t status = (params[2] & 0xC0) >> 6;
                uint16_t yHi = (uint16_t)params[2] & 0x3F;
                uint16_t yLo = params[3];

                p->type = type;
                p->status = status;
                p->x = (xHi << 8) | xLo;
                p->y = (yHi << 8) | yLo;
                p->force = params[4];
                p->area = params[5];
                p->flags = params[6];
                p->finger = HIDPP_MSN(params[6]);
            }

            return HIDPP_OK;
        }

        break;

    default:
        break;
    }

    e->type = HIDPP_EVENT_UNKNOWN;
    memcpy(&e->as.unknown, pkt, sizeof(*pkt));
    return HIDPP_EIO;
}

int hidpp_device_poll(hidpp_device_t *dev, struct hidpp_event *out) {
    if (!dev || !out)
        return HIDPP_EINVAL;

    hidpp_packet_t pkt;
    int res;

    do {
        if ((res = hidpp_receive(dev->receiver, &pkt)))
            return res;
    } while (dev->index != pkt.device || pkt.func & 0xF);

    return parse_event(dev, out, &pkt);
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

size_t hidpp_feature_list(hidpp_device_t *dev, uint16_t *out, size_t max) {
    if (!dev || !out || max == 0)
        return 0;

    size_t count = dev->numFeatures < max ? dev->numFeatures : max;
    memcpy(out, dev->features, count * sizeof(*out));
    return count;
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
    if (!dev || 0 == keymap_index(dev))
        return NULL;

    if (dev->keymap.initialized)
        return &dev->keymap;

    dev->keymap.feature = keymap_index(dev);
    return init_keymap(dev);
}

int hidpp_keymap_info(
    hidpp_keymap_t *map, struct hidpp_keymap_info *out, uint16_t id
) {
    if (!map || !out)
        return HIDPP_EINVAL;

    hidpp_receiver_t *rcv = map->device->receiver;
    out->numControls = map->numControls;

    if (id != 0) {
        uint8_t index = hidpp_keymap_index(map, id);

        hidpp_packet_t req, res;
        make_packet(&req, map->device, map->feature, 1);
        req.params[0] = index;

        if (hidpp_request(rcv, &req, &res))
            return HIDPP_EIO;

        out->controlIndex = index;
        out->controlId = HIDPP_WORD(res.params[0], res.params[1]);
        out->taskId = HIDPP_WORD(res.params[2], res.params[3]);
        out->flags = res.params[4];
        out->position = res.params[5];
        out->group = res.params[6];
        out->groupMask = res.params[7];
        out->rawXY = res.params[8];

        make_packet(&req, map->device, map->feature, 2);
        req.params[0] = HIDPP_MSB(id);
        req.params[1] = HIDPP_LSB(id);

        if (hidpp_request(rcv, &req, &res))
            return HIDPP_EIO;

        out->reportFlags = res.params[2];
        out->remapId = HIDPP_WORD(res.params[3], res.params[4]);
    }

    return HIDPP_OK;
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

static int check_invert_fn(hidpp_device_t *dev, uint8_t feat) {
    if (dev->hasInvertFn)
        return HIDPP_OK;

    hidpp_packet_t req, res;
    make_packet(&req, dev, feat, 0);

    if (hidpp_request(dev->receiver, &req, &res))
        return HIDPP_EIO;

    dev->hasInvertFn = res.params[0] & 0x2;
    return dev->hasInvertFn ? HIDPP_OK : HIDPP_ENOSYS;
}

int hidpp_invert_fn(hidpp_device_t *dev, int value) {
    if (!dev)
        return HIDPP_EINVAL;

    uint8_t feat = hidpp_feature_index(dev, 0x40A2);
    hidpp_packet_t req, res;

    if (feat == 0 || check_invert_fn(dev, feat))
        return HIDPP_ENOSYS;

    if (value == HIDPP_TOGGLE) {
        make_packet(&req, dev, feat, 1);

        if (hidpp_request(dev->receiver, &req, &res))
            return HIDPP_EIO;

        value = res.params[0] & 0x2;
    }

    return HIDPP_ENOSYS;
}

static int is_hidpp_compatible(struct hid_device_info *info) {
    if (0 == wcscmp(info->manufacturer_string, L"Logitech"))
        return HIDPP_TRUE;

    return HIDPP_FALSE;
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
        if (is_hidpp_compatible(info))
            convert_receiver_info(&out[count++], info);
    }

    hid_free_enumeration(head);

    return count;
}
