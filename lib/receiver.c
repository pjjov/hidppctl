/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "common.h"

#include <allocator.h>
#include <allocator_std.h>
#ifdef HIDPP_HAVE_HIDAPI
    #include <hidapi.h>
#endif
#include <pf_macro.h>
#include <pf_socket.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define MAX_RETRY 16
#define DEFAULT_TIMEOUT 2000
#define DEFAULT_SWID 14

/* usable without hidpp_init(), which only replaces it */
allocator_t *hidpp_allocator = &standard_allocator;

static void set_error(hidpp_receiver_t *rcv, const wchar_t *fmt, ...) {
    if (!rcv)
        return;

    va_list ap;
    va_start(ap, fmt);
    vswprintf(rcv->error, HIDPP_MAX_ERROR, fmt, ap);
    va_end(ap);
}

#ifdef HIDPP_HAVE_HIDAPI
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
#endif

int hidpp_init(allocator_t *alloc) {
    hidpp_allocator = alloc ? alloc : &standard_allocator;
#ifdef HIDPP_HAVE_HIDAPI
    return hid_init();
#else
    return HIDPP_OK;
#endif
}

void hidpp_exit(void) {
#ifdef HIDPP_HAVE_HIDAPI
    hid_exit();
#endif
}

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

#ifdef HIDPP_HAVE_HIDAPI
static void remember_origin(hidpp_receiver_t *rcv) {
    const struct hid_device_info *info = hid_get_device_info(rcv->handle);

    if (!info)
        return;

    strncpy(rcv->path, info->path ? info->path : "", sizeof(rcv->path) - 1);
    rcv->vendorId = info->vendor_id;
    rcv->productId = info->product_id;
    rcv->interfaceNumber = info->interface_number;
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
    remember_origin(rcv);
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
    remember_origin(rcv);
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

int hidpp_reopen(hidpp_receiver_t *rcv) {
    if (!rcv)
        return HIDPP_EINVAL;

    if (rcv->type != RCV_HIDAPI)
        return HIDPP_ENOSYS;

    hid_device *handle = NULL;

    if (rcv->path[0])
        handle = hid_open_path(rcv->path);

    /* the path can change when the device is reattached */
    if (!handle) {
        struct hid_device_info *info, *head;
        head = hid_enumerate(rcv->vendorId, rcv->productId);

        for (info = head; !handle && info; info = info->next)
            if (info->interface_number == rcv->interfaceNumber)
                handle = hid_open_path(info->path);

        hid_free_enumeration(head);
    }

    if (!handle) {
        set_error(rcv, L"Unable to reopen the receiver");
        return HIDPP_ENOENT;
    }

    hid_close(rcv->handle);
    rcv->handle = handle;
    remember_origin(rcv);

    if (rcv->nonblocking)
        hid_set_nonblocking(rcv->handle, 1);

    return HIDPP_OK;
}
#else
hidpp_receiver_t *hidpp_open(
    unsigned short vid, unsigned short pid, const wchar_t *serial
) {
    (void)vid, (void)pid, (void)serial;
    return NULL;
}

hidpp_receiver_t *hidpp_open_path(const char *path) {
    (void)path;
    return NULL;
}

hidpp_receiver_t *hidpp_open_interface(
    unsigned short vid, unsigned short pid, int interfaceNumber
) {
    (void)vid, (void)pid, (void)interfaceNumber;
    return NULL;
}

int hidpp_reopen(hidpp_receiver_t *rcv) {
    return rcv ? HIDPP_ENOSYS : HIDPP_EINVAL;
}
#endif

hidpp_receiver_t *hidpp_open_socket(int fd) {
    if (fd < 0)
        return NULL;

    hidpp_receiver_t *rcv;

    if (!(rcv = create_receiver()))
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

    if (!(rcv = create_receiver()))
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

    for (size_t i = 0; i < PF_COUNTOF(rcv->devices); i++)
        if (rcv->devices[i])
            hidpp_device_close(rcv->devices[i]);

#ifdef HIDPP_HAVE_HIDAPI
    if (rcv->type == RCV_HIDAPI)
        hid_close(rcv->handle);
#endif
    deallocate(hidpp_allocator, rcv, sizeof(*rcv));
}

#ifdef HIDPP_HAVE_HIDAPI
static void copy_string(char *dst, size_t size, const char *src) {
    if (!src)
        src = "";

    strncpy(dst, src, size - 1);
    dst[size - 1] = '\0';
}

static void copy_wide_string(wchar_t *dst, size_t size, const wchar_t *src) {
    if (!src)
        src = L"";

    wcsncpy(dst, src, size - 1);
    dst[size - 1] = L'\0';
}

static void convert_receiver_info(
    struct hidpp_receiver_info *out, const struct hid_device_info *info
) {
    copy_string(out->path, sizeof(out->path), info->path);
    copy_wide_string(out->serial, PF_COUNTOF(out->serial), info->serial_number);
    copy_wide_string(
        out->manufacturer,
        PF_COUNTOF(out->manufacturer),
        info->manufacturer_string
    );
    copy_wide_string(
        out->product, PF_COUNTOF(out->product), info->product_string
    );
    out->vendorId = info->vendor_id;
    out->productId = info->product_id;
    out->releaseNumber = info->release_number;
    out->usagePage = info->usage_page;
    out->usage = info->usage;
    out->interfaceNumber = info->interface_number;
    out->busType = info->bus_type;
}

const struct hidpp_receiver_info *hidpp_receiver_info(hidpp_receiver_t *rcv) {
    if (!rcv)
        return NULL;

    if (rcv->type != RCV_HIDAPI) {
        set_error(rcv, L"Receiver information is only available for hidapi");
        return NULL;
    }

    const struct hid_device_info *info;
    if (!(info = hid_get_device_info(rcv->handle))) {
        propagate_error(rcv);
        return NULL;
    }

    convert_receiver_info(&rcv->info, info);
    return &rcv->info;
}
#else
const struct hidpp_receiver_info *hidpp_receiver_info(hidpp_receiver_t *rcv) {
    set_error(rcv, L"Receiver information is only available for hidapi");
    return NULL;
}
#endif

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
#ifdef HIDPP_HAVE_HIDAPI
    case RCV_HIDAPI:
        return hid_write(rcv->handle, buf, len) < 0;
#endif
    case RCV_CUSTOM:
        return rcv->customSend(rcv, (char *)buf, len, rcv->customUser);
    case RCV_SOCKET: {
        pf_sock_ssize_t sent = pf_sock_send(rcv->socket, buf, len, 0);

        if (sent < 0 || (size_t)sent < len)
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
        set_error(rcv, L"tried to write %zu bytes.", len);
        return HIDPP_EIO;
    }

    return HIDPP_OK;
}

/*  Waits until the socket is readable. Returns 1 when it is, 0 on timeout
    (`timeout` is in milliseconds, negative waits forever) or a negative
    error code. */
static int socket_wait(int fd, int timeout) {
    pf_pollfd_t pfd = { 0 };
    pfd.fd = (pf_sock_t)fd;
    pfd.events = POLLIN;

    int rc = pf_sock_poll(&pfd, 1, timeout);

    if (rc < 0)
        return HIDPP_EIO;
    return rc > 0;
}

/*  Reads exactly `count` bytes from a stream socket, giving up if nothing
    arrives within `timeout` milliseconds. */
static int socket_read_exact(
    int fd, unsigned char *buf, size_t count, int timeout
) {
    size_t total = 0;

    while (total < count) {
        int rc = socket_wait(fd, timeout);

        if (rc < 0)
            return rc;
        if (rc == 0)
            return HIDPP_ETIMEDOUT;

        pf_sock_ssize_t got = pf_sock_recv(fd, buf + total, count - total, 0);

        if (got < 0)
            return HIDPP_EIO;
        if (got == 0)
            return HIDPP_ENODATA; /* peer closed the connection */

        total += (size_t)got;
    }

    return HIDPP_OK;
}

/*  HID++ reports are not length-prefixed, but the report kind implies the
    length, so a stream socket can be framed by reading the first byte
    first. Returns the packet length, 0 if no packet started within
    `timeout` milliseconds or a negative error code. */
static int socket_read_packet(
    int fd, unsigned char *buf, size_t max, int timeout
) {
    int rc = socket_wait(fd, timeout);

    if (rc <= 0)
        return rc;

    if ((rc = socket_read_exact(fd, buf, 1, timeout)))
        return rc;

    size_t len = packet_length(buf[0]);

    if (len == 0 || len > max)
        return HIDPP_EIO; /* stream is out of sync */

    /* A packet which stops halfway leaves the stream out of sync as well. */
    rc = socket_read_exact(fd, buf + 1, len - 1, timeout);
    if (rc == HIDPP_ETIMEDOUT)
        return HIDPP_EIO;
    if (rc)
        return rc;

    return (int)len;
}

static int recv_packet(hidpp_receiver_t *rcv, unsigned char *buf, size_t len) {
    switch (rcv->type) {
#ifdef HIDPP_HAVE_HIDAPI
    case RCV_HIDAPI:
        return rcv->nonblocking
            ? hid_read(rcv->handle, buf, len)
            : hid_read_timeout(rcv->handle, buf, len, rcv->timeout);
#endif
    case RCV_CUSTOM:
        return rcv->customRecv(rcv, (char *)buf, len, rcv->customUser);
    case RCV_SOCKET:
        return socket_read_packet(
            rcv->socket, buf, len, rcv->nonblocking ? 0 : rcv->timeout
        );
    default:
        return HIDPP_ENOSYS;
    }
}

int hidpp_receive(hidpp_receiver_t *rcv, hidpp_packet_t *out) {
    if (!rcv || !out)
        return HIDPP_EINVAL;

    unsigned char buf[HIDPP_LEN_XLONG] = { 0 };

    int ret = recv_packet(rcv, buf, HIDPP_LEN_XLONG);

    if (ret == 0) {
        if (rcv->nonblocking)
            return HIDPP_EAGAIN;
        set_error(rcv, L"Packet reading timed out");
        return HIDPP_ETIMEDOUT;
    } else if (ret < 0) {
#ifdef HIDPP_HAVE_HIDAPI
        if (rcv->type == RCV_HIDAPI) {
            propagate_error(rcv);
            return HIDPP_EIO;
        }
#endif

        /* socket and custom receivers already report hidpp_error codes */
        set_error(rcv, L"Receiving a packet failed (%d)", ret);
        return ret;
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

/*  HID++ 2.0 error report:
        [kind] [device] [0xFF] [feature index] [function | swid] [error code]
    HID++ 1.0 error report:
        [kind] [device] [0x8F] [sub id] [address] [error code]
    In both cases bytes 3 and 4 echo the first two bytes of the request, which
    in the packet structure are `func` and `params[0]`. The error code is
    stored in `params[1]`.
*/
#define HIDPP2_ERROR_FEATURE 0xFF
#define HIDPP1_ERROR_SUB_ID 0x8F

static int is_error_packet(
    const hidpp_packet_t *req, const hidpp_packet_t *res
) {
    return res->device == req->device
        && (res->feat == HIDPP2_ERROR_FEATURE
            || res->feat == HIDPP1_ERROR_SUB_ID)
        && res->func == req->feat && res->params[0] == req->func;
}

/*  Responses echo feature index, function and software id. Comparing the
    software id as well is what separates responses to our own requests from
    notifications (which always use swid 0) and from other software's replies.
*/
static int is_good_packet(
    const hidpp_packet_t *req, const hidpp_packet_t *res
) {
    return res->device == req->device && res->feat == req->feat
        && res->func == req->func;
}

const char *hidpp_protocol_error_str(int code) {
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

static int request_impl(
    hidpp_receiver_t *rcv, hidpp_packet_t *request, hidpp_packet_t *response
) {
    /* hidpp_request always waits for a matching response, even if the
       receiver has been put into non-blocking mode for event-loop
       polling elsewhere -- save/restore that flag around the retry
       loop below. */
    if (!rcv || !request || !response)
        return HIDPP_EINVAL;

    hidpp_bool_t wasNonblocking = rcv->nonblocking;
    rcv->nonblocking = 0;
    rcv->errorCode = 0;

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
            const char *error = hidpp_protocol_error_str(response->params[1]);
            set_error(
                rcv,
                L"HID++ %hs error: %hs (0x%02X)",
                response->feat == HIDPP1_ERROR_SUB_ID ? "1.0" : "2.0",
                error,
                response->params[1]
            );
            rcv->errorCode = response->params[1];
            rcv->nonblocking = wasNonblocking;
            return HIDPP_EIO;
        }

        if (is_good_packet(request, response)) {
            rcv->nonblocking = wasNonblocking;
            return HIDPP_OK;
        }
    }

    set_error(rcv, L"No response after %d reads", (int)rcv->retries);
    rcv->nonblocking = wasNonblocking;
    return HIDPP_ENODATA;
}

int hidpp_request(
    hidpp_receiver_t *rcv, hidpp_packet_t *request, hidpp_packet_t *response
) {
    int ret = request_impl(rcv, request, response);

    if (rcv)
        rcv->status = ret;
    return ret;
}

int hidpp_status(hidpp_receiver_t *rcv) { return rcv ? rcv->status : 0; }

int hidpp_error_code(hidpp_receiver_t *rcv) { return rcv ? rcv->errorCode : 0; }

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

#ifdef HIDPP_HAVE_HIDAPI
    if (hid_set_nonblocking(rcv->handle, nonblock ? 1 : 0) < 0) {
        propagate_error(rcv);
        return HIDPP_EIO;
    }
#endif

    rcv->nonblocking = nonblock ? 1 : 0;
    return HIDPP_OK;
}

int hidpp_set_swid(hidpp_receiver_t *rcv, uint8_t swid) {
    /* The software id is a nibble and 0 is used by device notifications. */
    if (!rcv || swid == 0 || swid > 0xF)
        return HIDPP_EINVAL;

    rcv->swid = swid;
    return HIDPP_OK;
}

#ifdef HIDPP_HAVE_HIDAPI
    #define LOGITECH_VENDOR_ID 0x046D

static int is_hidpp_compatible(struct hid_device_info *info) {
    /*  Bluetooth and some receivers report no manufacturer string, so
        the vendor id is checked first. */
    if (info->vendor_id == LOGITECH_VENDOR_ID)
        return HIDPP_TRUE;

    if (info->manufacturer_string
        && 0 == wcscmp(info->manufacturer_string, L"Logitech"))
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
#else
size_t hidpp_enumerate(
    unsigned short vid,
    unsigned short pid,
    struct hidpp_receiver_info *out,
    size_t max
) {
    (void)vid, (void)pid, (void)out, (void)max;
    return 0;
}
#endif
