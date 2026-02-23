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

#define HIDPP_WORD(msb, lsb) (((uint16_t)(msb) << 8) | (uint16_t)(lsb))
#define HIDPP_MSB(word) ((uint8_t)(((word) >> 8) & 0xF))
#define HIDPP_LSB(word) ((uint8_t)((word) & 0xF))

struct hidpp_receiver {
    hid_device *handle;
    wchar_t error[MAX_ERROR];
};

static void set_error(hidpp_receiver *rcv, const wchar_t *fmt, ...) {
    if (!rcv)
        return;

    va_list ap;
    va_start(ap, fmt);
    vswprintf(rcv->error, MAX_ERROR, fmt, ap);
    va_end(ap);
}

static void propagate_error(hidpp_receiver *rcv) {
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

int hidpp_make(
    hidpp_packet *out, uint8_t kind, uint8_t device, uint8_t feat, uint8_t func
) {
    out->kind = kind;
    out->device = device;
    out->feat = feat;
    out->func = func;
    memset(out->params, 0, sizeof(out->params));
    return HIDPP_OK;
}

int hidpp_init(void) { return hid_init(); }
void hidpp_exit(void) { hid_exit(); }

static hidpp_receiver *create_receiver(hid_device *handle) {
    hidpp_receiver *rcv;

    if (!(rcv = calloc(1, sizeof(*rcv)))) {
        hid_close(handle);
        return NULL;
    }

    rcv->handle = handle;
    return rcv;
}

hidpp_receiver *hidpp_open(
    unsigned short vid, unsigned short pid, const wchar_t *serial
) {
    hid_device *handle = hid_open(vid, pid, serial);
    return handle ? create_receiver(handle) : NULL;
}

hidpp_receiver *hidpp_open_path(const char *path) {
    hid_device *handle = hid_open_path(path);
    return handle ? create_receiver(handle) : NULL;
}

void hidpp_close(hidpp_receiver *rcv) {
    if (!rcv)
        return;

    hid_close(rcv->handle);
    free(rcv);
}

int hidpp_send(hidpp_receiver *rcv, hidpp_packet *pkt) {
    size_t len = packet_length(pkt->kind);

    if (len <= 0) {
        set_error(rcv, L"hidpp_send: invalid packet kind");
        return HIDPP_EINVAL;
    }

    if (hid_write(rcv->handle, (uint8_t *)pkt, len) < 0) {
        propagate_error(rcv);
        return HIDPP_EIO;
    }

    return HIDPP_OK;
}

int hidpp_receive(hidpp_receiver *rcv, hidpp_packet *out, int timeout) {
    int ret = hid_read_timeout(
        rcv->handle, (uint8_t *)out, sizeof(*out), timeout
    );

    if (ret < 0) {
        propagate_error(rcv);
        return HIDPP_EIO;
    } else if (ret < 4) {
        set_error(rcv, L"hidpp_receive: read invalid packet");
        return HIDPP_EIO;
    } else if (ret == 0) {
        set_error(rcv, L"hidpp_receive: read timed out");
        return HIDPP_ETIMEDOUT;
    }

    return HIDPP_OK;
}

static int is_error_packet(const hidpp_packet *req, const hidpp_packet *res) {
    return res->device == req->device && res->feat == 0xFF
        && res->params[0] == req->feat && res->params[1] == req->func;
}

static int is_good_packet(const hidpp_packet *req, const hidpp_packet *res) {
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
    hidpp_receiver *rcv,
    hidpp_packet *request,
    hidpp_packet *response,
    int timeout
) {
    int ret = hidpp_send(rcv, request);

    if (ret < 0)
        return ret;

    for (int attempts = 0; attempts < MAX_RETRY; ++attempts) {
        ret = hidpp_receive(rcv, response, timeout);

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

    set_error(rcv, L"hidpp_request: no response after %d reads", MAX_RETRY);
    return -1;
}

const wchar_t *hidpp_error(hidpp_receiver *rcv) {
    if (!rcv)
        return hid_error(NULL);
    if (rcv->error[0] != L'\0')
        return rcv->error;
    return hid_error(rcv->handle);
}

struct hid_device_info *hidpp_enumerate(
    unsigned short vid, unsigned short pid
) {
    struct hid_device_info *head = hid_enumerate(vid, pid);
    struct hid_device_info *prev = NULL, *next;

    for (struct hid_device_info *cur = head; cur; cur = next) {
        next = cur->next;

        if (cur->usage_page != 0xFF43) {
            if (prev)
                prev->next = next;
            else
                head = next;
            cur->next = NULL;
            hid_free_enumeration(cur);
        } else {
            prev = cur;
        }
    }
    return head;
}
