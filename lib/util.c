/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "common.h" /* IWYU pragma: keep */

#include <hidapi.h>

const char *hidpp_version(void) { return HIDPP_VERSION_STRING; }

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

const wchar_t *hidpp_error(hidpp_receiver_t *rcv) {
    if (!rcv)
        return hid_error(NULL);
    if (rcv->error[0] != L'\0')
        return rcv->error;
    if (rcv->type == RCV_HIDAPI)
        return hid_error(rcv->handle);
    return NULL;
}

const char *hidpp_error_str(int code) {
    switch (code) {
        /* clang-format off */
    case HIDPP_OK:         return "Success";
    case HIDPP_ENOENT:     return "No such entry";
    case HIDPP_EINTR:      return "Interrupted";
    case HIDPP_EIO:        return "I/O error";
    case HIDPP_EAGAIN:     return "Resource temporarily unavailable";
    case HIDPP_ENOMEM:     return "Out of memory";
    case HIDPP_EEXIST:     return "Already exists";
    case HIDPP_EINVAL:     return "Invalid argument";
    case HIDPP_ENOSYS:     return "Not supported";
    case HIDPP_ENODATA:    return "No data available";
    case HIDPP_ETIMEDOUT:  return "Operation timed out";
    default:               return "Unknown error";
        /* clang-format on */
    }
}
