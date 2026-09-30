/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#ifndef HIDPP_H
#define HIDPP_H

/** Title: Configure HID++ compatible peripherals.

    This C library is a counterpart to the **hidppctl** command-line tool.

    The library provides functions for querying and configuring devices that use
    the proprietary HID++ protocol. Alongside that, it provides functions for
    simulating basic inputs, such as keyboard presses.

    The `hidpp_receiver_t` object refers to a HID++ Bluetooth receiver which
    allows for multiple devices to connect to it. You can interact with these
    devices using `hidpp_device_t` objects.
*/

#ifndef HIDPP_INLINE
    #define HIDPP_INLINE static inline
#endif

#ifndef HIDPP_API
    #define HIDPP_API
#endif

#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Name: HIDPP_VERSION_*
    Individual parts of the version string.
*/
#define HIDPP_VERSION_MAJOR 0
#define HIDPP_VERSION_MINOR 1
#define HIDPP_VERSION_PATCH 0
#define HIDPP_MAKE_VERSION(major, minor, patch)        \
    (((major) * 1000000) + ((minor) * 1000) + (patch))

/** Library version, suitable for numeric feature-detection
    (e.g. #if HIDPP_VERSION >= HIDPP_MAKE_VERSION(1,0,0)).
*/
#define HIDPP_VERSION                                                 \
    HIDPP_MAKE_VERSION(                                               \
        HIDPP_VERSION_MAJOR, HIDPP_VERSION_MINOR, HIDPP_VERSION_PATCH \
    )

/** Library version as a string. */
#define HIDPP_VERSION_STRING "0.1.0"

#define HIDPP_KIND_SHORT 0x10 /* Short report  (7 bytes payload) */
#define HIDPP_KIND_LONG 0x11  /* Long report  (20 bytes payload) */
#define HIDPP_KIND_XLONG 0x12 /* Very-long report (64 bytes payload) */

#define HIDPP_LEN_SHORT 7
#define HIDPP_LEN_LONG 20
#define HIDPP_LEN_XLONG 64

#define HIDPP_MSB(word) ((uint8_t)(((word) >> 8) & 0xFF))
#define HIDPP_LSB(word) ((uint8_t)((word) & 0xFF))
#define HIDPP_WORD(msb, lsb) (((uint16_t)(msb) << 8) | (uint16_t)(lsb))
#define HIDPP_DWORD(a, b, c, d)                                           \
    (((uint32_t)(c) << 24) | ((uint32_t)(c) << 16) | ((uint32_t)(c) << 8) \
     | (uint32_t)(d))

#define HIDPP_BYTE(msn, lsn) (((msn) << 4) | ((lsn) & 0xF))
#define HIDPP_MSN(word) ((uint8_t)(((word) >> 4) & 0xF))
#define HIDPP_LSN(word) ((uint8_t)((word) & 0xF))

/** Error codes returned by the library. */
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
    HIDPP_TOGGLE = -1,
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

/** Buttons usable with hidpp_input_button(). */
enum hidpp_mouse_button {
    HIDPP_BTN_LEFT = 1,
    HIDPP_BTN_RIGHT = 2,
    HIDPP_BTN_MIDDLE = 3,
    HIDPP_BTN_SIDE = 4,
    HIDPP_BTN_EXTRA = 5,
};

typedef char hidpp_bool_t;

typedef struct allocator_t allocator_t;
typedef struct hidpp_receiver_t hidpp_receiver_t;
typedef struct hidpp_device_t hidpp_device_t;
typedef struct hidpp_keymap_t hidpp_keymap_t;
typedef struct hidpp_input_t hidpp_input_t;

/** A single request/response 2.0 protocol packet. */
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

enum hidpp_event_type {
    HIDPP_EVENT_NONE,
    HIDPP_EVENT_UNKNOWN,
    HIDPP_EVENT_BUTTON,
    HIDPP_EVENT_MOUSE,
    HIDPP_EVENT_BATTERY,
    HIDPP_EVENT_WHEEL,
    HIDPP_EVENT_RATCHET,
    HIDPP_EVENT_RATCHET_SWITCH,
    HIDPP_EVENT_TOUCH_PAD_POINTS,
    HIDPP_EVENT_TOUCH_MOUSE_POINTS,
    HIDPP_EVENT_TOUCH_MOUSE_STATUS,
    HIDPP__EVENT_MAX,
};

enum hidpp_battery_level {
    HIDPP_BATTERY_CRITICAL = 1,
    HIDPP_BATTERY_LOW = 2,
    HIDPP_BATTERY_GOOD = 4,
    HIDPP_BATTERY_FULL = 8,
};

enum hidpp_battery_charge {
    HIDPP_BATTERY_DISCHARGING = 0,
    HIDPP_BATTERY_CHARGING = 1,
    HIDPP_BATTERY_CHARGE_COMPLETE = 2,
    HIDPP_BATTERY_CHARGE_ERROR = 3,
};

struct hidpp_event {
    int type;
    union {
        char _size[64];
        void *_alignment;

        hidpp_packet_t unknown;
        uint16_t buttons[4];
        uint16_t mouse[2];

        struct {
            uint8_t chargeState;
            uint8_t batteryLevel;
            uint8_t chargeStatus;
            uint8_t externalPower;
        } battery;

        struct {
            uint8_t flags;
            int16_t delta;
        } wheel;

        uint8_t ratchetSwitch;

        struct {
            int8_t deltaV;
            int8_t deltaH;
        } ratchet;

        struct hidpp_touch_mouse_point {
            uint16_t x;
            uint16_t y;
            uint8_t wx;
            uint8_t wy;
        } touchMousePoints[4];

        struct {
            uint8_t flags;
            uint8_t mouseLifted;
            uint8_t buttonDown;
        } touchMouseStatus;

        struct {
            uint16_t timestamp;
            struct hidpp_touch_pad_point {
                uint8_t type;
                uint8_t status;
                uint16_t x;
                uint16_t y;
                uint8_t force;
                uint8_t area;
                uint8_t flags;
                uint8_t finger;
            } data[2];
        } touchPadPoints;
    } as;
};

struct hidpp_constant {
    uint16_t code;
    const char *name;
};

/** Feature name and code pairs. */
extern const struct hidpp_constant hidpp_constant_features[];
extern const size_t hidpp_constant_features_count;

/** Keymap control name and code pairs. */
extern const struct hidpp_constant hidpp_constant_controls[];
extern const size_t hidpp_constant_controls_count;

/** Returns the library's runtime version string (see HIDPP_VERSION_STRING).
    Useful for shared-library consumers to detect a mismatch against the
    header they compiled with.
*/
HIDPP_API const char *hidpp_version(void);

/** Initialize the underlying hidapi library. */
HIDPP_API int hidpp_init(allocator_t *allocator);

/** Finalize the underlying hidapi library. */
HIDPP_API void hidpp_exit(void);

/** Open a HID++ receiver by Vendor/Product ID. If multiple matching
    receivers are attached, the first enumerated is opened.

    > Use hidpp_open_path() to select a specific interface.
*/
HIDPP_API hidpp_receiver_t *hidpp_open(
    unsigned short vid, unsigned short pid, const wchar_t *serial
);

/** Open a HID++ receiver by platform path. */
HIDPP_API hidpp_receiver_t *hidpp_open_path(const char *path);

/** Open a HID++ receiver by Vendor/Product ID and interface number.
    If multiple matching receivers are found, the first one is opened.
*/
HIDPP_API hidpp_receiver_t *hidpp_open_interface(
    unsigned short vid, unsigned short pid, int interfaceNumber
);

/** Close a HID++ receiver and free its resources. */
HIDPP_API void hidpp_close(hidpp_receiver_t *rcv);

/** Initializes `out` with passed parameters. */
HIDPP_API int hidpp_make(
    hidpp_packet_t *out,
    uint8_t dev,
    uint8_t feat,
    uint8_t func,
    uint8_t *data,
    size_t length
);

/** Send request packet `pkt` to HID++ receiver.
    Errors: EINVAL, EIO.
*/
HIDPP_API int hidpp_send(hidpp_receiver_t *rcv, hidpp_packet_t *pkt);

/** Read one HID++ report, blocking for up to `timeout`.
    Errors: EINVAL, EIO, ETIMEDOUT, EAGAIN.
*/
HIDPP_API int hidpp_receive(hidpp_receiver_t *rcv, hidpp_packet_t *out);

/** Send a packet and receive the matching response. Automatically
    retries on unrelated incoming packets (e.g. HID input reports).
    Errors: ENODATA, `hidpp_send` and `hidpp_receive` errors.
*/
HIDPP_API int hidpp_request(
    hidpp_receiver_t *rcv, hidpp_packet_t *request, hidpp_packet_t *response
);

/** Sets the timeout in milliseconds for IO operations.
    Errors: EINVAL.
*/
HIDPP_API int hidpp_set_timeout(hidpp_receiver_t *rcv, int timeout);

/** Enables (`nonblock` != 0) or disables non-blocking reads on `rcv`.
    With non-blocking reads enabled, hidpp_receive/hidpp_poll/
    hidpp_device_poll return HIDPP_EAGAIN immediately instead of
    blocking for up to the configured timeout when no report is
    available yet -- useful for integrating with an external event
    loop (call hidpp_poll periodically, e.g. on a timer or whenever
    your loop is otherwise idle, instead of dedicating a thread to it).
    hidpp_request() is unaffected: it always waits (up to its own retry
    budget) for a matching response.

    Errors: EINVAL, EIO.
*/
HIDPP_API int hidpp_set_nonblocking(hidpp_receiver_t *rcv, int nonblock);

/** Sets the software id of the HID++ requests.
    Errors: EINVAL.
*/
HIDPP_API int hidpp_set_swid(hidpp_receiver_t *rcv, uint8_t swid);

/** Polls the receiver for available events. Unlike `hidpp_device_poll`,
    this function can only output events of type `HIDPP_EVENT_UNKNOWN`.

    Errors: EINVAL, EIO, `hidpp_receive` errors.
*/
HIDPP_API int hidpp_poll(hidpp_receiver_t *rcv, struct hidpp_event *out);

/** Opens the device at index `device` and returns the connection. */
HIDPP_API hidpp_device_t *hidpp_device_open(
    hidpp_receiver_t *rcv, uint8_t device
);

/** Send `pkt` to HID++ receiver. */
HIDPP_API int hidpp_device_send(hidpp_device_t *dev, hidpp_packet_t *pkt);

/** Read one HID++ report, blocking for up to `timeout`. */
HIDPP_API int hidpp_device_receive(hidpp_device_t *dev, hidpp_packet_t *out);

/** Send a packet and receive the matching response. Automatically
    retries on unrelated incoming packets (e.g. HID input reports).
*/
HIDPP_API int hidpp_device_request(
    hidpp_device_t *dev, hidpp_packet_t *request, hidpp_packet_t *response
);

/** Polls the device for available events. */
HIDPP_API int hidpp_device_poll(hidpp_device_t *dev, struct hidpp_event *out);

/** Returns a 64-bit id number that can be used to uniquely identify a device
    for caching purposes. This function circumvents all other caching mechanisms
    by directly requesting necessary data to build a cache id.

    > A cache id is built by concatenating a list of bytes that
    > represent firmware's prefix, version and build number.
*/
HIDPP_API uint64_t hidpp_cache_id(hidpp_device_t *dev);

/** Clears cached device information. This includes:

    - Feature information (ids, indexes, flags...)
    - Keymap information (controls, states, flags...)

    Use this function periodically for long running programs or if connecting
    and disconnecting devices.
*/
HIDPP_API void hidpp_clear_cache(hidpp_device_t *dev);

/** Closes the `device` from the receiver. */
HIDPP_API void hidpp_device_close(hidpp_device_t *dev);

/** Reads receiver information to `out`.
    Errors: EINVAL, EIO.
*/
HIDPP_API int hidpp_receiver_info(
    hidpp_receiver_t *rcv, struct hidpp_receiver_info *out
);

/** Frees `struct hidpp_receiver_info` objects. */
HIDPP_API void hidpp_free_info(struct hidpp_receiver_info *info);

/** Reads device information to `out`.
    Errors: EINVAL, EIO.
*/
HIDPP_API int hidpp_device_info(
    hidpp_device_t *dev, struct hidpp_device_info *out
);

/** Pings the device with `data`.
    Errors: EINVAL, EIO.
*/
HIDPP_API int hidpp_ping(hidpp_device_t *dev, uint8_t data);

/** Resolve a feature ID to its index on the device. */
HIDPP_API int hidpp_feature_index(hidpp_device_t *dev, uint16_t feature);

/** Resolve a feature index to its ID on the device. */
HIDPP_API int hidpp_feature_id(hidpp_device_t *dev, uint8_t index);

/** Copies up to `max` of the device's feature IDs into `out`, in index
    order (index 0 is the Root feature, so `out[0]` is always 0x0000).
    Returns the number of entries written.
*/
HIDPP_API size_t
hidpp_feature_list(hidpp_device_t *dev, uint16_t *out, size_t max);

/** Returns the name of the feature with the passed ID. */
HIDPP_API const char *hidpp_feature_name(uint16_t feature);

/** HID++ 2.0 feature information. */
struct hidpp_feature_info {
    const char *name;
    uint16_t id;
    uint8_t index;
    uint8_t version;
    uint8_t flags;
    hidpp_bool_t isObsolete;
    hidpp_bool_t isHidden;
    hidpp_bool_t initialized;
};

/** Returns information about the feature with given `id`. */
HIDPP_API struct hidpp_feature_info *hidpp_feature_info(
    hidpp_device_t *dev, uint16_t featId
);

/* Firmware entity type. */
enum hidpp_firmware_type {
    HIDPP_FIRMWARE_MAIN_APP = 0,
    HIDPP_FIRMWARE_BOOT_LOADER = 1,
    HIDPP_FIRMWARE_HARDWARE = 2,
};

/* Firmware entity information. */
struct hidpp_firmware_entity {
    uint8_t id;
    uint8_t type;
    uint32_t prefix;
    uint16_t version;
    uint16_t buildNumber;
    uint8_t reserved;
    uint8_t specificInfo[9];

    const char *typeName;
    hidpp_bool_t initialized;
};

/** Queries the device for firmware information for given entity id. */
HIDPP_API struct hidpp_firmware_entity *hidpp_firmware_entity(
    hidpp_device_t *dev, uint8_t id, uint8_t *count
);

/** Returns the object for configuring device's keybindings. */
HIDPP_API hidpp_keymap_t *hidpp_keymap(hidpp_device_t *dev);

/** Resolve a control ID to its index on the device. */
HIDPP_API int hidpp_keymap_index(hidpp_keymap_t *map, uint16_t control);

/** Resolve a keymap index to its ID on the device. */
HIDPP_API int hidpp_keymap_id(hidpp_keymap_t *map, uint8_t index);

/** Returns the ID of the control by it's `name`. */
HIDPP_API int hidpp_keymap_from_name(const char *name);

/** Returns the name of the control with the passed ID. */
HIDPP_API const char *hidpp_keymap_name(uint16_t feature);

/** Copies up to `max` of the keymap's control IDs into `out`.
    Returns the number of entries written.
*/
HIDPP_API size_t
hidpp_keymap_list(hidpp_keymap_t *map, uint16_t *out, size_t max);

/** Diverts a control to be handled by the event handler. */
HIDPP_API int hidpp_keymap_divert(hidpp_keymap_t *map, uint16_t id, int value);

/** Remaps control's behaviour to another one's. */
HIDPP_API int hidpp_keymap_remap(
    hidpp_keymap_t *map, uint16_t id, uint16_t remap
);

/** Information about an individual control (button or key). */
struct hidpp_keymap_info {
    uint16_t id;
    uint8_t index;
    uint16_t taskId;
    uint8_t flags;
    uint8_t position;
    uint8_t group;
    uint8_t groupMask;
    uint8_t rawXY;

    uint8_t reportFlags;
    uint16_t remapId;

    hidpp_bool_t isVirtual;
    hidpp_bool_t isPersistable;
    hidpp_bool_t isDivertable;
    hidpp_bool_t isReprogrammable;
    hidpp_bool_t isFnTogglable;
    hidpp_bool_t isHotkey;
    hidpp_bool_t isFunctionKey;
    hidpp_bool_t isMouseButton;

    hidpp_bool_t initialized;
    const char *name;
    const char *remapName;
};

/** Reads device's keymap information about the given control `id`. */
HIDPP_API struct hidpp_keymap_info *hidpp_keymap_info(
    hidpp_keymap_t *map, uint16_t id
);

/** Information about control's current state. */
struct hidpp_keymap_state {
    uint16_t remapId;
    uint8_t flags;
    hidpp_bool_t rawXY;
    hidpp_bool_t isPersistent;
    hidpp_bool_t isDiverted;
};

/** Reads current state of the control with `id`. */
HIDPP_API int hidpp_keymap_state(
    hidpp_keymap_t *map, uint16_t id, struct hidpp_keymap_state *out
);

/** Returns the last error message of `rcv` or it's devices. */
HIDPP_API const wchar_t *hidpp_error(hidpp_receiver_t *rcv);

/** Returns a human-readable string for a `hidpp_error` code (e.g.
    HIDPP_EIO -> "I/O error"). Unlike hidpp_error(), this does not
    depend on a receiver and never returns NULL.
*/
HIDPP_API const char *hidpp_error_str(int code);

/** Enumerate HID++ capable devices using `hid_enumerate`. */
HIDPP_API size_t hidpp_enumerate(
    unsigned short vid,
    unsigned short pid,
    struct hidpp_receiver_info *out,
    size_t max
);

/** Creates a handle for simulating keyboard/mouse input. */
HIDPP_API hidpp_input_t *hidpp_input_new(allocator_t *allocator);

/** Frees the input object for simulating keyboard input. */
HIDPP_API void hidpp_input_free(hidpp_input_t *input);

/** Returns a platform-specific key code for a given `name`. */
HIDPP_API int hidpp_input_key(const char *name);

/** Simulates key press using specified modifiers. */
HIDPP_API int hidpp_input_press(
    hidpp_input_t *input, int key, int *mods, size_t count
);

/** Simulates key release using specified modifiers. */
HIDPP_API int hidpp_input_release(
    hidpp_input_t *input, int key, int *mods, size_t count
);

/** Simulates key press or release using specified modifiers. */
HIDPP_API int hidpp_input_set(
    hidpp_input_t *input, int key, int *mods, size_t count, int value
);

/** Types out `text` as a sequence of key presses/releases, applying
    shift automatically for uppercase letters and shifted punctuation.
    Only covers the printable US-layout ASCII range plus '\t' and '\n';
    unmappable characters are skipped. Returns HIDPP_OK, or a negative
    hidpp_error if `fd` is invalid.
*/
HIDPP_API int hidpp_input_type(hidpp_input_t *input, const char *text);

/** Simulates a relative mouse movement of (`dx`, `dy`) pixels/counts. */
HIDPP_API int hidpp_input_move(hidpp_input_t *input, int dx, int dy);

/** Simulates a press (`value` != 0) or release (`value` == 0) of a
    mouse button (see enum hidpp_mouse_button).
*/
HIDPP_API int hidpp_input_button(hidpp_input_t *input, int button, int value);

/** Simulates scroll wheel movement. `dy` is vertical scroll (positive is
    up), `dx` is horizontal scroll (positive is right); either may be 0.
*/
HIDPP_API int hidpp_input_scroll(hidpp_input_t *input, int dx, int dy);

#ifdef __cplusplus
}
#endif

#endif
