/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

/** Section: HID++ feature documentation

    Per-key lighting (id: 0x8081)
    Big endian integers are exchanged. Zone ids 0 and 255 are invalid and
    end the list of zones of a request.

    -----------------------
    Fn 0 - get info
    -----------------------
    request:
        uint8_t typeOfInfo (0 = zone presence)
        uint8_t param (0 = zones 0-111, 1 = 112-223, 2 = 224-255)
    response:
        uint8_t typeOfInfo
        uint8_t param
        uint8_t presence[14] (bit N of byte K - zone 112 * param + 8K + N)

    -----------------------
    Fn 1 - set individual RGB zones
    -----------------------
    request:
        { uint8_t zone; uint8_t red, green, blue; } [4]
    response:
        uint8_t zones[4]

    -----------------------
    Fn 2 - set consecutive RGB zones
    -----------------------
    request:
        uint8_t firstZone
        { uint8_t red, green, blue; } [5]
    response:
        uint8_t firstZone

    -----------------------
    Fn 3 - set consecutive RGB zones, 5-bit delta compression
    Fn 4 - set consecutive RGB zones, 4-bit delta compression
    -----------------------
    request:
        uint8_t firstZone
        uint8_t deltas[15]
            the signed red, green and blue deltas (current - previous, modulo
            256) of 8 (5-bit) or 10 (4-bit) zones, packed most significant
            bit first
    response:
        uint8_t firstZone

    -----------------------
    Fn 5 - set range RGB zones
    -----------------------
    request:
        { uint8_t first, last; uint8_t red, green, blue; } [3]
    response:
        uint8_t first[3]

    -----------------------
    Fn 6 - set RGB zones single value
    -----------------------
    request:
        uint8_t red, green, blue
        uint8_t zones[13]
    response:
        uint8_t red, green, blue
        uint8_t firstZone

    -----------------------
    Fn 7 - frame end (shows the changes)
    -----------------------
    request:
        uint8_t persistence (0 = RAM, 1 = RAM and EEPROM)
        uint16_t currentFrame
        uint16_t framesTillNextChange (0 when streaming)
    response:
*/

#define PERKEY_ID 0x8081

#define PRESENCE_PAGES 3
#define PRESENCE_PAGE_ZONES 112
#define PRESENCE_BYTES 14

#define FIRST_ZONE 1
#define LAST_ZONE 254

#define INDIVIDUAL_PER_CALL 4
#define CONSECUTIVE_PER_CALL 5
#define DELTA5_PER_CALL 8
#define DELTA4_PER_CALL 10
#define RANGES_PER_CALL 3
#define SINGLE_PER_CALL 13

enum {
    FUNC_INFO,
    FUNC_INDIVIDUAL,
    FUNC_CONSECUTIVE,
    FUNC_DELTA5,
    FUNC_DELTA4,
    FUNC_RANGE,
    FUNC_SINGLE,
    FUNC_FRAME_END,
};

struct hidpp_feat_perkey {
    hidpp_bool_t initialized;
    hidpp_bool_t unsupported;

    uint8_t featIndex;
    hidpp_bool_t hasPresence;
    uint8_t presence[32]; /* bit N of byte K - zone 8K + N */
};

static int query_presence(hidpp_device_t *dev, struct hidpp_feat_perkey *pk) {
    hidpp_packet_t req, res;
    int rc;

    make_packet(&req, dev, pk->featIndex, FUNC_INFO);

    for (uint8_t page = 0; page < PRESENCE_PAGES; page++) {
        req.params[1] = page;

        if ((rc = hidpp_device_request(dev, &req, &res)))
            return is_transient(dev, rc) ? rc : HIDPP_OK;

        for (unsigned k = 0; k < PRESENCE_BYTES; k++) {
            unsigned byte = page * PRESENCE_PAGE_ZONES / 8 + k;

            if (byte < sizeof(pk->presence))
                pk->presence[byte] = res.params[2 + k];
        }
    }

    pk->hasPresence = HIDPP_TRUE;
    return HIDPP_OK;
}

static int ensure_init(struct hidpp_feat_perkey **out, hidpp_device_t *dev) {
    struct hidpp_feat_perkey *pk;

    if (!out || !dev)
        return HIDPP_EINVAL;

    pk = dev->features[HIDPP_FEAT_PERKEY];
    *out = pk;

    if (pk->initialized)
        return pk->unsupported ? HIDPP_ENOSYS : HIDPP_OK;

    memset(pk, 0, sizeof(*pk));
    int featIndex = feature_lookup(dev, PERKEY_ID);
    int rc;

    if (featIndex < 0)
        return featIndex;

    pk->featIndex = (uint8_t)featIndex;
    pk->unsupported = featIndex == 0;

    if (!pk->unsupported && (rc = query_presence(dev, pk)))
        return rc;

    pk->initialized = HIDPP_TRUE;

    return pk->unsupported ? HIDPP_ENOSYS : HIDPP_OK;
}

static void clear_cache(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_perkey *pk = feat;
    (void)dev;
    pk->initialized = HIDPP_FALSE;
}

static void collect_cache(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_perkey *pk;
    (void)feat;
    ensure_init(&pk, dev);
}

static size_t save_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_perkey *pk = feat;
    uint8_t *start = b;
    (void)dev;

    SAVE_BYTE(b, pk->initialized);
    SAVE_BYTE(b, pk->unsupported);

    if (!pk->initialized || pk->unsupported)
        return b - start;

    SAVE_BYTE(b, pk->featIndex);
    SAVE_BYTE(b, pk->hasPresence);
    SAVE_BYTES(b, (const char *)pk->presence, sizeof(pk->presence));

    return b - start;
}

static size_t load_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_perkey *pk = feat;
    uint8_t *start = b;
    (void)dev;

    pk->initialized = LOAD_BYTE(b);
    pk->unsupported = LOAD_BYTE(b);

    if (!pk->initialized || pk->unsupported)
        return b - start;

    pk->featIndex = LOAD_BYTE(b);
    pk->hasPresence = LOAD_BYTE(b);
    LOAD_BYTES(b, (char *)pk->presence, sizeof(pk->presence));

    return b - start;
}

const struct hidpp_feat_vt hidpp_feat_perkey_vt = {
    .size = sizeof(struct hidpp_feat_perkey),
    .alignment = _Alignof(struct hidpp_feat_perkey),
    .maxCacheSize = sizeof(struct hidpp_feat_perkey),
    .minCacheSize = 2 * sizeof(uint8_t),
    .collectCache = collect_cache,
    .saveCache = save_cache,
    .loadCache = load_cache,
    .clearCache = clear_cache,
};

static int is_valid_zone(uint8_t id) {
    return id >= FIRST_ZONE && id <= LAST_ZONE;
}

/* Zones are taken to exist if the device has not told otherwise. */
static int has_zone(const struct hidpp_feat_perkey *pk, uint8_t id) {
    if (!is_valid_zone(id))
        return 0;

    return !pk->hasPresence || (pk->presence[id / 8] & (1u << (id % 8)));
}

/* The last zone of the device, which delta windows must not pass. */
static uint8_t last_zone(const struct hidpp_feat_perkey *pk) {
    for (int id = LAST_ZONE; pk->hasPresence && id > FIRST_ZONE; id--)
        if (has_zone(pk, (uint8_t)id))
            return (uint8_t)id;

    return LAST_ZONE;
}

int hidpp_perkey_zones(hidpp_device_t *dev, uint8_t *out, size_t max) {
    struct hidpp_feat_perkey *pk;
    int rc;

    if (!out && max)
        return HIDPP_EINVAL;

    if ((rc = ensure_init(&pk, dev)))
        return rc;

    if (!pk->hasPresence)
        return HIDPP_ENOSYS;

    int count = 0;

    for (int id = FIRST_ZONE; id <= LAST_ZONE; id++) {
        if (!has_zone(pk, (uint8_t)id))
            continue;

        if ((size_t)count < max)
            out[count] = (uint8_t)id;

        count++;
    }

    return count;
}

int hidpp_perkey_has_zone(hidpp_device_t *dev, uint8_t id) {
    struct hidpp_feat_perkey *pk;
    int rc;

    if ((rc = ensure_init(&pk, dev)))
        return rc;

    return has_zone(pk, id);
}

static int send_individual(
    hidpp_device_t *dev,
    struct hidpp_feat_perkey *pk,
    const uint8_t *ids,
    const struct hidpp_color *colors,
    size_t count
) {
    hidpp_packet_t req, res;
    make_packet(&req, dev, pk->featIndex, FUNC_INDIVIDUAL);
    req.kind = HIDPP_KIND_LONG;

    /* unused entries stay zero, which ends the list */
    for (size_t i = 0; i < count; i++) {
        req.params[4 * i] = ids[i];
        req.params[4 * i + 1] = colors[i].red;
        req.params[4 * i + 2] = colors[i].green;
        req.params[4 * i + 3] = colors[i].blue;
    }

    return hidpp_device_request(dev, &req, &res);
}

int hidpp_perkey_set(
    hidpp_device_t *dev,
    const uint8_t *ids,
    const struct hidpp_color *colors,
    size_t count
) {
    struct hidpp_feat_perkey *pk;
    int rc;

    if ((!ids || !colors) && count)
        return HIDPP_EINVAL;

    if ((rc = ensure_init(&pk, dev)))
        return rc;

    for (size_t i = 0; i < count; i++)
        if (!is_valid_zone(ids[i]))
            return HIDPP_EINVAL;

    for (size_t start = 0; start < count; start += INDIVIDUAL_PER_CALL) {
        size_t n = count - start;

        if (n > INDIVIDUAL_PER_CALL)
            n = INDIVIDUAL_PER_CALL;

        if ((rc = send_individual(dev, pk, &ids[start], &colors[start], n)))
            return rc;
    }

    return HIDPP_OK;
}

static int send_consecutive(
    hidpp_device_t *dev,
    struct hidpp_feat_perkey *pk,
    uint8_t first,
    const struct hidpp_color *colors
) {
    hidpp_packet_t req, res;
    make_packet(&req, dev, pk->featIndex, FUNC_CONSECUTIVE);
    req.kind = HIDPP_KIND_LONG;
    req.params[0] = first;

    for (size_t i = 0; i < CONSECUTIVE_PER_CALL; i++) {
        req.params[1 + 3 * i] = colors[i].red;
        req.params[2 + 3 * i] = colors[i].green;
        req.params[3 + 3 * i] = colors[i].blue;
    }

    return hidpp_device_request(dev, &req, &res);
}

/*  Sets the remaining zones of a consecutive run, which would be padded
    with black by a consecutive request, individually. */
static int send_individual_run(
    hidpp_device_t *dev,
    struct hidpp_feat_perkey *pk,
    uint8_t first,
    const struct hidpp_color *colors,
    size_t count
) {
    uint8_t ids[INDIVIDUAL_PER_CALL];
    int rc;

    for (size_t start = 0; start < count; start += INDIVIDUAL_PER_CALL) {
        size_t n = count - start;

        if (n > INDIVIDUAL_PER_CALL)
            n = INDIVIDUAL_PER_CALL;

        for (size_t i = 0; i < n; i++)
            ids[i] = (uint8_t)(first + start + i);

        if ((rc = send_individual(dev, pk, ids, &colors[start], n)))
            return rc;
    }

    return HIDPP_OK;
}

int hidpp_perkey_set_consecutive(
    hidpp_device_t *dev,
    uint8_t first,
    const struct hidpp_color *colors,
    size_t count
) {
    struct hidpp_feat_perkey *pk;
    int rc;

    if (!colors && count)
        return HIDPP_EINVAL;

    if ((rc = ensure_init(&pk, dev)))
        return rc;

    if (count && (!is_valid_zone(first) || first + count - 1 > LAST_ZONE))
        return HIDPP_EINVAL;

    size_t i = 0;

    for (; i + CONSECUTIVE_PER_CALL <= count; i += CONSECUTIVE_PER_CALL)
        if ((rc = send_consecutive(dev, pk, (uint8_t)(first + i), &colors[i])))
            return rc;

    return send_individual_run(
        dev, pk, (uint8_t)(first + i), &colors[i], count - i
    );
}

int hidpp_perkey_set_range(
    hidpp_device_t *dev, uint8_t first, uint8_t last, struct hidpp_color color
) {
    struct hidpp_feat_perkey *pk;
    int rc;

    if ((rc = ensure_init(&pk, dev)))
        return rc;

    if (!is_valid_zone(first) || !is_valid_zone(last) || first > last)
        return HIDPP_EINVAL;

    /* a range must span more than one zone */
    if (first == last)
        return send_individual(dev, pk, &first, &color, 1);

    hidpp_packet_t req, res;
    make_packet(&req, dev, pk->featIndex, FUNC_RANGE);
    req.kind = HIDPP_KIND_LONG;
    req.params[0] = first;
    req.params[1] = last;
    req.params[2] = color.red;
    req.params[3] = color.green;
    req.params[4] = color.blue;

    return hidpp_device_request(dev, &req, &res);
}

int hidpp_perkey_set_color(
    hidpp_device_t *dev,
    const uint8_t *ids,
    size_t count,
    struct hidpp_color color
) {
    struct hidpp_feat_perkey *pk;
    int rc;

    if (!ids && count)
        return HIDPP_EINVAL;

    if ((rc = ensure_init(&pk, dev)))
        return rc;

    for (size_t i = 0; i < count; i++)
        if (!is_valid_zone(ids[i]))
            return HIDPP_EINVAL;

    hidpp_packet_t req, res;

    for (size_t start = 0; start < count; start += SINGLE_PER_CALL) {
        size_t n = count - start;

        if (n > SINGLE_PER_CALL)
            n = SINGLE_PER_CALL;

        make_packet(&req, dev, pk->featIndex, FUNC_SINGLE);
        req.kind = HIDPP_KIND_LONG;
        req.params[0] = color.red;
        req.params[1] = color.green;
        req.params[2] = color.blue;
        memcpy(&req.params[3], &ids[start], n);

        if ((rc = hidpp_device_request(dev, &req, &res)))
            return rc;
    }

    return HIDPP_OK;
}

/* Shortest signed distance from `from` to `to`, rolling over at 256. */
static int delta(uint8_t from, uint8_t to) {
    return (int8_t)(uint8_t)(to - from);
}

/* Appends the `bits` low bits of `value` to `out`, most significant first. */
static void put_bits(uint8_t *out, size_t *pos, int value, unsigned bits) {
    for (unsigned b = bits; b-- > 0;) {
        if ((unsigned)value & (1u << b))
            out[*pos / 8] |= (uint8_t)(0x80 >> (*pos % 8));

        (*pos)++;
    }
}

struct frame {
    const struct hidpp_color *previous;
    const struct hidpp_color *current;
    size_t count;
};

static int changed(const struct frame *f, size_t i) {
    return i < f->count
        && memcmp(&f->previous[i], &f->current[i], sizeof(f->current[i]));
}

static void deltas_of(const struct frame *f, size_t i, int out[3]) {
    if (i >= f->count) {
        out[0] = out[1] = out[2] = 0; /* zones past the frame stay as is */
        return;
    }

    out[0] = delta(f->previous[i].red, f->current[i].red);
    out[1] = delta(f->previous[i].green, f->current[i].green);
    out[2] = delta(f->previous[i].blue, f->current[i].blue);
}

/* Whether the `zones` zones from `i` fit into `bits` bit deltas. */
static int fits(const struct frame *f, size_t i, size_t zones, unsigned bits) {
    int min = -(1 << (bits - 1)), max = (1 << (bits - 1)) - 1;
    int d[3];

    for (size_t k = i; k < i + zones; k++) {
        deltas_of(f, k, d);

        for (int c = 0; c < 3; c++)
            if (d[c] < min || d[c] > max)
                return 0;
    }

    return 1;
}

static int send_delta(
    hidpp_device_t *dev,
    struct hidpp_feat_perkey *pk,
    const struct frame *f,
    uint8_t first,
    size_t i,
    unsigned bits
) {
    size_t zones = bits == 4 ? DELTA4_PER_CALL : DELTA5_PER_CALL;
    size_t pos = 0;
    int d[3];

    hidpp_packet_t req, res;
    make_packet(
        &req, dev, pk->featIndex, bits == 4 ? FUNC_DELTA4 : FUNC_DELTA5
    );
    req.kind = HIDPP_KIND_LONG;
    req.params[0] = (uint8_t)(first + i);

    for (size_t k = i; k < i + zones; k++) {
        deltas_of(f, k, d);

        for (int c = 0; c < 3; c++)
            put_bits(&req.params[1], &pos, d[c], bits);
    }

    return hidpp_device_request(dev, &req, &res);
}

/*  Follows the procedure suggested by the specification: compressed
    deltas when they are small enough, consecutive zones when most of
    them changed and individual zones for the rest. */
int hidpp_perkey_update(
    hidpp_device_t *dev,
    uint8_t first,
    const struct hidpp_color *previous,
    const struct hidpp_color *current,
    size_t count
) {
    struct hidpp_feat_perkey *pk;
    int rc;

    if ((!previous || !current) && count)
        return HIDPP_EINVAL;

    if ((rc = ensure_init(&pk, dev)))
        return rc;

    if (count && (!is_valid_zone(first) || first + count - 1 > LAST_ZONE))
        return HIDPP_EINVAL;

    struct frame f = { previous, current, count };
    size_t last = last_zone(pk);
    int requests = 0;
    size_t i = 0;

    while (i < count) {
        if (!changed(&f, i)) {
            i++;
            continue;
        }

        /* delta windows may cover zones past the frame, but not the device */
        size_t room = last >= first + i ? last - (first + i) + 1 : 0;

        if (room >= DELTA4_PER_CALL && fits(&f, i, DELTA4_PER_CALL, 4)) {
            rc = send_delta(dev, pk, &f, first, i, 4);
            i += DELTA4_PER_CALL;
        } else if (room >= DELTA5_PER_CALL && fits(&f, i, DELTA5_PER_CALL, 5)) {
            rc = send_delta(dev, pk, &f, first, i, 5);
            i += DELTA5_PER_CALL;
        } else if (
            i + CONSECUTIVE_PER_CALL <= count && changed(&f, i + 1)
            && changed(&f, i + 2) && changed(&f, i + 3)
        ) {
            rc = send_consecutive(dev, pk, (uint8_t)(first + i), &current[i]);
            i += CONSECUTIVE_PER_CALL;
        } else {
            uint8_t ids[INDIVIDUAL_PER_CALL];
            struct hidpp_color colors[INDIVIDUAL_PER_CALL];
            size_t n = 0;

            for (; i < count && n < INDIVIDUAL_PER_CALL; i++) {
                if (!changed(&f, i))
                    continue;

                ids[n] = (uint8_t)(first + i);
                colors[n++] = current[i];
            }

            rc = send_individual(dev, pk, ids, colors, n);
        }

        if (rc)
            return rc;

        requests++;
    }

    return requests;
}

int hidpp_perkey_frame_end(
    hidpp_device_t *dev,
    uint8_t persistence,
    uint16_t frame,
    uint16_t framesToNext
) {
    struct hidpp_feat_perkey *pk;
    int rc;

    if ((rc = ensure_init(&pk, dev)))
        return rc;

    if (persistence > 1)
        return HIDPP_EINVAL;

    hidpp_packet_t req, res;
    make_packet(&req, dev, pk->featIndex, FUNC_FRAME_END);
    req.kind = HIDPP_KIND_LONG;
    req.params[0] = persistence;
    req.params[1] = HIDPP_MSB(frame);
    req.params[2] = HIDPP_LSB(frame);
    req.params[3] = HIDPP_MSB(framesToNext);
    req.params[4] = HIDPP_LSB(framesToNext);

    return hidpp_device_request(dev, &req, &res);
}
