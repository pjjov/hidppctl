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
    (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | ((uint32_t)(c) << 8) \
     | (uint32_t)(d))

#define HIDPP_QWORD(a, b, c, d, e, f, g, h)                                  \
    (((uint64_t)(a) << 56) | ((uint64_t)(b) << 48) | ((uint64_t)(c) << 40)   \
     | ((uint64_t)(d) << 32) | ((uint64_t)(e) << 24) | ((uint64_t)(f) << 16) \
     | ((uint64_t)(g) << 8) | (uint64_t)(h))

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

/** Error codes of HID++ error reports (see hidpp_error_code()). */
enum hidpp_protocol_error {
    HIDPP_ERR_NONE = 0,
    HIDPP_ERR_UNKNOWN = 1,
    HIDPP_ERR_INVALID_ARGUMENT = 2,
    HIDPP_ERR_OUT_OF_RANGE = 3,
    HIDPP_ERR_HW_ERROR = 4,
    HIDPP_ERR_INTERNAL = 5,
    HIDPP_ERR_INVALID_FEATURE_INDEX = 6,
    HIDPP_ERR_INVALID_FUNCTION_ID = 7,
    HIDPP_ERR_BUSY = 8,
    HIDPP_ERR_UNSUPPORTED = 9,
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
    HIDPP_TYPE_HEADSET,
    HIDPP_TYPE_WEBCAM,
    HIDPP_TYPE_STEERING_WHEEL,
    HIDPP_TYPE_JOYSTICK,
    HIDPP_TYPE_GAMEPAD,
    HIDPP_TYPE_DOCK,
    HIDPP_TYPE_SPEAKER,
    HIDPP_TYPE_MICROPHONE,
    HIDPP_TYPE_ILLUMINATION_LIGHT,
    HIDPP_TYPE_PROGRAMMABLE_CONTROLLER,
    HIDPP_TYPE_CAR_SIM_PEDALS,
    HIDPP_TYPE_ADAPTER,
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
    HIDPP_EVENT_BACKLIGHT,
    HIDPP_EVENT_ILLUMINATION,
    HIDPP__EVENT_MAX,
};

/** Kinds of `HIDPP_EVENT_ILLUMINATION` events (see `as.illumination.kind`). */
enum hidpp_illumination_event {
    HIDPP_ILLUMINATION_EVENT_STATE,             /* `state` is valid */
    HIDPP_ILLUMINATION_EVENT_BRIGHTNESS,        /* `value` is valid */
    HIDPP_ILLUMINATION_EVENT_COLOR_TEMPERATURE, /* `value` is valid */
    HIDPP_ILLUMINATION_EVENT_BRIGHTNESS_MAX,    /* `value` is valid */
    HIDPP_ILLUMINATION_EVENT_CLAMPED, /* `source` and `value` is valid */
};

enum hidpp_battery_charge {
    HIDPP_BATTERY_DISCHARGING = 0,
    HIDPP_BATTERY_RECHARGING = 1,
    HIDPP_BATTERY_CHARGE_FINAL = 2,
    HIDPP_BATTERY_CHARGE_COMPLETE = 3,
    HIDPP_BATTERY_SUBOPTIMAL_RECHARGING = 4,
    HIDPP_BATTERY_INVALID_TYPE = 5,
    HIDPP_BATTERY_THREMAL_ERROR = 6,
    HIDPP_BATTERY_CHARGE_ERROR = 7,
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
            uint8_t levelCount;
            uint8_t level;
            uint8_t status; /* enum hidpp_backlight_status */
            uint8_t effect; /* enum hidpp_backlight_effect */
        } backlight;

        struct {
            uint8_t kind;   /* enum hidpp_illumination_event */
            uint8_t state;  /* 0 = off, 1 = on */
            uint8_t source; /* 0 = unknown, 1 = HID++, 2 = button */
            uint16_t value;
        } illumination;

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

/** Opens a HID++ receiver which communicates using a socket. */
HIDPP_API hidpp_receiver_t *hidpp_open_socket(int fd);

/** Callback used for a custom receiver sending mechanism. */
typedef int(hidpp_receiver_send_fn)(
    hidpp_receiver_t *rcv, const char *buffer, size_t size, void *user
);

/** Callback used for a custom receiver receiving mechanism. */
typedef int(hidpp_receiver_recv_fn)(
    hidpp_receiver_t *rcv, char *buffer, size_t size, void *user
);

/** Opens a HID++ receiver which communicates using custom callbacks. */
HIDPP_API hidpp_receiver_t *hidpp_open_custom(
    hidpp_receiver_send_fn *send, hidpp_receiver_recv_fn *recv, void *user
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

/** Prevents packet sending for hooks. */
#define HIDPP_HOOK_SKIP_IO -192

/** Callback which handles packets before they are sent and after they have been
    received from the receiver. For bidirectional requests, both the request and
    response packet will be provided after the exchange.

    Returning `HIDPP_HOOK_SKIP_IO` from the hook will prevent the packet from
    reaching the receiver.
*/
typedef int(hidpp_protocol_hook_fn)(
    hidpp_receiver_t *rcv, hidpp_packet_t *req, hidpp_packet_t *res, void *user
);

/** Sets the protocol hook which is called on each exchange.
    Returns: previously set protocol hook or `NULL`.
*/
HIDPP_API hidpp_protocol_hook_fn *hidpp_set_protocol_hook(
    hidpp_receiver_t *rcv, hidpp_protocol_hook_fn *hook, void *user
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

/** Queries the device type. */
HIDPP_API int hidpp_device_type(hidpp_device_t *dev);

/** Queries the device name. */
HIDPP_API const char *hidpp_device_name(hidpp_device_t *dev);

/** HID++ device information */
struct hidpp_device_info {
    uint8_t major;
    uint8_t minor;
    uint8_t index;

    uint8_t type;
    const char *name;
    const char *nickname;
    const char *serialNumber;
};

/** Reads device information to `out`.
    Errors: EINVAL, EIO.
*/
HIDPP_API int hidpp_device_info(
    hidpp_device_t *dev, struct hidpp_device_info *out
);

/** Closes the `device` from the receiver. */
HIDPP_API void hidpp_device_close(hidpp_device_t *dev);

/** Returns a 64-bit id number that can be used to uniquely identify a device
    for caching purposes. This function circumvents all other caching mechanisms
    by directly requesting necessary data to build a cache id.

    > A cache id is built by concatenating a list of bytes that
    > represent firmware's prefix, version and build number.
*/
HIDPP_API uint64_t hidpp_cache_id(hidpp_device_t *dev);

/** Attempts to cache data for all features of the given device. */
HIDPP_API void hidpp_cache_collect(hidpp_device_t *dev);

/** Saves current cache content into the given buffer and reports it's size.
    Parameter 'size' should contain the maximum buffer size at call time.
    Errors: EINVAL, ENOMEM.
*/
HIDPP_API int hidpp_cache_save(hidpp_device_t *dev, void *buffer, size_t *size);

/** Loads previously saved cache buffer for given device.
    Errors: EINVAL, ENOMEM, ENOSYS.
*/
HIDPP_API int hidpp_cache_load(
    hidpp_device_t *dev, const void *buffer, size_t size
);

/** Clears cached device information. This includes:

    - Feature information (ids, indexes, flags...)
    - Keymap information (controls, states, flags...)

    Use this function periodically for long running programs or if connecting
    and disconnecting devices.
*/
HIDPP_API void hidpp_cache_clear(hidpp_device_t *dev);

/** Reads receiver information to `out`.
    Errors: EINVAL, EIO.
*/
HIDPP_API int hidpp_receiver_info(
    hidpp_receiver_t *rcv, struct hidpp_receiver_info *out
);

/** Frees `struct hidpp_receiver_info` objects. */
HIDPP_API void hidpp_free_info(struct hidpp_receiver_info *info);

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
    hidpp_bool_t isEngineering;
    hidpp_bool_t isManufacturingDeactivatable;
    hidpp_bool_t isComplianceDeactivatable;
    hidpp_bool_t initialized;
};

/** Returns information about the feature with given `id`. */
HIDPP_API struct hidpp_feature_info *hidpp_feature_info(
    hidpp_device_t *dev, uint16_t featId
);

/** Returns the nickname stored on the device itself. */
HIDPP_API const char *hidpp_nickname_get(hidpp_device_t *dev);

/** Sets the device's nickname (stored on device itself). */
HIDPP_API int hidpp_nickname_set(hidpp_device_t *dev, const char *nickname);

/** Resets the device's nickname to the default one. */
HIDPP_API int hidpp_nickname_reset(hidpp_device_t *dev);

/** Returns the device's default nickname. */
HIDPP_API const char *hidpp_nickname_default(hidpp_device_t *dev);

/** Queries device's firmware for a serial number. */
HIDPP_API const char *hidpp_serial_number(hidpp_device_t *dev);

/** Firmware information */
struct hidpp_firmware_info {
    uint8_t entityCount;
    uint32_t unitId;
    uint16_t transportFlags;
    uint64_t modelId;
    uint8_t extendedModelId; /* device color */
    uint8_t capabilities;
    uint8_t reserved;

    hidpp_bool_t hasSerialNumber;
    hidpp_bool_t supportsUSB;
    hidpp_bool_t supportsEQuad;
    hidpp_bool_t supportsBTLE; /* Bluetooth low energy */
    hidpp_bool_t supportsBT;   /* Bluetooth */
    hidpp_bool_t initialized;
};

/** Queries the device for firmware information. */
HIDPP_API struct hidpp_firmware_info *hidpp_firmware_info(hidpp_device_t *dev);

/* Firmware entity type. */
enum hidpp_firmware_type {
    HIDPP_FIRMWARE_MAIN_APP = 0,
    HIDPP_FIRMWARE_BOOT_LOADER = 1,
    HIDPP_FIRMWARE_HARDWARE = 2,
    HIDPP_FIRMWARE_TOUCHPAD = 3,
    HIDPP_FIRMWARE_OPTICAL_SENSOR = 4,
    HIDPP_FIRMWARE_SOFTDEVICE = 5,
    HIDPP_FIRMWARE_RF_COMPANION_MCU = 6,
    HIDPP_FIRMWARE_FACTORY_APPLICATION = 7,
    HIDPP_FIRMWARE_RGB_CUSTOM_EFFECT = 8,
    HIDPP_FIRMWARE_MOTOR_DRIVE = 9,
};

/* Firmware entity information. */
struct hidpp_firmware_entity {
    uint8_t id;
    uint8_t type;
    uint32_t firmwareNumber;
    uint8_t revision;
    uint16_t buildNumber;
    uint8_t reserved;
    uint8_t specificInfo[9];

    const char *typeName;
    hidpp_bool_t initialized;
};

/** Queries the device for firmware information for given entity id. */
HIDPP_API struct hidpp_firmware_entity *hidpp_firmware_entity(
    hidpp_device_t *dev, uint8_t id
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

/** HID++ Battery capabilities. */
struct hidpp_battery_info {
    uint8_t levelCount;
    uint8_t flags;
    uint16_t nominalLife;
    uint8_t criticalLevel;

    hidpp_bool_t disabledOSD;
    hidpp_bool_t enabledMileage;
    hidpp_bool_t isRechargable;
    hidpp_bool_t initialized;
};

/** Battery discharge status. */
struct hidpp_battery_status {
    uint8_t dischargeLevel;
    uint8_t dischargeNextLevel;
    uint8_t status;

    const char *statusName;
    const char *levelName;
};

/** Returns device's battery capabilities. */
HIDPP_API struct hidpp_battery_info *hidpp_battery_info(hidpp_device_t *dev);

/** Queries the device for it's current discharge level. */
HIDPP_API struct hidpp_battery_status *hidpp_battery_status(
    hidpp_device_t *dev
);

/** Selects the host the device is currently connected to. Usable wherever a
    host index is expected by the host, backlight and action functions.
*/
#define HIDPP_HOST_CURRENT 0xFF

/** Bus (transport) a host is paired through. */
enum hidpp_host_bus {
    HIDPP_HOST_BUS_NONE = 0, /* unused slot */
    HIDPP_HOST_BUS_EQUAD = 1,
    HIDPP_HOST_BUS_USB = 2,
    HIDPP_HOST_BUS_BT = 3,
    HIDPP_HOST_BUS_BLE = 4,
    HIDPP_HOST_BUS_BLE_PRO = 5, /* Bolt */
};

/** Multi-host capabilities (features 0x1814 and 0x1815). */
struct hidpp_host_info {
    uint8_t count;         /* number of host slots, paired or not */
    uint16_t capabilities; /* raw 0x1815 capability mask, 0 without 0x1815 */

    hidpp_bool_t canSwitch;   /* 0x1814: hidpp_host_switch() works */
    hidpp_bool_t hasHostInfo; /* 0x1815: per-host information is available */
    hidpp_bool_t canGetName;
    hidpp_bool_t canSetName;
    hidpp_bool_t canMove;
    hidpp_bool_t canDelete;
    hidpp_bool_t hasOsVersion;
    hidpp_bool_t hasEQuadDescriptor;
    hidpp_bool_t hasUSBDescriptor;
    hidpp_bool_t hasBTDescriptor;
    hidpp_bool_t hasBLEDescriptor;
    hidpp_bool_t initialized;
};

/** State of a single host slot. */
struct hidpp_host_status {
    uint8_t index;
    uint8_t status;  /* 0 = empty slot, 1 = paired */
    uint8_t busType; /* enum hidpp_host_bus */
    uint8_t numPages;
    uint8_t nameLength;
    uint8_t nameMaxLength;

    hidpp_bool_t isPaired;
    const char *busName;
};

/** Host operating system, as stored by software (the firmware never reads
    it). All fields are zero if nothing has been stored.
*/
struct hidpp_host_os {
    uint8_t type;
    uint8_t version;
    uint16_t revision;
    uint16_t build;
};

/** One page of the host descriptor. `data` is the raw payload that follows
    the (host, bus) header and its layout depends on `busType`.
*/
struct hidpp_host_descriptor {
    uint8_t index;
    uint8_t busType; /* enum hidpp_host_bus */
    uint8_t data[14];
};

/** Returns the multi-host capabilities of the device.
    Errors (as NULL): device has neither of the host features.
*/
HIDPP_API struct hidpp_host_info *hidpp_host_info(hidpp_device_t *dev);

/** Returns the index of the host the device is connected to, or an error.
    This is never cached, the device can be switched by its user.
    Errors: EINVAL, ENOSYS, EIO.
*/
HIDPP_API int hidpp_host_current(hidpp_device_t *dev);

/** Switches the device to another host. The device is most probably going to
    disconnect from this host and no answer is awaited: success only means
    that the request was sent.
    Errors: EINVAL (no such host), ENOSYS, EIO.
*/
HIDPP_API int hidpp_host_switch(hidpp_device_t *dev, uint8_t host);

/** Reads the status of `host` (or HIDPP_HOST_CURRENT).
    Errors: EINVAL, ENOSYS, EIO.
*/
HIDPP_API int hidpp_host_status(
    hidpp_device_t *dev, uint8_t host, struct hidpp_host_status *out
);

/** Copies the friendly name of `host` into `out` (always null terminated,
    truncated to `size - 1` bytes). Returns the number of bytes written,
    excluding the terminator. Unpaired hosts have an empty name.
    Errors: EINVAL, ENOSYS, EIO.
*/
HIDPP_API int hidpp_host_name(
    hidpp_device_t *dev, uint8_t host, char *out, size_t size
);

/** Sets the friendly name of `host`. The name must be printable ASCII and
    not longer than the `nameMaxLength` of the host.
    Errors: EINVAL, ENOMEM (too long), ENOSYS, EIO.
*/
HIDPP_API int hidpp_host_set_name(
    hidpp_device_t *dev, uint8_t host, const char *name
);

/** Moves a host to a different slot. Other hosts may shift up or down.
    Errors: EINVAL, ENOSYS, EIO.
*/
HIDPP_API int hidpp_host_move(
    hidpp_device_t *dev, uint8_t host, uint8_t newIndex
);

/** Unpairs `host`, which makes its slot available for new pairing.
    Errors: EINVAL, ENOSYS, EIO.
*/
HIDPP_API int hidpp_host_delete(hidpp_device_t *dev, uint8_t host);

/** Reads the operating system stored for `host`.
    Errors: EINVAL, ENOSYS, EIO.
*/
HIDPP_API int hidpp_host_os(
    hidpp_device_t *dev, uint8_t host, struct hidpp_host_os *out
);

/** Stores the operating system of `host`.
    Errors: EINVAL, ENOSYS, EIO.
*/
HIDPP_API int hidpp_host_set_os(
    hidpp_device_t *dev, uint8_t host, const struct hidpp_host_os *os
);

/** Reads a page (0 .. numPages - 1) of the descriptor of `host`.
    Errors: EINVAL, ENOSYS, EIO.
*/
HIDPP_API int hidpp_host_descriptor(
    hidpp_device_t *dev,
    uint8_t host,
    uint8_t page,
    struct hidpp_host_descriptor *out
);

/** Returns the cookie (a persistent byte of software defined meaning) of
    `host`, or an error. Cookies are cleared by the firmware when a new
    host is paired.
    Errors: EINVAL, ENOSYS, EIO.
*/
HIDPP_API int hidpp_host_cookie(hidpp_device_t *dev, uint8_t host);

/** Writes the cookie of `host`.
    Errors: EINVAL, ENOSYS, EIO.
*/
HIDPP_API int hidpp_host_set_cookie(
    hidpp_device_t *dev, uint8_t host, uint8_t cookie
);

/** Backlight modes selectable by software (feature 0x1982 v3). */
enum hidpp_backlight_mode {
    HIDPP_BACKLIGHT_MODE_NONE = 0,
    HIDPP_BACKLIGHT_MODE_AUTO = 1,
    HIDPP_BACKLIGHT_MODE_TEMP_MANUAL = 2,
    HIDPP_BACKLIGHT_MODE_PERM_MANUAL = 3,
};

/** Backlight effects. The value is the bit index in the effect list. */
enum hidpp_backlight_effect {
    HIDPP_BACKLIGHT_EFFECT_STATIC = 0,
    HIDPP_BACKLIGHT_EFFECT_NONE = 1,
    HIDPP_BACKLIGHT_EFFECT_BREATHING = 2,
    HIDPP_BACKLIGHT_EFFECT_CONTRAST = 3,
    HIDPP_BACKLIGHT_EFFECT_REACTION = 4,
    HIDPP_BACKLIGHT_EFFECT_RANDOM = 5,
    HIDPP_BACKLIGHT_EFFECT_WAVES = 6,
};

/** Backlight status reported by the device. */
enum hidpp_backlight_status {
    HIDPP_BACKLIGHT_DISABLED_BY_SW = 0,
    HIDPP_BACKLIGHT_DISABLED_BY_BATTERY = 1,
    HIDPP_BACKLIGHT_ALS = 2,
    HIDPP_BACKLIGHT_ALS_SATURATED = 3,
    HIDPP_BACKLIGHT_MANUAL_TEMPORARY = 4,
    HIDPP_BACKLIGHT_MANUAL_PERMANENT = 5,
};

/** Backlight capabilities. They do not change at runtime. */
struct hidpp_backlight_info {
    uint8_t version;     /* version of the feature on the device */
    uint8_t levelCount;  /* number of brightness levels (nbLevels) */
    uint16_t effectList; /* bit mask, bit N set if effect N is available */

    hidpp_bool_t hasEffects;   /* version >= 2 */
    hidpp_bool_t hasDurations; /* version >= 3: modes, level and durations */
    hidpp_bool_t supportsWow;
    hidpp_bool_t supportsCrown;
    hidpp_bool_t supportsPowerSave;
    hidpp_bool_t supportsAutoMode;
    hidpp_bool_t supportsTempManualMode;
    hidpp_bool_t supportsPermManualMode;

    /** Out-of-box durations in seconds (version >= 3). */
    uint16_t defaultDurationHandsOut;
    uint16_t defaultDurationHandsIn;
    uint16_t defaultDurationPowered;
    hidpp_bool_t initialized;
};

/** Backlight configuration, stored persistently on the device. Durations are
    in seconds, from 5 seconds to 2 hours, in steps of 5 seconds.
*/
struct hidpp_backlight_config {
    hidpp_bool_t enabled;
    hidpp_bool_t wow;       /* effect at power on */
    hidpp_bool_t crown;     /* effect when the crown is touched */
    hidpp_bool_t powerSave; /* disable at critical battery level */
    uint8_t mode;           /* enum hidpp_backlight_mode */
    uint8_t effect;         /* enum hidpp_backlight_effect */
    uint8_t level;          /* level used in the permanent manual mode */
    uint16_t durationHandsOut;
    uint16_t durationHandsIn;
    uint16_t durationPowered;
};

/** What the backlight is doing right now. */
struct hidpp_backlight_state {
    uint8_t levelCount;
    uint8_t level;
    uint8_t status; /* enum hidpp_backlight_status */
    uint8_t effect; /* enum hidpp_backlight_effect */

    const char *statusName;
    const char *effectName;
};

/** Returns the backlight capabilities.
    Errors (as NULL): the device has no supported backlight feature.
*/
HIDPP_API struct hidpp_backlight_info *hidpp_backlight_info(
    hidpp_device_t *dev
);

/** Reads the stored backlight configuration.
    Errors: EINVAL, ENOSYS, EIO.
*/
HIDPP_API int hidpp_backlight_get(
    hidpp_device_t *dev, struct hidpp_backlight_config *out
);

/** Writes the whole backlight configuration, which is meant to be modified
    after hidpp_backlight_get(). Options the device does not support are
    rejected.
    Errors: EINVAL, ENOSYS, EIO.
*/
HIDPP_API int hidpp_backlight_set(
    hidpp_device_t *dev, const struct hidpp_backlight_config *config
);

/** Queries what the backlight is currently doing.
    Errors: EINVAL, ENOSYS, EIO.
*/
HIDPP_API int hidpp_backlight_state(
    hidpp_device_t *dev, struct hidpp_backlight_state *out
);

/** Applies a backlight effect until the next power cycle (it is not stored).
    Errors: EINVAL, ENOSYS, EIO.
*/
HIDPP_API int hidpp_backlight_set_effect(hidpp_device_t *dev, uint8_t effect);

/** Returns the name of a backlight effect. */
HIDPP_API const char *hidpp_backlight_effect_name(uint8_t effect);

/** Individually controllable properties of an illumination device. */
enum hidpp_illumination_control {
    HIDPP_ILLUMINATION_BRIGHTNESS = 0,        /* lumen */
    HIDPP_ILLUMINATION_COLOR_TEMPERATURE = 1, /* kelvin */
};

/** Capabilities of a single control. They do not change at runtime. */
struct hidpp_illumination_info {
    uint16_t min;
    uint16_t max;
    uint16_t res; /* step size; 0 if only a single value is supported */
    uint8_t maxLevels;
    uint8_t flags;

    hidpp_bool_t hasEvents;
    hidpp_bool_t hasLinearLevels;
    hidpp_bool_t hasNonLinearLevels;
    hidpp_bool_t hasDynamicMaximum; /* brightness only, version >= 1 */
    hidpp_bool_t isSettable;        /* min != max */
    hidpp_bool_t initialized;
};

#define HIDPP_ILLUMINATION_MAX_LEVELS 15

/** User-selectable values of a control. */
struct hidpp_illumination_levels {
    hidpp_bool_t linear;
    uint16_t min;  /* linear: value of the first level */
    uint16_t max;  /* linear: value of the last level */
    uint16_t step; /* linear: difference between levels */
    uint8_t count; /* number of levels */
    uint16_t values[HIDPP_ILLUMINATION_MAX_LEVELS]; /* non-linear levels */
};

/** Returns the capabilities of `control`.
    Errors (as NULL): no illumination feature or invalid control.
*/
HIDPP_API struct hidpp_illumination_info *hidpp_illumination_info(
    hidpp_device_t *dev, int control
);

/** Returns 0 (off), 1 (on) or an error. */
HIDPP_API int hidpp_illumination_get(hidpp_device_t *dev);

/** Turns the light on (`state` != 0) or off.
    Errors: EINVAL, ENOSYS, EIO.
*/
HIDPP_API int hidpp_illumination_set(hidpp_device_t *dev, int state);

/** Returns the current value of `control` or an error. */
HIDPP_API int hidpp_illumination_value(hidpp_device_t *dev, int control);

/** Sets the value of `control`. The value must be within the range of the
    control and a multiple of its resolution.
    Errors: EINVAL, ENOSYS, EIO.
*/
HIDPP_API int hidpp_illumination_set_value(
    hidpp_device_t *dev, int control, uint16_t value
);

/** Reads the levels of `control`.
    Errors: EINVAL, ENOSYS, EIO.
*/
HIDPP_API int hidpp_illumination_levels(
    hidpp_device_t *dev, int control, struct hidpp_illumination_levels *out
);

/** Replaces the levels of `control`.
    Errors: EINVAL, ENOSYS, EIO.
*/
HIDPP_API int hidpp_illumination_set_levels(
    hidpp_device_t *dev,
    int control,
    const struct hidpp_illumination_levels *levels
);

/** Resets the levels of `control` to factory defaults.
    Errors: EINVAL, ENOSYS, EIO.
*/
HIDPP_API int hidpp_illumination_reset_levels(hidpp_device_t *dev, int control);

/** Returns the effective maximum brightness, which is 0 if none is in effect.
    The device can answer with a "busy" error while it is still testing
    itself, which is reported as EAGAIN; try again later.
    Errors: EINVAL, ENOSYS, EAGAIN, EIO.
*/
HIDPP_API int hidpp_illumination_effective_max(hidpp_device_t *dev);

/** Kind of the action performed when a control is triggered. */
enum hidpp_action_type {
    HIDPP_ACTION_NONE = 0, /* no output */
    HIDPP_ACTION_KEY = 1,
    HIDPP_ACTION_MOUSE_BUTTON = 2,      /* value is a bit map of buttons */
    HIDPP_ACTION_X_DISPLACEMENT = 3,    /* value is signed */
    HIDPP_ACTION_Y_DISPLACEMENT = 4,    /* value is signed */
    HIDPP_ACTION_VERTICAL_ROLLER = 5,   /* value is signed */
    HIDPP_ACTION_HORIZONTAL_ROLLER = 6, /* value is signed */
    HIDPP_ACTION_CONSUMER = 7,
    HIDPP_ACTION_INTERNAL = 8, /* value is the index of a device function */
    HIDPP_ACTION_POWER = 9,
};

/** Bits of the modifier mask of a key action (same as HID boot keyboards). */
enum hidpp_action_modifier {
    HIDPP_MOD_LEFT_CTRL = 0x01,
    HIDPP_MOD_LEFT_SHIFT = 0x02,
    HIDPP_MOD_LEFT_ALT = 0x04,
    HIDPP_MOD_LEFT_META = 0x08,
    HIDPP_MOD_RIGHT_CTRL = 0x10,
    HIDPP_MOD_RIGHT_SHIFT = 0x20,
    HIDPP_MOD_RIGHT_ALT = 0x40,
    HIDPP_MOD_RIGHT_META = 0x80,
};

/** What persistent actions the device is able to perform. */
struct hidpp_action_info {
    uint16_t flags;
    uint8_t count;     /* number of controls */
    uint8_t hostCount; /* 0xFF if the device has not said */

    hidpp_bool_t canKey;
    hidpp_bool_t canMouseButton;
    hidpp_bool_t canXDisplacement;
    hidpp_bool_t canYDisplacement;
    hidpp_bool_t canVerticalRoller;
    hidpp_bool_t canHorizontalRoller;
    hidpp_bool_t canConsumer;
    hidpp_bool_t canInternal;
    hidpp_bool_t canPower;
    hidpp_bool_t initialized;
};

/** Persistent action performed by a control on a host. */
struct hidpp_action {
    uint16_t id;
    uint8_t host;
    uint8_t type; /* enum hidpp_action_type */
    uint16_t value;
    uint8_t modifiers; /* bit mask of enum hidpp_action_modifier */
    uint8_t status;

    hidpp_bool_t isRemapped; /* false if the factory default is used */
    const char *name;        /* name of the control */
    const char *typeName;
};

/** Returns what actions the device can perform.
    Errors (as NULL): the device has no persistent action feature.
*/
HIDPP_API struct hidpp_action_info *hidpp_action_info(hidpp_device_t *dev);

/** Returns the control ID at the given table index or an error. */
HIDPP_API int hidpp_action_id(hidpp_device_t *dev, uint8_t index);

/** Copies up to `max` control IDs into `out` and returns their count. */
HIDPP_API size_t
hidpp_action_list(hidpp_device_t *dev, uint16_t *out, size_t max);

/** Reads the action stored for control `id` on `host`.
    Errors: EINVAL, ENOENT (no such control), ENOSYS, EIO.
*/
HIDPP_API int hidpp_action_get(
    hidpp_device_t *dev, uint16_t id, uint8_t host, struct hidpp_action *out
);

/** Stores the action of control `id` on `host` in the device's memory.
    Errors: EINVAL, ENOENT (no such control), ENOSYS (unsupported action), EIO.
*/
HIDPP_API int hidpp_action_set(
    hidpp_device_t *dev,
    uint16_t id,
    uint8_t host,
    uint8_t type,
    uint16_t value,
    uint8_t modifiers
);

/** Restores factory behaviour of control `id` on `host`. */
HIDPP_API int hidpp_action_reset(
    hidpp_device_t *dev, uint16_t id, uint8_t host
);

/** Restores factory behaviour of all controls on hosts selected by the bit
    mask `hostMask` (bit N selects host N).
*/
HIDPP_API int hidpp_action_reset_all(hidpp_device_t *dev, uint8_t hostMask);

/** Returns the name of an action type. */
HIDPP_API const char *hidpp_action_type_name(uint8_t type);

/** State of a rotating or touch sensitive control within a gesture. */
enum hidpp_rotation_state {
    HIDPP_ROTATION_INACTIVE = 0,
    HIDPP_ROTATION_START = 1,
    HIDPP_ROTATION_ACTIVE = 2,
    HIDPP_ROTATION_STOP = 3,
};

/** Kind of the vertical scrolling roller (feature 0x2100). */
enum hidpp_roller_type {
    HIDPP_ROLLER_STANDARD = 1,
    HIDPP_ROLLER_3G = 3,
    HIDPP_ROLLER_MICRO_RATCHET = 4,
    HIDPP_ROLLER_TOUCH_PAD = 5,
    HIDPP_ROLLER_TOUCH_PAD_NATURAL = 6, /* natural scrolling by default */
};

/** Vertical scrolling (features 0x2100 and 0x2120). */
struct hidpp_scroll_info {
    uint8_t rollerType;      /* enum hidpp_roller_type */
    uint8_t ratchetsPerTurn; /* number of ratchets in a full turn */
    uint8_t scrollLines;     /* 0 = keep the OS setting, 0xFF = one page */
    uint8_t multiplier;      /* counts per ratchet in high resolution mode */

    hidpp_bool_t hasRollerInfo; /* 0x2100: the fields above are valid */
    hidpp_bool_t hasHighRes;    /* 0x2120: high resolution can be toggled */
    const char *rollerTypeName;
    hidpp_bool_t initialized;
};

/** Returns the roller information.
    Errors (as NULL): device has neither of the scrolling features.
*/
HIDPP_API struct hidpp_scroll_info *hidpp_scroll_info(hidpp_device_t *dev);

/** Returns 1 if high resolution scrolling is on, 0 if not, or an error.
    Errors: EINVAL, ENOSYS, EIO.
*/
HIDPP_API int hidpp_scroll_highres(hidpp_device_t *dev);

/** Turns high resolution scrolling on (`enabled` != 0) or off.
    Errors: EINVAL, ENOSYS, EIO.
*/
HIDPP_API int hidpp_scroll_set_highres(hidpp_device_t *dev, int enabled);


/** Keys which can be disabled with feature 0x4521. */
enum hidpp_disable_key {
    HIDPP_DISABLE_CAPS_LOCK = 0x01,
    HIDPP_DISABLE_NUM_LOCK = 0x02,
    HIDPP_DISABLE_SCROLL_LOCK = 0x04,
    HIDPP_DISABLE_INSERT = 0x08,
    HIDPP_DISABLE_WINDOWS = 0x10,
};

/** What keys can be disabled (features 0x4521 and 0x4522). */
struct hidpp_disable_keys_info {
    uint8_t keys;      /* 0x4521: mask of enum hidpp_disable_key */
    uint8_t maxUsages; /* 0x4522: number of usages that can be disabled */

    hidpp_bool_t hasKeys;
    hidpp_bool_t hasUsages;
    hidpp_bool_t initialized;
};

/** Returns what keys can be disabled.
    Errors (as NULL): device has neither of the features.
*/
HIDPP_API struct hidpp_disable_keys_info *hidpp_disable_keys_info(
    hidpp_device_t *dev
);

/** Returns the mask of disabled keys (enum hidpp_disable_key) or an error.
    Errors: EINVAL, ENOSYS, EIO.
*/
HIDPP_API int hidpp_disable_keys_get(hidpp_device_t *dev);

/** Disables the keys in `mask` and enables all others.
    Errors: EINVAL, ENOSYS, EIO.
*/
HIDPP_API int hidpp_disable_keys_set(hidpp_device_t *dev, uint8_t mask);

/** Disables keyboard keys by their HID usage (keyboard page). They are
    added to the keys which are already disabled.
    Errors: EINVAL, ENOSYS, EIO (also if the device is full).
*/
HIDPP_API int hidpp_disable_usages(
    hidpp_device_t *dev, const uint8_t *usages, size_t count
);

/** Enables keyboard keys previously disabled by their HID usage.
    Errors: EINVAL, ENOSYS, EIO.
*/
HIDPP_API int hidpp_enable_usages(
    hidpp_device_t *dev, const uint8_t *usages, size_t count
);

/** Enables every key disabled by its HID usage.
    Errors: EINVAL, ENOSYS, EIO.
*/
HIDPP_API int hidpp_enable_all_usages(hidpp_device_t *dev);


/** States of the crown button. */
enum hidpp_crown_button {
    HIDPP_CROWN_BUTTON_INACTIVE = 0,
    HIDPP_CROWN_BUTTON_PRESS = 1,
    HIDPP_CROWN_BUTTON_SHORT = 2, /* short press active */
    HIDPP_CROWN_BUTTON_LONG = 3,  /* long press threshold reached */
    HIDPP_CROWN_BUTTON_LONG_ACTIVE = 4,
    HIDPP_CROWN_BUTTON_RELEASE = 5,
};

/** Crown capabilities (feature 0x4600). */
struct hidpp_crown_info {
    uint16_t capabilities;
    uint16_t slots;    /* per revolution */
    uint16_t ratchets; /* per revolution */

    hidpp_bool_t hasButton;
    hidpp_bool_t hasLongPress;
    hidpp_bool_t isMechanized; /* ratchet with no manual control */
    hidpp_bool_t canSetRotationTimeout;
    hidpp_bool_t canSetShortLongTimeout;
    hidpp_bool_t canSetDoubleTapSpeed;
    hidpp_bool_t hasProximity;
    hidpp_bool_t hasTouch;
    hidpp_bool_t hasTap;
    hidpp_bool_t hasDoubleTap;
    hidpp_bool_t initialized;
};

/** Where the crown reports to. */
enum hidpp_crown_reporting {
    HIDPP_CROWN_UNCHANGED = 0, /* only valid when setting */
    HIDPP_CROWN_HID = 1,
    HIDPP_CROWN_DIVERTED = 2, /* HID++ (HIDPP_EVENT_CROWN) */
};

/** Crown configuration. When setting, zero fields are left unchanged.
    Times are in milliseconds, in steps of 10.
*/
struct hidpp_crown_mode {
    uint8_t reporting;        /* enum hidpp_crown_reporting */
    uint8_t ratchet;          /* enum hidpp_smartshift_mode */
    uint16_t rotationTimeout; /* at most 640 ms */
    uint16_t shortLongTimeout;
    uint16_t doubleTapSpeed;
};

/** Returns the crown capabilities.
    Errors (as NULL): device has no crown.
*/
HIDPP_API struct hidpp_crown_info *hidpp_crown_info(hidpp_device_t *dev);

/** Reads the crown configuration.
    Errors: EINVAL, ENOSYS, EIO.
*/
HIDPP_API int hidpp_crown_get(
    hidpp_device_t *dev, struct hidpp_crown_mode *out
);

/** Changes the crown configuration. Timeouts the device does not let
    software configure must be 0.
    Errors: EINVAL, ENOSYS, EIO.
*/
HIDPP_API int hidpp_crown_set(
    hidpp_device_t *dev, const struct hidpp_crown_mode *mode
);


/** Brightness control capabilities (feature 0x8040). They are meant for
    cosmetic and RGB lighting, white illumination uses feature 0x1990.
*/
struct hidpp_brightness_info {
    uint8_t version;
    uint16_t min;
    uint16_t max;
    uint16_t steps;
    uint8_t capabilities;

    hidpp_bool_t hasHwBrightness; /* dedicated brightness buttons */
    hidpp_bool_t hasEvents;
    hidpp_bool_t hasState; /* hidpp_brightness_state() works */
    hidpp_bool_t hasHwOnOff;
    hidpp_bool_t isTransient; /* settings are not persistent */
    hidpp_bool_t initialized;
};

/** Returns the brightness capabilities.
    Errors (as NULL): device has no brightness control.
*/
HIDPP_API struct hidpp_brightness_info *hidpp_brightness_info(
    hidpp_device_t *dev
);

/** Returns the brightness or an error.
    Errors: EINVAL, ENOSYS, EIO.
*/
HIDPP_API int hidpp_brightness_get(hidpp_device_t *dev);

/** Sets the brightness (`min` to `max`).
    Errors: EINVAL, ENOSYS, EIO.
*/
HIDPP_API int hidpp_brightness_set(hidpp_device_t *dev, uint16_t value);

/** Returns 1 if the lighting is on, 0 if it is off, or an error.
    Errors: EINVAL, ENOSYS, EIO.
*/
HIDPP_API int hidpp_brightness_state(hidpp_device_t *dev);

/** Turns the lighting on (`on` != 0) or off.
    Errors: EINVAL, ENOSYS, EIO.
*/
HIDPP_API int hidpp_brightness_set_state(hidpp_device_t *dev, int on);


/** Returns the HID++ error code (`enum hidpp_protocol_error`) the device
    answered the last request of `rcv` with, or 0 if the last request did not
    fail that way. Requests answered by an error report fail with HIDPP_EIO,
    so this is what tells them apart from transport failures and from each
    other (for example "busy" from "invalid argument").
*/
HIDPP_API int hidpp_error_code(hidpp_receiver_t *rcv);

/** Returns a human-readable string for a HID++ error code. */
HIDPP_API const char *hidpp_protocol_error_str(int code);

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
