/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

/** Section: HID++ feature documentation

    Extended adjustable DPI (id: 0x2202)
    Big endian integers are exchanged. Directions are 0 = X and 1 = Y.

    -----------------------
    Fn 0 - get sensor count
    -----------------------
    request:
    response:
        uint8_t sensorCount

    -----------------------
    Fn 1 - get sensor capabilities
    -----------------------
    request:
        uint8_t sensor
    response:
        uint8_t sensor
        uint8_t dpiLevelCount
        uint8_t flags
        - bit 0 - separate Y resolution
        - bit 1 - lift off distance
        - bit 2 - calibration
        - bit 3 - profile

    -----------------------
    Fn 2 - get sensor DPI ranges
    -----------------------
    request:
        uint8_t sensor
        uint8_t direction
        uint8_t requestIndex (0, 1, ...)
    response:
        uint8_t sensor
        uint8_t direction
        uint8_t requestIndex
        uint8_t data[13]

        The data of all requests forms one list of words, terminated by
        0x0000. A word of 0xE000 | step between two values means every
        step-th resolution between them can be used, which is why the
        highest resolution is 0xDFFF.

    -----------------------
    Fn 3 - get sensor DPI list (of the current profile)
    -----------------------
    request:
        uint8_t sensor
        uint8_t direction
    response:
        uint8_t sensor
        uint8_t direction
        uint16_t dpi[] (terminated by 0x0000)

    -----------------------
    Fn 4 - get sensor LOD list (of the current profile)
    -----------------------
    request:
        uint8_t sensor
    response:
        uint8_t sensor
        uint8_t lod[3] (1 = low, 2 = medium, 3 = high, terminated by 0)

    -----------------------
    Fn 5 - get sensor DPI parameters
    -----------------------
    request:
        uint8_t sensor
    response:
        uint8_t sensor
        uint16_t dpiX
        uint16_t defaultDpiX
        uint16_t dpiY
        uint16_t defaultDpiY
        uint8_t lod

    -----------------------
    Fn 6 - set sensor DPI parameters
    -----------------------
    request:
        uint8_t sensor
        uint16_t dpiX
        uint16_t dpiY
        uint8_t lod
    response:

    -----------------------
    Fn 7 - show sensor DPI status
    -----------------------
    request:
        uint8_t sensor
        uint8_t dpiLevel (1 .. dpiLevelCount)
        uint8_t ledHoldType (timer, event, on, off)
        uint8_t buttonNumber
    response:

    -----------------------
    Fn 8 - get DPI calibration info
    -----------------------
    request:
        uint8_t sensor
    response:
        uint8_t sensor
        uint8_t mouseWidth
        uint16_t mouseLength
        uint16_t calibDpiX
        uint16_t calibDpiY

    -----------------------
    Fn 9 - start DPI calibration
    -----------------------
    request:
        uint8_t sensor
        uint8_t direction
        uint16_t expectedCount
        uint8_t calibType (0 = hardware, 1 = software)
        uint8_t calibStartTimeout (seconds, at most 60)
        uint8_t calibHWProcessTimeout
        uint8_t calibSWProcessTimeout
    response:

    -----------------------
    Fn 10 - set DPI calibration
    -----------------------
    request:
        uint8_t sensor
        uint8_t direction
        int16_t calibCor (-1023 to 1023)
    response:

    -----------------------
    Events
    -----------------------
    0 - sensor DPI parameters:
        uint8_t sensor, uint16_t dpiX, uint16_t dpiY, uint8_t lod
    1 - DPI calibration completed:
        uint8_t sensor, uint8_t direction, int16_t calibCor,
        int16_t calibDelta
*/

#define DPI_ID 0x2202

#define MAX_SENSORS 4
#define MAX_RANGE_WORDS 48
#define MAX_RANGE_REQUESTS 8
#define RANGE_CHUNK 13
#define STEP_MARKER 0xE000
#define STEP_MASK 0x1FFF
#define MAX_LOD HIDPP_DPI_LOD_HIGH
#define MAX_CORRECTION 1023
#define MAX_TIMEOUT 60

/* clang-format off */

#define CAP_Y_FLAG           0x01
#define CAP_LOD_FLAG         0x02
#define CAP_CALIBRATION_FLAG 0x04
#define CAP_PROFILE_FLAG     0x08

/* clang-format on */

struct dpi_sensor {
    struct hidpp_dpi_sensor info;
    uint8_t rangeCount[2];
    uint16_t ranges[2][MAX_RANGE_WORDS]; /* raw words, without the 0 */
};

struct hidpp_feat_dpi {
    struct hidpp_feat_base base;
    uint8_t sensorCount;
    struct dpi_sensor sensors[MAX_SENSORS];
};

static void unpack_flags(struct hidpp_dpi_sensor *info) {
    info->hasY = !!(info->flags & CAP_Y_FLAG);
    info->hasLod = !!(info->flags & CAP_LOD_FLAG);
    info->hasCalibration = !!(info->flags & CAP_CALIBRATION_FLAG);
    info->hasProfile = !!(info->flags & CAP_PROFILE_FLAG);
}

static int is_step(uint16_t word) {
    return (word & STEP_MARKER) == STEP_MARKER;
}

/* Collects the range words of `direction`, which span several requests. */
static int query_ranges(
    hidpp_device_t *dev,
    struct hidpp_feat_dpi *dpi,
    struct dpi_sensor *s,
    uint8_t direction
) {
    uint8_t bytes[MAX_RANGE_REQUESTS * RANGE_CHUNK + 1];
    size_t length = 0;
    hidpp_packet_t req, res;
    int rc;

    make_packet(&req, dev, dpi->base.featIndex, 2);
    req.params[0] = s->info.index;
    req.params[1] = direction;

    for (uint8_t n = 0; n < MAX_RANGE_REQUESTS; n++) {
        req.params[2] = n;

        if ((rc = hidpp_device_request(dev, &req, &res)))
            return rc;

        memcpy(&bytes[length], &res.params[3], RANGE_CHUNK);
        length += RANGE_CHUNK;

        /* a complete zero word ends the list */
        size_t k = 0;

        while (k + 1 < length && HIDPP_WORD(bytes[k], bytes[k + 1]))
            k += 2;

        if (k + 1 < length)
            break;
    }

    uint8_t count = 0;

    for (size_t k = 0; k + 1 < length && count < MAX_RANGE_WORDS; k += 2) {
        uint16_t word = HIDPP_WORD(bytes[k], bytes[k + 1]);

        if (word == 0)
            break;

        s->ranges[direction][count++] = word;
    }

    s->rangeCount[direction] = count;
    return HIDPP_OK;
}

static int query_sensor(
    hidpp_device_t *dev, struct hidpp_feat_dpi *dpi, uint8_t index
) {
    struct dpi_sensor *s = &dpi->sensors[index];
    hidpp_packet_t req, res;
    int rc;

    make_packet(&req, dev, dpi->base.featIndex, 1);
    req.params[0] = index;

    /* a sensor the device refuses for good stays uninitialized */
    if ((rc = hidpp_device_request(dev, &req, &res)))
        return is_transient(dev, rc) ? rc : HIDPP_OK;

    s->info.index = index;
    s->info.levelCount = res.params[1];
    s->info.flags = res.params[2];
    unpack_flags(&s->info);

    for (uint8_t dir = HIDPP_DPI_X; dir <= HIDPP_DPI_Y; dir++) {
        if (dir == HIDPP_DPI_Y && !s->info.hasY)
            break;

        if ((rc = query_ranges(dev, dpi, s, dir)) && is_transient(dev, rc))
            return rc;
    }

    s->info.initialized = HIDPP_TRUE;
    return HIDPP_OK;
}

static int query_info(hidpp_device_t *dev, void *feat) {
    struct hidpp_feat_dpi *dpi = feat;
    hidpp_packet_t req, res;
    make_packet(&req, dev, dpi->base.featIndex, 0);

    int rc;

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return is_transient(dev, rc) ? rc : HIDPP_OK;

    dpi->sensorCount = res.params[0] < MAX_SENSORS ? res.params[0]
                                                   : MAX_SENSORS;

    for (uint8_t i = 0; i < dpi->sensorCount; i++)
        if ((rc = query_sensor(dev, dpi, i)))
            return rc;

    return HIDPP_OK;
}

static int ensure_init(struct hidpp_feat_dpi **out, hidpp_device_t *dev) {
    void *feat;
    int rc = hidpp__feature_init(dev, HIDPP_FEAT_DPI, &feat);
    *out = feat;
    return rc;
}

static size_t save_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_dpi *dpi = feat;
    uint8_t *start = b;
    (void)dev;

    SAVE_BYTE(b, dpi->sensorCount);

    for (uint8_t i = 0; i < dpi->sensorCount; i++) {
        struct dpi_sensor *s = &dpi->sensors[i];
        SAVE_BYTE(b, s->info.initialized);
        SAVE_BYTE(b, s->info.levelCount);
        SAVE_BYTE(b, s->info.flags);

        for (int dir = 0; dir < 2; dir++) {
            SAVE_BYTE(b, s->rangeCount[dir]);

            for (uint8_t k = 0; k < s->rangeCount[dir]; k++)
                SAVE_WORD(b, s->ranges[dir][k]);
        }
    }

    return b - start;
}

static size_t load_cache(hidpp_device_t *dev, void *feat, uint8_t *b) {
    struct hidpp_feat_dpi *dpi = feat;
    uint8_t *start = b;
    (void)dev;

    dpi->sensorCount = LOAD_BYTE(b);

    if (dpi->sensorCount > MAX_SENSORS)
        dpi->sensorCount = MAX_SENSORS;

    for (uint8_t i = 0; i < dpi->sensorCount; i++) {
        struct dpi_sensor *s = &dpi->sensors[i];
        s->info.index = i;
        s->info.initialized = LOAD_BYTE(b);
        s->info.levelCount = LOAD_BYTE(b);
        s->info.flags = LOAD_BYTE(b);
        unpack_flags(&s->info);

        for (int dir = 0; dir < 2; dir++) {
            s->rangeCount[dir] = LOAD_BYTE(b);

            if (s->rangeCount[dir] > MAX_RANGE_WORDS)
                s->rangeCount[dir] = MAX_RANGE_WORDS;

            for (uint8_t k = 0; k < s->rangeCount[dir]; k++)
                s->ranges[dir][k] = LOAD_WORD(b);
        }
    }

    return b - start;
}

static int parse_event(
    hidpp_device_t *dev,
    void *feat,
    uint16_t id,
    uint8_t func,
    const hidpp_packet_t *pkt,
    struct hidpp_event *e
) {
    const uint8_t *p = pkt->params;
    (void)dev, (void)feat;

    if (id != DPI_ID)
        return HIDPP_ENOENT;

    if (func == 0) {
        e->type = HIDPP_EVENT_DPI;
        e->as.dpi.sensor = p[0];
        e->as.dpi.x = HIDPP_WORD(p[1], p[2]);
        e->as.dpi.y = HIDPP_WORD(p[3], p[4]);
        e->as.dpi.lod = p[5];
        return HIDPP_OK;
    } else if (func == 1) {
        e->type = HIDPP_EVENT_DPI_CALIBRATION;
        e->as.dpiCalibration.sensor = p[0];
        e->as.dpiCalibration.direction = p[1];
        e->as.dpiCalibration.correction = (int16_t)HIDPP_WORD(p[2], p[3]);
        e->as.dpiCalibration.delta = (int16_t)HIDPP_WORD(p[4], p[5]);
        return HIDPP_OK;
    }

    return HIDPP_ENOENT;
}

const struct hidpp_feat_vt hidpp_feat_dpi_vt = {
    FEAT_VT_SIZE(struct hidpp_feat_dpi),
    .ids = { DPI_ID },
    .query = query_info,
    .saveCache = save_cache,
    .loadCache = load_cache,
    .parseEvent = parse_event,
};

static int get_sensor(
    hidpp_device_t *dev,
    uint8_t sensor,
    struct hidpp_feat_dpi **dpi,
    struct dpi_sensor **out
) {
    int rc;

    if ((rc = ensure_init(dpi, dev)))
        return rc;

    if (sensor >= (*dpi)->sensorCount)
        return HIDPP_EINVAL;

    *out = &(*dpi)->sensors[sensor];
    return (*out)->info.initialized ? HIDPP_OK : HIDPP_ENOSYS;
}

static int check_direction(const struct dpi_sensor *s, uint8_t direction) {
    if (direction > HIDPP_DPI_Y || (direction == HIDPP_DPI_Y && !s->info.hasY))
        return HIDPP_EINVAL;

    return HIDPP_OK;
}

/*  Decodes the range words. A value followed by a step marker and another
    value is a range, the end of which can start the next range. */
static size_t decode_ranges(
    const uint16_t *words, size_t n, struct hidpp_dpi_range *out, size_t max
) {
    size_t count = 0;
    hidpp_bool_t ended = HIDPP_FALSE; /* words[i] ended the previous range */

    for (size_t i = 0; i < n; i++) {
        uint16_t value = words[i];

        if (is_step(value)) {
            ended = HIDPP_FALSE;
            continue; /* a marker without a start value */
        }

        if (i + 2 < n && is_step(words[i + 1]) && !is_step(words[i + 2])) {
            if (out && count < max) {
                out[count].min = value;
                out[count].max = words[i + 2];
                out[count].step = words[i + 1] & STEP_MASK;
            }

            count++;
            i++; /* continue at the end value */
            ended = HIDPP_TRUE;
            continue;
        }

        if (!ended) {
            if (out && count < max) {
                out[count].min = value;
                out[count].max = value;
                out[count].step = 0;
            }

            count++;
        }

        ended = HIDPP_FALSE;
    }

    return count;
}

static int check_dpi(
    const struct dpi_sensor *s, uint8_t direction, uint16_t value
) {
    struct hidpp_dpi_range ranges[MAX_RANGE_WORDS];

    if (value == 0 || value > HIDPP_DPI_MAX)
        return HIDPP_EINVAL;

    size_t count = decode_ranges(
        s->ranges[direction], s->rangeCount[direction], ranges, MAX_RANGE_WORDS
    );

    if (count == 0)
        return HIDPP_OK; /* the device has not told */

    for (size_t i = 0; i < count && i < MAX_RANGE_WORDS; i++) {
        const struct hidpp_dpi_range *r = &ranges[i];

        if (value < r->min || value > r->max)
            continue;

        if (r->step == 0 || (value - r->min) % r->step == 0)
            return HIDPP_OK;
    }

    return HIDPP_EINVAL;
}

int hidpp_dpi_sensor_count(hidpp_device_t *dev) {
    struct hidpp_feat_dpi *dpi;
    int rc;

    if ((rc = ensure_init(&dpi, dev)))
        return rc;

    return dpi->sensorCount;
}

struct hidpp_dpi_sensor *hidpp_dpi_sensor(hidpp_device_t *dev, uint8_t sensor) {
    struct hidpp_feat_dpi *dpi;
    struct dpi_sensor *s;

    if (!dev || get_sensor(dev, sensor, &dpi, &s))
        return NULL;

    return &s->info;
}

int hidpp_dpi_ranges(
    hidpp_device_t *dev,
    uint8_t sensor,
    uint8_t direction,
    struct hidpp_dpi_range *out,
    size_t max
) {
    struct hidpp_feat_dpi *dpi;
    struct dpi_sensor *s;
    int rc;

    if (!out && max)
        return HIDPP_EINVAL;

    if ((rc = get_sensor(dev, sensor, &dpi, &s)))
        return rc;

    if ((rc = check_direction(s, direction)))
        return rc;

    size_t count = decode_ranges(
        s->ranges[direction], s->rangeCount[direction], out, max
    );
    return (int)(count < max ? count : max);
}

int hidpp_dpi_levels(
    hidpp_device_t *dev,
    uint8_t sensor,
    uint8_t direction,
    uint16_t *out,
    size_t max
) {
    struct hidpp_feat_dpi *dpi;
    struct dpi_sensor *s;
    int rc;

    if (!out && max)
        return HIDPP_EINVAL;

    if ((rc = get_sensor(dev, sensor, &dpi, &s)))
        return rc;

    if ((rc = check_direction(s, direction)))
        return rc;

    hidpp_packet_t req, res;
    make_packet(&req, dev, dpi->base.featIndex, 3);
    req.params[0] = sensor;
    req.params[1] = direction;

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    size_t count = 0;

    for (size_t k = 2; k + 1 < HIDPP_LEN_LONG - 4 && count < max; k += 2) {
        uint16_t value = HIDPP_WORD(res.params[k], res.params[k + 1]);

        if (value == 0)
            break;

        out[count++] = value;
    }

    return (int)count;
}

int hidpp_dpi_lods(
    hidpp_device_t *dev, uint8_t sensor, uint8_t *out, size_t max
) {
    struct hidpp_feat_dpi *dpi;
    struct dpi_sensor *s;
    int rc;

    if (!out && max)
        return HIDPP_EINVAL;

    if ((rc = get_sensor(dev, sensor, &dpi, &s)))
        return rc;

    if (!s->info.hasLod)
        return HIDPP_ENOSYS;

    hidpp_packet_t req, res;
    make_packet(&req, dev, dpi->base.featIndex, 4);
    req.params[0] = sensor;

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    size_t count = 0;

    for (size_t k = 1; k <= MAX_LOD && count < max; k++) {
        if (res.params[k] == 0)
            break;

        out[count++] = res.params[k];
    }

    return (int)count;
}

int hidpp_dpi_get(hidpp_device_t *dev, uint8_t sensor, struct hidpp_dpi *out) {
    struct hidpp_feat_dpi *dpi;
    struct dpi_sensor *s;
    int rc;

    if (!out)
        return HIDPP_EINVAL;

    if ((rc = get_sensor(dev, sensor, &dpi, &s)))
        return rc;

    hidpp_packet_t req, res;
    make_packet(&req, dev, dpi->base.featIndex, 5);
    req.params[0] = sensor;

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    out->x = HIDPP_WORD(res.params[1], res.params[2]);
    out->defaultX = HIDPP_WORD(res.params[3], res.params[4]);
    out->y = s->info.hasY ? HIDPP_WORD(res.params[5], res.params[6]) : 0;
    out->defaultY = s->info.hasY ? HIDPP_WORD(res.params[7], res.params[8]) : 0;
    out->lod = s->info.hasLod ? res.params[9] : 0;
    return HIDPP_OK;
}

int hidpp_dpi_set(
    hidpp_device_t *dev, uint8_t sensor, uint16_t x, uint16_t y, uint8_t lod
) {
    struct hidpp_feat_dpi *dpi;
    struct dpi_sensor *s;
    int rc;

    if ((rc = get_sensor(dev, sensor, &dpi, &s)))
        return rc;

    if ((rc = check_dpi(s, HIDPP_DPI_X, x)))
        return rc;

    if (s->info.hasY) {
        if ((rc = check_dpi(s, HIDPP_DPI_Y, y)))
            return rc;
    } else if (y != 0) {
        return HIDPP_EINVAL;
    }

    if (s->info.hasLod ? lod > MAX_LOD : lod != 0)
        return HIDPP_EINVAL;

    hidpp_packet_t req, res;
    make_packet(&req, dev, dpi->base.featIndex, 6);
    req.params[0] = sensor;
    req.params[1] = HIDPP_MSB(x);
    req.params[2] = HIDPP_LSB(x);
    req.params[3] = HIDPP_MSB(y);
    req.params[4] = HIDPP_LSB(y);
    req.params[5] = lod;
    req.kind = HIDPP_KIND_LONG;

    return hidpp_device_request(dev, &req, &res);
}

int hidpp_dpi_show(
    hidpp_device_t *dev,
    uint8_t sensor,
    uint8_t level,
    uint8_t hold,
    uint8_t button
) {
    struct hidpp_feat_dpi *dpi;
    struct dpi_sensor *s;
    int rc;

    if ((rc = get_sensor(dev, sensor, &dpi, &s)))
        return rc;

    if (level == 0 || (s->info.levelCount && level > s->info.levelCount))
        return HIDPP_EINVAL;

    if (hold > HIDPP_DPI_LED_OFF)
        return HIDPP_EINVAL;

    hidpp_packet_t req, res;
    make_packet(&req, dev, dpi->base.featIndex, 7);
    req.params[0] = sensor;
    req.params[1] = level;
    req.params[2] = hold;
    req.params[3] = button;
    req.kind = HIDPP_KIND_LONG;

    return hidpp_device_request(dev, &req, &res);
}

int hidpp_dpi_calibration_info(
    hidpp_device_t *dev, uint8_t sensor, struct hidpp_dpi_calibration_info *out
) {
    struct hidpp_feat_dpi *dpi;
    struct dpi_sensor *s;
    int rc;

    if (!out)
        return HIDPP_EINVAL;

    if ((rc = get_sensor(dev, sensor, &dpi, &s)))
        return rc;

    if (!s->info.hasCalibration)
        return HIDPP_ENOSYS;

    hidpp_packet_t req, res;
    make_packet(&req, dev, dpi->base.featIndex, 8);
    req.params[0] = sensor;

    if ((rc = hidpp_device_request(dev, &req, &res)))
        return rc;

    out->mouseWidth = res.params[1];
    out->mouseLength = HIDPP_WORD(res.params[2], res.params[3]);
    out->dpiX = HIDPP_WORD(res.params[4], res.params[5]);
    out->dpiY = HIDPP_WORD(res.params[6], res.params[7]);
    return HIDPP_OK;
}

int hidpp_dpi_start_calibration(
    hidpp_device_t *dev, uint8_t sensor, const struct hidpp_dpi_calibration *cal
) {
    struct hidpp_feat_dpi *dpi;
    struct dpi_sensor *s;
    int rc;

    if (!cal)
        return HIDPP_EINVAL;

    if ((rc = get_sensor(dev, sensor, &dpi, &s)))
        return rc;

    if (!s->info.hasCalibration)
        return HIDPP_ENOSYS;

    if ((rc = check_direction(s, cal->direction)))
        return rc;

    if (cal->type > 1 || cal->startTimeout > MAX_TIMEOUT
        || cal->hwProcessTimeout > MAX_TIMEOUT
        || cal->swProcessTimeout > MAX_TIMEOUT)
        return HIDPP_EINVAL;

    hidpp_packet_t req, res;
    make_packet(&req, dev, dpi->base.featIndex, 9);
    req.kind = HIDPP_KIND_LONG;
    req.params[0] = sensor;
    req.params[1] = cal->direction;
    req.params[2] = HIDPP_MSB(cal->expectedCount);
    req.params[3] = HIDPP_LSB(cal->expectedCount);
    req.params[4] = cal->type;
    req.params[5] = cal->startTimeout;
    req.params[6] = cal->hwProcessTimeout;
    req.params[7] = cal->swProcessTimeout;

    return hidpp_device_request(dev, &req, &res);
}

int hidpp_dpi_set_calibration(
    hidpp_device_t *dev, uint8_t sensor, uint8_t direction, int16_t correction
) {
    struct hidpp_feat_dpi *dpi;
    struct dpi_sensor *s;
    int rc;

    if ((rc = get_sensor(dev, sensor, &dpi, &s)))
        return rc;

    if (!s->info.hasCalibration)
        return HIDPP_ENOSYS;

    if ((rc = check_direction(s, direction)))
        return rc;

    if (correction < -MAX_CORRECTION || correction > MAX_CORRECTION)
        return HIDPP_EINVAL;

    uint16_t raw = (uint16_t)correction;
    hidpp_packet_t req, res;
    make_packet(&req, dev, dpi->base.featIndex, 10);
    req.kind = HIDPP_KIND_LONG;
    req.params[0] = sensor;
    req.params[1] = direction;
    req.params[2] = HIDPP_MSB(raw);
    req.params[3] = HIDPP_LSB(raw);

    return hidpp_device_request(dev, &req, &res);
}
