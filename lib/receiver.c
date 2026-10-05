/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "common.h"

#include <allocator.h>
#include <allocator_std.h>
#include <hidapi.h>
#include <pf_socket.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define MAX_RETRY 16
#define DEFAULT_TIMEOUT 2000
#define DEFAULT_SWID 14

allocator_t *hidpp_allocator = NULL;

static void set_error(hidpp_receiver_t *rcv, const wchar_t *fmt, ...) {
    if (!rcv)
        return;

    va_list ap;
    va_start(ap, fmt);
    vswprintf(rcv->error, HIDPP_MAX_ERROR, fmt, ap);
    va_end(ap);
}

static void propagate_error(hidpp_receiver_t *rcv) {
    if (!rcv || rcv->type != RCV_HIDAPI)
        return;

    const wchar_t *e = hid_error(rcv->handle);

    if (e)
        wcsncpy(rcv->error, e, 255);
    else
        wcsncpy(rcv->error, L"Unknown hidapi error", 255);
    rcv->error[255] = L'\0';
}

int hidpp_init(allocator_t *alloc) {
    hidpp_allocator = alloc ? alloc : &standard_allocator;
    return hid_init();
}

void hidpp_exit(void) { hid_exit(); }

static hidpp_receiver_t *create_receiver(void) {
    hidpp_receiver_t *rcv;

    if (!(rcv = allocate(hidpp_allocator, sizeof(*rcv))))
        return NULL;

    memset(rcv, 0, sizeof(*rcv));
    rcv->retries = MAX_RETRY;
    rcv->timeout = DEFAULT_TIMEOUT;
    rcv->swid = DEFAULT_SWID;
    return rcv;
}

hidpp_receiver_t *hidpp_open(
    unsigned short vid, unsigned short pid, const wchar_t *serial
) {
    hidpp_receiver_t *rcv;
    hid_device *handle;

    if (!(handle = hid_open(vid, pid, serial)))
        return NULL;

    if (!(rcv = create_receiver())) {
        hid_close(handle);
        return NULL;
    }

    rcv->type = RCV_HIDAPI;
    rcv->handle = handle;
    return rcv;
}

hidpp_receiver_t *hidpp_open_path(const char *path) {
    hidpp_receiver_t *rcv;
    hid_device *handle;

    if (!(handle = hid_open_path(path)))
        return NULL;

    if (!(rcv = create_receiver())) {
        hid_close(handle);
        return NULL;
    }

    rcv->type = RCV_HIDAPI;
    rcv->handle = handle;
    return rcv;
}

hidpp_receiver_t *hidpp_open_interface(
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

hidpp_receiver_t *hidpp_open_socket(int fd) {
    if (fd < 0)
        return NULL;

    hidpp_receiver_t *rcv;

    if ((rcv = create_receiver()))
        return NULL;

    rcv->type = RCV_SOCKET;
    rcv->socket = fd;
    return rcv;
}

hidpp_receiver_t *hidpp_open_custom(
    hidpp_receiver_send_fn *send, hidpp_receiver_recv_fn *recv, void *user
) {
    if (!send || !recv)
        return NULL;

    hidpp_receiver_t *rcv;

    if ((rcv = create_receiver()))
        return NULL;

    rcv->type = RCV_CUSTOM;
    rcv->customSend = send;
    rcv->customRecv = recv;
    rcv->customUser = user;
    return rcv;
}

void hidpp_close(hidpp_receiver_t *rcv) {
    if (!rcv)
        return;

    if (rcv->type == RCV_HIDAPI)
        hid_close(rcv->handle);
    deallocate(hidpp_allocator, rcv, sizeof(*rcv));
}

static const char *copy_string(const char *str) {
    size_t len = strlen(str) + 1;
    char *out;

    if (!(out = allocate(hidpp_allocator, len * sizeof(char))))
        return NULL;

    memcpy(out, str, len);
    return out;
}

static const wchar_t *copy_wide_string(const wchar_t *str) {
    size_t len = wcslen(str) + 1;
    wchar_t *out;

    if (!(out = allocate(hidpp_allocator, len * sizeof(wchar_t))))
        return NULL;

    wmemcpy(out, str, len);
    return out;
}

static void free_wide_string(const wchar_t *str) {
    deallocate(
        hidpp_allocator, (void *)str, (wcslen(str) + 1) * sizeof(wchar_t)
    );
}

void hidpp_free_info(struct hidpp_receiver_info *info) {
    deallocate(hidpp_allocator, (void *)info->path, strlen(info->path) + 1);
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

    if (rcv->type != RCV_HIDAPI)
        return HIDPP_ENOSYS;

    struct hid_device_info *info;
    if (!(info = hid_get_device_info(rcv->handle)))
        return HIDPP_EIO;

    convert_receiver_info(out, info);
    return HIDPP_OK;
}

hidpp_protocol_hook_fn *hidpp_set_protocol_hook(
    hidpp_receiver_t *rcv, hidpp_protocol_hook_fn *hook, void *user
) {
    if (!rcv)
        return NULL;

    hidpp_protocol_hook_fn *prev = rcv->hook;
    rcv->hook = hook;
    rcv->hookUser = user;
    return prev;
}

static int send_packet(hidpp_receiver_t *rcv, unsigned char *buf, size_t len) {
    switch (rcv->type) {
    case RCV_HIDAPI:
        return hid_write(rcv->handle, buf, len) < 0;
    case RCV_CUSTOM:
        return rcv->customSend(rcv, (char *)buf, len, rcv->customUser);
    case RCV_SOCKET: {
        pf_sock_ssize_t sent = pf_sock_send(rcv->socket, buf, len, 0);

        if (sent < len)
            return HIDPP_EIO;
        return HIDPP_OK;
    }
    default:
        return HIDPP_ENOSYS;
    }
}

int hidpp_send(hidpp_receiver_t *rcv, hidpp_packet_t *pkt) {
    if (!rcv || !pkt)
        return HIDPP_EINVAL;

    int rc;

    if (rcv->hook && (rc = rcv->hook(rcv, pkt, NULL, rcv->hookUser)))
        return rc == HIDPP_HOOK_SKIP_IO ? HIDPP_OK : rc;

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

    if (send_packet(rcv, buf, len)) {
        set_error(rcv, L"tried to write %lu bytes.", len);
        return HIDPP_EIO;
    }

    return HIDPP_OK;
}

static int recv_packet(hidpp_receiver_t *rcv, unsigned char *buf, size_t len) {
    switch (rcv->type) {
    case RCV_HIDAPI:
        return rcv->nonblocking
            ? hid_read(rcv->handle, buf, len)
            : hid_read_timeout(rcv->handle, buf, len, rcv->timeout);
    case RCV_CUSTOM:
        return rcv->customRecv(rcv, (char *)buf, len, rcv->customUser);
    case RCV_SOCKET: {
        pf_sock_ssize_t received = pf_sock_recv(rcv->socket, buf, len, 0);

        if (received < 0)
            return HIDPP_EIO;
        if (received != len)
            return HIDPP_ENODATA;
        return HIDPP_OK;
    }
    default:
        return HIDPP_ENOSYS;
    }
}

int hidpp_receive(hidpp_receiver_t *rcv, hidpp_packet_t *out) {
    if (!rcv || !out)
        return HIDPP_EINVAL;

    unsigned char buf[HIDPP_LEN_XLONG];

    int ret = recv_packet(rcv, buf, HIDPP_LEN_XLONG);

    if (ret == 0) {
        if (rcv->nonblocking)
            return HIDPP_EAGAIN;
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

    if (rcv->hook)
        return rcv->hook(rcv, rcv->currentRequest, out, rcv->hookUser);

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
    /* hidpp_request always waits for a matching response, even if the
       receiver has been put into non-blocking mode for event-loop
       polling elsewhere -- save/restore that flag around the retry
       loop below. */
    hidpp_bool_t wasNonblocking = rcv->nonblocking;
    rcv->nonblocking = 0;

    int ret = hidpp_send(rcv, request);

    if (ret < 0) {
        rcv->nonblocking = wasNonblocking;
        return ret;
    }

    for (int attempts = 0; attempts < rcv->retries; ++attempts) {
        rcv->currentRequest = request;
        ret = hidpp_receive(rcv, response);
        rcv->currentRequest = NULL;

        if (ret < 0) {
            rcv->nonblocking = wasNonblocking;
            return ret;
        }

        if (is_error_packet(request, response)) {
            const char *error = hidpp_strerror(response->params[2]);
            set_error(rcv, L"HID++ error: %hs", error);
            rcv->nonblocking = wasNonblocking;
            return HIDPP_EIO;
        }

        if (is_good_packet(request, response)) {
            rcv->nonblocking = wasNonblocking;
            return HIDPP_OK;
        }
    }

    set_error(rcv, L"No response after %d reads", MAX_RETRY);
    rcv->nonblocking = wasNonblocking;
    return HIDPP_ENODATA;
}

int hidpp_set_timeout(hidpp_receiver_t *rcv, int timeout) {
    if (!rcv)
        return HIDPP_EINVAL;

    rcv->timeout = timeout;
    return HIDPP_OK;
}

int hidpp_set_nonblocking(hidpp_receiver_t *rcv, int nonblock) {
    if (!rcv)
        return HIDPP_EINVAL;

    if (rcv->type != RCV_HIDAPI)
        return HIDPP_ENOSYS;

    if (hid_set_nonblocking(rcv->handle, nonblock ? 1 : 0) < 0) {
        propagate_error(rcv);
        return HIDPP_EIO;
    }

    rcv->nonblocking = nonblock ? 1 : 0;
    return HIDPP_OK;
}

int hidpp_set_swid(hidpp_receiver_t *rcv, uint8_t swid) {
    if (!rcv)
        return HIDPP_EINVAL;

    rcv->swid = swid;
    return HIDPP_OK;
}

static int is_hidpp_compatible(struct hid_device_info *info) {
    if (0 == wcscmp(info->manufacturer_string, L"Logitech"))
        return HIDPP_TRUE;

    return HIDPP_FALSE;
}

size_t hidpp_enumerate(
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
