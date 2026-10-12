/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

/*  A fake HID++ 2.0 device used by the tests.

    The fake is attached to the library through `hidpp_open_custom`: every
    report written by the library is parsed and answered according to the
    configured feature set, and the answers are handed back through the
    receive callback. Nothing touches real hardware.

    Packets, call counts and the device state are all exposed so tests can
    check what exactly went over the wire, not only what the API returned.
*/
#ifndef HIDPP_TEST_FAKE_H
#define HIDPP_TEST_FAKE_H

#include <hidpp.h>

#include <stddef.h>
#include <stdint.h>

#define FAKE_MAX_FEATURES 24
#define FAKE_MAX_QUEUE 256
#define FAKE_MAX_LOG 1024
#define FAKE_MAX_CONTROLS 64
#define FAKE_MAX_ENTITIES 8
#define FAKE_NAME_MAX 255

/** A feature handler returns this to stay silent (simulates a timeout). */
#define FAKE_SILENT (-1)

/** Feature indexes of the device created by `fake_new`. */
enum {
    FAKE_IDX_ROOT = 0,
    FAKE_IDX_FEATURE_SET = 1,
    FAKE_IDX_FIRMWARE = 2,
    FAKE_IDX_DEVICE_NAME = 3,
    FAKE_IDX_NICKNAME = 4,
    FAKE_IDX_BATTERY = 5,
    FAKE_IDX_KEYMAP = 6,
};

/** Misbehaviours which can be mixed in `fake_t.noise`. Each of them queues
    an additional packet before the genuine response to a request. */
enum fake_noise {
    /** A notification (swid 0) for the same feature and function. */
    FAKE_NOISE_NOTIFY = 1 << 0,
    /** A well-formed response, but addressed from another device index. */
    FAKE_NOISE_OTHER_DEVICE = 1 << 1,
    /** A response to the same function with a different software id. */
    FAKE_NOISE_OTHER_SWID = 1 << 2,
    /** A response for a different function of the same feature. */
    FAKE_NOISE_OTHER_FUNC = 1 << 3,
    /** An input report which is not HID++ at all. */
    FAKE_NOISE_FOREIGN_REPORT = 1 << 4,
};

typedef struct fake_t fake_t;

/** Handler of a single feature. `func` is the function number (without the
    software id). `res` is pre-filled with the echoed header and zeroed
    parameters. Return 0 on success, a HID++ 2.0 error code (1-255) to send an
    error report, or `FAKE_SILENT` to not answer at all.
*/
typedef int (*fake_handler_fn)(
    fake_t *f, uint8_t func, const hidpp_packet_t *req, hidpp_packet_t *res
);

struct fake_feature {
    uint16_t id;
    uint8_t type;
    uint8_t version;
    fake_handler_fn handler;
    unsigned calls; /* requests addressed at this feature */
};

struct fake_packet {
    uint8_t raw[HIDPP_LEN_XLONG];
    size_t len;
};

struct fake_control {
    uint16_t cid;
    uint16_t tid;
    uint8_t flags;
    uint8_t position;
    uint8_t group;
    uint8_t groupMask;
    uint8_t flags2;
    /* reporting state */
    uint8_t divert;
    uint8_t persist;
    uint8_t rawXY;
    uint16_t remap;
};

struct fake_entity {
    uint8_t type;
    char prefix[3];
    uint8_t number;
    uint8_t revision;
    uint16_t build;
    uint8_t active;
    uint16_t pid;
    uint8_t extra[5];
};

struct fake_t {
    hidpp_receiver_t *rcv;
    hidpp_device_t *dev;

    /* identity */
    uint8_t index;
    uint8_t protoMajor, protoMinor;

    /* features; index 0 is the (implicit) root, features[0] has index 1 */
    struct fake_feature features[FAKE_MAX_FEATURES];
    unsigned featureCount;
    int featureSetCountOverride; /* -1 = report the real count */

    /* transport behaviour */
    int hidpp10;    /* answer everything with a HID++ 1.0 error */
    int silent;     /* never answer */
    int drop;       /* number of upcoming requests to ignore */
    int noise;      /* mask of enum fake_noise */
    int failSend;   /* the send callback fails with HIDPP_EIO */
    int failRecv;   /* the receive callback fails with HIDPP_EIO */
    int replyKind;  /* kind of the responses (default HIDPP_KIND_LONG) */
    int errorKind;  /* kind of the error reports (default HIDPP_KIND_SHORT) */
    int swidOffset; /* added to the echoed software id of responses */
    int garbage;    /* fill every response payload with random bytes */
    uint32_t garbageState; /* PRNG state used by `garbage` */

    /* queue of reports waiting to be read by the library */
    struct fake_packet queue[FAKE_MAX_QUEUE];
    unsigned head, tail;

    /* everything the library has written */
    struct fake_packet log[FAKE_MAX_LOG];
    unsigned requests;
    unsigned calls[256]; /* requests per feature index */

    /* 0x1000 battery status */
    uint8_t batLevel, batNext, batStatus;
    uint8_t batLevelCount, batFlags, batCritical;
    uint16_t batNominal;

    /* 0x0005 device name */
    char name[FAKE_NAME_MAX + 1];
    uint8_t deviceType;

    /* 0x0003 firmware info */
    uint8_t fwEntityCount;
    uint8_t fwUnitId[4];
    uint16_t fwTransport;
    uint8_t fwModelId[6];
    uint8_t fwExtModel;
    uint8_t fwCapabilities;
    uint8_t fwTypeHighBits; /* reserved bits OR-ed into entity types */
    char fwSerial[12];
    struct fake_entity fwEntities[FAKE_MAX_ENTITIES];

    /* 0x0007 friendly name */
    char nick[FAKE_NAME_MAX + 1];
    char nickDefault[FAKE_NAME_MAX + 1];
    uint8_t nickMax;

    /* 0x1b04 reprogrammable controls */
    uint8_t keymapCount;
    struct fake_control controls[FAKE_MAX_CONTROLS];
};

/** Creates a device with only Root and Feature Set. The library receiver is
    already open, but no device object has been opened yet. */
fake_t *fake_new_bare(void);

/** Creates a device with every feature the library supports, populated with
    realistic data (see fake.c for the exact values). */
fake_t *fake_new(void);

/** Closes the receiver (and all devices) and frees the fake. */
void fake_free(fake_t *f);

/** Opens the fake device through the library. Returns NULL on failure. */
hidpp_device_t *fake_open(fake_t *f);

/** Adds a feature at the next free index and returns that index. */
int fake_add_feature(
    fake_t *f, uint16_t id, uint8_t version, fake_handler_fn handler
);

/** Adds the individual features. Return the feature index. */
int fake_add_battery(fake_t *f);
int fake_add_device_name(fake_t *f);
int fake_add_firmware_info(fake_t *f);
int fake_add_nickname(fake_t *f);
int fake_add_keymap(fake_t *f, uint16_t id);

/** Appends a control to the keymap. Returns the control index. */
int fake_add_control(
    fake_t *f, uint16_t cid, uint16_t tid, uint8_t flags, uint8_t position
);

/** Queues a notification (swid 0) from the given feature index. */
void fake_notify(
    fake_t *f, uint8_t featIndex, uint8_t func, const uint8_t *data, size_t n
);

/** Queues raw bytes as if the device had sent them. */
void fake_queue_raw(fake_t *f, const void *data, size_t len);

/** Number of reports waiting in the queue. */
unsigned fake_pending(const fake_t *f);

/** Drops everything in the queue. */
void fake_flush(fake_t *f);

/** Returns the n-th logged report (n counts from 0). */
const struct fake_packet *fake_logged(const fake_t *f, unsigned n);

/** Returns the last logged report or NULL. */
const struct fake_packet *fake_last(const fake_t *f);

/** Total number of requests addressed at `featIndex` so far. */
unsigned fake_calls(const fake_t *f, uint8_t featIndex);

/** Forgets the log and the counters (but not the device state). */
void fake_reset_log(fake_t *f);

/** Finds a control by its id. */
struct fake_control *fake_control(fake_t *f, uint16_t cid);

#endif /* HIDPP_TEST_FAKE_H */
