/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#ifndef HIDPP_H
#define HIDPP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#define HIDPP_KIND_SHORT 0x10 /**< Short report  (7 bytes payload) */
#define HIDPP_KIND_LONG 0x11 /**< Long report  (20 bytes payload) */
#define HIDPP_KIND_XLONG 0x12 /**< Very-long report (64 bytes payload) */

#define HIDPP_LEN_SHORT 7
#define HIDPP_LEN_LONG 20
#define HIDPP_LEN_XLONG 64

enum hidpp_error {
    HIDPP_OK = 0,
    HIDPP_ENOENT = -2,
    HIDPP_EINTR = -4,
    HIDPP_EIO = -5,
    HIDPP_EAGAIN = -11,
    HIDPP_ENOMEM = -12,
    HIDPP_EEXIST = -17,
    HIDPP_EINVAL = -22,
    HIDPP_ENOSYS = -38,
    HIDPP_ENODATA = -61,
    HIDPP_ETIMEDOUT = -110,
};

typedef struct hidpp_receiver hidpp_receiver;
typedef struct hidpp_device hidpp_device;

typedef struct hidpp_packet {
    uint8_t kind;
    uint8_t device;
    uint8_t feat;
    uint8_t func;
    uint8_t params[HIDPP_LEN_XLONG - 4];
} hidpp_packet;

/** Initialize the underlying hidapi library. **/
int hidpp_init(void);

/** Finalize the underlying hidapi library. **/
void hidpp_exit(void);

/** Open a HID++ receiver by Vendor/Product ID. If multiple matching
    receivers are attached, the first enumerated is opened.

    > Use hidpp_open_path() to select a specific interface.
**/
hidpp_receiver *hidpp_open(
    unsigned short vid, unsigned short pid, const wchar_t *serial
);

/** Open a HID++ receiver by platform path. **/
hidpp_receiver *hidpp_open_path(const char *path);

/** Close a HID++ receiver and free its resources. **/
void hidpp_close(hidpp_receiver *rcv);

/** Initializes `out` with passed parameters. **/
int hidpp_make(
    hidpp_packet *out, uint8_t kind, uint8_t dev, uint8_t feat, uint8_t func
);

/** Send `pkt` to HID++ receiver. **/
int hidpp_send(hidpp_receiver *rcv, hidpp_packet *pkt);

/** Read one HID++ report, blocking for up to `timeout`. **/
int hidpp_receive(hidpp_receiver *rcv, hidpp_packet *out, int timeout);

/** Send a packet and receive the matching response. Automatically
    retries on unrelated incoming packets (e.g. HID input reports).
**/
int hidpp_request(
    hidpp_receiver *rcv,
    hidpp_packet *request,
    hidpp_packet *response,
    int timeout
);

/** Enumerate HID++ capable devices using `hid_enumerate`. **/
struct hid_device_info *hidpp_enumerate(unsigned short vid, unsigned short pid);

#ifdef __cplusplus
}
#endif

#endif
