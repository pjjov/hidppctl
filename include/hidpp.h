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

#ifndef HIDPP_INLINE
    #define HIDPP_INLINE static inline
#endif

#ifndef HIDPP_API
    #define HIDPP_API
#endif

#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

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

enum hidpp_bool {
    HIDPP_FALSE = 0,
    HIDPP_TRUE = 1,
};

enum hidpp_device_type {
    HIDPP_TYPE_KEYBOARD,
    HIDPP_TYPE_REMOTE,
    HIDPP_TYPE_NUMPAD,
    HIDPP_TYPE_MOUSE,
    HIDPP_TYPE_TOUCHPAD,
    HIDPP_TYPE_TRACKBALL,
    HIDPP_TYPE_PRESENTER,
    HIDPP_TYPE_RECEIVER,
};

typedef struct allocator_t allocator_t;
typedef struct hidpp_receiver_t hidpp_receiver_t;
typedef struct hidpp_device_t hidpp_device_t;
typedef struct hidpp_keymap_t hidpp_keymap_t;

typedef struct hidpp_packet_t {
    uint8_t kind;
    uint8_t device;
    uint8_t feat;
    uint8_t func;
    uint8_t params[HIDPP_LEN_XLONG - 4];
} hidpp_packet_t;

struct hidpp_device_info {
    uint8_t major;
    uint8_t minor;

    uint8_t index;
    uint8_t type;
    uint8_t numFeatures;
    const char *name;
};

struct hidpp_receiver_info {
    const char *path;
    const wchar_t *serial;
    const wchar_t *manufacturer;
    const wchar_t *product;

    uint16_t vendorId;
    uint16_t productId;
    uint16_t releaseNumber;
    uint16_t usagePage;
    uint16_t usage;
    int interfaceNumber;
    int busType;

    void *_enumerate;
};

struct hidpp_keymap_info {
    uint8_t numControls;

    uint8_t controlIndex;
    uint16_t controlId;
    uint16_t taskId;
    uint8_t flags;
    uint8_t position;
    uint8_t group;
    uint8_t groupMask;
    uint8_t rawXY;

    uint8_t reportFlags;
    uint16_t remapId;
};

/** Initialize the underlying hidapi library. **/
HIDPP_API int hidpp_init(allocator_t *allocator);

/** Finalize the underlying hidapi library. **/
HIDPP_API void hidpp_exit(void);

/** Open a HID++ receiver by Vendor/Product ID. If multiple matching
    receivers are attached, the first enumerated is opened.

    > Use hidpp_open_path() to select a specific interface.
**/
HIDPP_API hidpp_receiver_t *hidpp_open(
    unsigned short vid, unsigned short pid, const wchar_t *serial
);

/** Open a HID++ receiver by platform path. **/
HIDPP_API hidpp_receiver_t *hidpp_open_path(const char *path);

/** Close a HID++ receiver and free its resources. **/
HIDPP_API void hidpp_close(hidpp_receiver_t *rcv);

/** Initializes `out` with passed parameters. **/
HIDPP_API int hidpp_make(
    hidpp_packet_t *out,
    uint8_t dev,
    uint8_t feat,
    uint8_t func,
    uint8_t *data,
    size_t length
);

/** Send `pkt` to HID++ receiver. **/
HIDPP_API int hidpp_send(hidpp_receiver_t *rcv, hidpp_packet_t *pkt);

/** Read one HID++ report, blocking for up to `timeout`. **/
HIDPP_API int hidpp_receive(hidpp_receiver_t *rcv, hidpp_packet_t *out);

/** Send a packet and receive the matching response. Automatically
    retries on unrelated incoming packets (e.g. HID input reports).
**/
HIDPP_API int hidpp_request(
    hidpp_receiver_t *rcv, hidpp_packet_t *request, hidpp_packet_t *response
);

/** Opens the device at index `device` and returns the connection. **/
HIDPP_API hidpp_device_t *hidpp_device_open(
    hidpp_receiver_t *rcv, uint8_t device
);

/** Closes the `device` from the receiver. **/
HIDPP_API int hidpp_device_close(hidpp_device_t *dev);

/** Resolve a feature ID to its index on the device. **/
HIDPP_API int hidpp_feature_index(hidpp_device_t *dev, uint16_t feature);

/** Resolve a feature index to its ID on the device. **/
HIDPP_API int hidpp_feature_id(hidpp_device_t *dev, uint8_t index);

/** Returns the name of the feature with the passed ID. **/
HIDPP_API const char *hidpp_feature_name(uint16_t feature);

/** Reads receiver information to `out`. **/
HIDPP_API int hidpp_receiver_info(
    hidpp_receiver_t *rcv, struct hidpp_receiver_info *out
);

HIDPP_API void hidpp_free_info(struct hidpp_receiver_info *info);

/** Reads device information to `out`. **/
HIDPP_API int hidpp_device_info(
    hidpp_device_t *dev, struct hidpp_device_info *out
);

/** Pings the device with `data`. **/
HIDPP_API int hidpp_ping(hidpp_device_t *dev, uint8_t data);

/** Returns the object for configuring device's keybindings. **/
HIDPP_API hidpp_keymap_t *hidpp_keymap(hidpp_device_t *dev);

/** Reads device's keymap information to `out`. **/
HIDPP_API int hidpp_keymap_info(
    hidpp_keymap_t *map, struct hidpp_keymap_info *out, uint16_t id
);

/** Resolve a control ID to its index on the device. **/
HIDPP_API int hidpp_keymap_index(hidpp_keymap_t *map, uint16_t control);

/** Resolve a keymap index to its ID on the device. **/
HIDPP_API int hidpp_keymap_id(hidpp_keymap_t *map, uint8_t index);

/** Returns the ID of the control by it's `name`. **/
HIDPP_API int hidpp_keymap_from_name(const char *name);

/** Returns the name of the control with the passed ID. **/
HIDPP_API const char *hidpp_keymap_name(uint16_t feature);

/** Diverts a control to be handled by the event handler. **/
HIDPP_API int hidpp_keymap_divert(hidpp_keymap_t *map, uint16_t id, int value);

/** Remaps control's behaviour to another one's. **/
HIDPP_API int hidpp_keymap_remap(
    hidpp_keymap_t *map, uint16_t id, uint16_t remap
);

/** Returns the last error message of `rcv` or it's devices. **/
const wchar_t *hidpp_error(hidpp_receiver_t *rcv);

/** Enumerate HID++ capable devices using `hid_enumerate`. **/
HIDPP_API size_t hidpp_enumerate(
    unsigned short vid,
    unsigned short pid,
    struct hidpp_receiver_info *out,
    size_t max
);

/** Opens a file descriptor for simulating keyboard input. **/
HIDPP_API int hidpp_input_open(void);

/** Closes the file descriptor for simulating keyboard input. **/
HIDPP_API void hidpp_input_close(int fd);

/** Returns a platform-specific key code for a given `name`. **/
HIDPP_API int hidpp_input_key(const char *name);

/** Simulates key press using specified modifiers. **/
HIDPP_API int hidpp_input_press(int fd, int key, int *mods, size_t count);

/** Simulates key release using specified modifiers. **/
HIDPP_API int hidpp_input_release(int fd, int key, int *mods, size_t count);

#ifdef __cplusplus
}
#endif

#endif
