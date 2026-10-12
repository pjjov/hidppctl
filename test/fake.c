/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "fake.h"

#include <stdlib.h>
#include <string.h>

/* HID++ 2.0 error codes used by the fake. */
#define ERR_INVALID_ARGUMENT 2
#define ERR_OUT_OF_RANGE 3
#define ERR_INVALID_FEATURE_INDEX 6
#define ERR_INVALID_FUNCTION 7

static size_t kind_length(int kind) {
    switch (kind) {
    case HIDPP_KIND_SHORT:
        return HIDPP_LEN_SHORT;
    case HIDPP_KIND_LONG:
        return HIDPP_LEN_LONG;
    case HIDPP_KIND_XLONG:
        return HIDPP_LEN_XLONG;
    default:
        return 0;
    }
}

/* ------------------------------------------------------------------ */
/* Queue                                                              */
/* ------------------------------------------------------------------ */

void fake_queue_raw(fake_t *f, const void *data, size_t len) {
    if (len > HIDPP_LEN_XLONG)
        len = HIDPP_LEN_XLONG;

    struct fake_packet *p = &f->queue[f->tail % FAKE_MAX_QUEUE];
    memset(p, 0, sizeof(*p));
    memcpy(p->raw, data, len);
    p->len = len;
    f->tail++;
}

static void queue_packet(fake_t *f, const hidpp_packet_t *pkt) {
    size_t len = kind_length(pkt->kind);

    if (len == 0)
        return;

    uint8_t raw[HIDPP_LEN_XLONG] = { 0 };
    raw[0] = pkt->kind;
    raw[1] = pkt->device;
    raw[2] = pkt->feat;
    raw[3] = pkt->func;
    memcpy(&raw[4], pkt->params, len - 4);
    fake_queue_raw(f, raw, len);
}

unsigned fake_pending(const fake_t *f) { return f->tail - f->head; }

void fake_flush(fake_t *f) { f->head = f->tail; }

void fake_notify(
    fake_t *f, uint8_t featIndex, uint8_t func, const uint8_t *data, size_t n
) {
    hidpp_packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.kind = HIDPP_KIND_LONG;
    pkt.device = f->index;
    pkt.feat = featIndex;
    pkt.func = (uint8_t)(func << 4); /* software id 0 marks notifications */

    if (data)
        memcpy(pkt.params, data, n > 16 ? 16 : n);

    queue_packet(f, &pkt);
}

/* ------------------------------------------------------------------ */
/* Log                                                                */
/* ------------------------------------------------------------------ */

const struct fake_packet *fake_logged(const fake_t *f, unsigned n) {
    return n < f->requests && n < FAKE_MAX_LOG ? &f->log[n] : NULL;
}

const struct fake_packet *fake_last(const fake_t *f) {
    return f->requests ? fake_logged(f, f->requests - 1) : NULL;
}

unsigned fake_calls(const fake_t *f, uint8_t featIndex) {
    return f->calls[featIndex];
}

void fake_reset_log(fake_t *f) {
    f->requests = 0;
    memset(f->calls, 0, sizeof(f->calls));
    for (unsigned i = 0; i < FAKE_MAX_FEATURES; i++)
        f->features[i].calls = 0;
}

/* ------------------------------------------------------------------ */
/* Feature handlers                                                   */
/* ------------------------------------------------------------------ */

/* number of parameter bytes that fit into the response */
static size_t reply_capacity(const hidpp_packet_t *res) {
    return kind_length(res->kind) - 4;
}

static int root_handler(
    fake_t *f, uint8_t func, const hidpp_packet_t *req, hidpp_packet_t *res
) {
    switch (func) {
    case 0: { /* getFeature(id) */
        uint16_t id = HIDPP_WORD(req->params[0], req->params[1]);

        if (id == 0x0000)
            return 0; /* the root itself lives at index 0 */

        for (unsigned i = 0; i < f->featureCount; i++) {
            if (f->features[i].id == id) {
                res->params[0] = (uint8_t)(i + 1);
                res->params[1] = f->features[i].type;
                res->params[2] = f->features[i].version;
                return 0;
            }
        }
        return 0; /* index 0 means "not found" */
    }
    case 1: /* ping */
        res->params[0] = f->protoMajor;
        res->params[1] = f->protoMinor;
        res->params[2] = req->params[2];
        return 0;
    default:
        return ERR_INVALID_FUNCTION;
    }
}

static int feature_set_handler(
    fake_t *f, uint8_t func, const hidpp_packet_t *req, hidpp_packet_t *res
) {
    switch (func) {
    case 0:
        res->params[0] = f->featureSetCountOverride >= 0
            ? (uint8_t)f->featureSetCountOverride
            : (uint8_t)f->featureCount;
        return 0;
    case 1: {
        uint8_t index = req->params[0];

        if (index > f->featureCount)
            return ERR_INVALID_ARGUMENT;

        if (index == 0)
            return 0; /* root: id 0x0000 */

        struct fake_feature *ft = &f->features[index - 1];
        res->params[0] = HIDPP_MSB(ft->id);
        res->params[1] = HIDPP_LSB(ft->id);
        res->params[2] = ft->type;
        res->params[3] = ft->version;
        return 0;
    }
    default:
        return ERR_INVALID_FUNCTION;
    }
}

static int battery_handler(
    fake_t *f, uint8_t func, const hidpp_packet_t *req, hidpp_packet_t *res
) {
    (void)req;
    switch (func) {
    case 0:
        res->params[0] = f->batLevel;
        res->params[1] = f->batNext;
        res->params[2] = f->batStatus;
        return 0;
    case 1:
        res->params[0] = f->batLevelCount;
        res->params[1] = f->batFlags;
        res->params[2] = HIDPP_MSB(f->batNominal);
        res->params[3] = HIDPP_LSB(f->batNominal);
        res->params[4] = f->batCritical;
        return 0;
    default:
        return ERR_INVALID_FUNCTION;
    }
}

static int device_name_handler(
    fake_t *f, uint8_t func, const hidpp_packet_t *req, hidpp_packet_t *res
) {
    size_t len = strlen(f->name);

    switch (func) {
    case 0:
        res->params[0] = (uint8_t)len;
        return 0;
    case 1: {
        size_t offset = req->params[0];
        size_t room = reply_capacity(res);

        if (offset > len)
            return ERR_OUT_OF_RANGE;

        size_t n = len - offset < room ? len - offset : room;
        memcpy(res->params, &f->name[offset], n);
        return 0;
    }
    case 2:
        res->params[0] = f->deviceType;
        return 0;
    default:
        return ERR_INVALID_FUNCTION;
    }
}

static int firmware_handler(
    fake_t *f, uint8_t func, const hidpp_packet_t *req, hidpp_packet_t *res
) {
    switch (func) {
    case 0:
        res->params[0] = f->fwEntityCount;
        memcpy(&res->params[1], f->fwUnitId, 4);
        res->params[5] = HIDPP_MSB(f->fwTransport);
        res->params[6] = HIDPP_LSB(f->fwTransport);
        memcpy(&res->params[7], f->fwModelId, 6);
        res->params[13] = f->fwExtModel;
        res->params[14] = f->fwCapabilities;
        return 0;
    case 1: {
        uint8_t id = req->params[0];

        if (id >= f->fwEntityCount || id >= FAKE_MAX_ENTITIES)
            return ERR_OUT_OF_RANGE;

        struct fake_entity *e = &f->fwEntities[id];
        res->params[0] = (uint8_t)(e->type | f->fwTypeHighBits);
        memcpy(&res->params[1], e->prefix, 3);
        res->params[4] = e->number;
        res->params[5] = e->revision;
        res->params[6] = HIDPP_MSB(e->build);
        res->params[7] = HIDPP_LSB(e->build);
        res->params[8] = e->active;
        res->params[9] = HIDPP_MSB(e->pid);
        res->params[10] = HIDPP_LSB(e->pid);
        memcpy(&res->params[11], e->extra, 5);
        return 0;
    }
    case 2:
        if (!(f->fwCapabilities & 1))
            return ERR_INVALID_FUNCTION;
        memcpy(res->params, f->fwSerial, 12);
        return 0;
    default:
        return ERR_INVALID_FUNCTION;
    }
}

static int nickname_handler(
    fake_t *f, uint8_t func, const hidpp_packet_t *req, hidpp_packet_t *res
) {
    size_t len = strnlen(f->nick, FAKE_NAME_MAX);
    size_t defLen = strnlen(f->nickDefault, FAKE_NAME_MAX);

    switch (func) {
    case 0:
        res->params[0] = (uint8_t)len;
        res->params[1] = f->nickMax;
        res->params[2] = (uint8_t)defLen;
        return 0;
    case 1:
    case 2: {
        const char *src = func == 1 ? f->nick : f->nickDefault;
        size_t srcLen = func == 1 ? len : defLen;
        size_t offset = req->params[0];

        if (offset > srcLen)
            return ERR_OUT_OF_RANGE;

        size_t n = srcLen - offset < 15 ? srcLen - offset : 15;
        res->params[0] = req->params[0]; /* echoes the offset */
        memcpy(&res->params[1], &src[offset], n);
        return 0;
    }
    case 3: {
        size_t offset = req->params[0];

        if (offset + 15 > FAKE_NAME_MAX || offset >= f->nickMax)
            return ERR_OUT_OF_RANGE;

        /* a short report would have lost the characters: refuse it */
        if (req->kind == HIDPP_KIND_SHORT)
            return ERR_INVALID_ARGUMENT;

        memcpy(&f->nick[offset], &req->params[1], 15);
        f->nick[FAKE_NAME_MAX] = '\0';
        res->params[0] = (uint8_t)strnlen(f->nick, FAKE_NAME_MAX);
        return 0;
    }
    case 4:
        strcpy(f->nick, f->nickDefault);
        res->params[0] = (uint8_t)strlen(f->nick);
        return 0;
    default:
        return ERR_INVALID_FUNCTION;
    }
}

struct fake_control *fake_control(fake_t *f, uint16_t cid) {
    for (unsigned i = 0; i < f->keymapCount; i++)
        if (f->controls[i].cid == cid)
            return &f->controls[i];
    return NULL;
}

static int keymap_handler(
    fake_t *f, uint8_t func, const hidpp_packet_t *req, hidpp_packet_t *res
) {
    switch (func) {
    case 0:
        res->params[0] = f->keymapCount;
        return 0;
    case 1: {
        if (req->params[0] >= f->keymapCount)
            return ERR_OUT_OF_RANGE;

        struct fake_control *c = &f->controls[req->params[0]];
        res->params[0] = HIDPP_MSB(c->cid);
        res->params[1] = HIDPP_LSB(c->cid);
        res->params[2] = HIDPP_MSB(c->tid);
        res->params[3] = HIDPP_LSB(c->tid);
        res->params[4] = c->flags;
        res->params[5] = c->position;
        res->params[6] = c->group;
        res->params[7] = c->groupMask;
        res->params[8] = c->flags2;
        return 0;
    }
    case 2: {
        uint16_t cid = HIDPP_WORD(req->params[0], req->params[1]);
        struct fake_control *c = fake_control(f, cid);

        if (!c)
            return ERR_INVALID_ARGUMENT;

        res->params[0] = HIDPP_MSB(c->cid);
        res->params[1] = HIDPP_LSB(c->cid);
        res->params[2] = (uint8_t)((c->divert ? 0x01 : 0)
                                   | (c->persist ? 0x04 : 0)
                                   | (c->rawXY ? 0x10 : 0));
        res->params[3] = HIDPP_MSB(c->remap);
        res->params[4] = HIDPP_LSB(c->remap);
        return 0;
    }
    case 3: {
        uint16_t cid = HIDPP_WORD(req->params[0], req->params[1]);
        struct fake_control *c = fake_control(f, cid);

        if (!c)
            return ERR_INVALID_ARGUMENT;

        uint8_t flags = req->params[2];

        /* every setting is only applied if its "valid" bit is set */
        if (flags & 0x02)
            c->divert = flags & 0x01;
        if (flags & 0x08)
            c->persist = !!(flags & 0x04);
        if (flags & 0x20)
            c->rawXY = !!(flags & 0x10);

        /* the target is only transported by long reports */
        if (req->kind != HIDPP_KIND_SHORT) {
            uint16_t remap = HIDPP_WORD(req->params[3], req->params[4]);
            c->remap = remap ? remap : c->cid;
        }

        memcpy(res->params, req->params, 5);
        return 0;
    }
    default:
        return ERR_INVALID_FUNCTION;
    }
}

/* ------------------------------------------------------------------ */
/* Request processing                                                 */
/* ------------------------------------------------------------------ */

static void queue_error(
    fake_t *f, const hidpp_packet_t *req, uint8_t feat, uint8_t code
) {
    hidpp_packet_t err;
    memset(&err, 0, sizeof(err));
    err.kind = (uint8_t)f->errorKind;
    err.device = req->device;
    err.feat = feat;
    err.func = req->feat;
    err.params[0] = req->func;
    err.params[1] = code;
    queue_packet(f, &err);
}

static void queue_noise(fake_t *f, const hidpp_packet_t *req) {
    hidpp_packet_t n;
    memset(&n, 0, sizeof(n));
    n.kind = HIDPP_KIND_LONG;
    n.device = req->device;
    n.feat = req->feat;
    n.func = req->func;

    if (f->noise & FAKE_NOISE_FOREIGN_REPORT) {
        const uint8_t foreign[] = { 0x02, 0x01, 0x00, 0x04,
                                    0x00, 0x00, 0x00, 0x00 };
        fake_queue_raw(f, foreign, sizeof(foreign));
    }

    if (f->noise & FAKE_NOISE_NOTIFY) {
        n.func = (uint8_t)(req->func & 0xF0);
        queue_packet(f, &n);
        n.func = req->func;
    }

    if (f->noise & FAKE_NOISE_OTHER_DEVICE) {
        n.device = req->device == 0xFF ? 1 : (uint8_t)(req->device + 1);
        queue_packet(f, &n);
        n.device = req->device;
    }

    if (f->noise & FAKE_NOISE_OTHER_SWID) {
        n.func = (uint8_t)((req->func & 0xF0) | ((req->func + 1) & 0x0F));
        queue_packet(f, &n);
        n.func = req->func;
    }

    if (f->noise & FAKE_NOISE_OTHER_FUNC) {
        n.func = (uint8_t)((((req->func >> 4) + 1) << 4) | (req->func & 0x0F));
        queue_packet(f, &n);
        n.func = req->func;
    }
}

static void process(fake_t *f, const uint8_t *raw, size_t len) {
    hidpp_packet_t req;
    memset(&req, 0, sizeof(req));
    req.kind = raw[0];
    req.device = raw[1];
    req.feat = raw[2];
    req.func = raw[3];
    memcpy(req.params, &raw[4], len - 4);

    if (f->requests < FAKE_MAX_LOG) {
        struct fake_packet *p = &f->log[f->requests];
        memset(p, 0, sizeof(*p));
        memcpy(p->raw, raw, len);
        p->len = len;
    }

    f->requests++;
    f->calls[req.feat]++;

    if (req.feat >= 1 && req.feat <= f->featureCount)
        f->features[req.feat - 1].calls++;

    if (f->silent || req.device != f->index)
        return;

    if (f->drop > 0) {
        f->drop--;
        return;
    }

    if (f->hidpp10) {
        queue_error(f, &req, 0x8F, 0x01); /* ERR_INVALID_SUBID */
        return;
    }

    queue_noise(f, &req);

    hidpp_packet_t res;
    memset(&res, 0, sizeof(res));
    res.kind = (uint8_t)f->replyKind;
    res.device = req.device;
    res.feat = req.feat;
    res.func = req.func;

    uint8_t func = (uint8_t)(req.func >> 4);
    int rc;

    if (req.feat == 0)
        rc = root_handler(f, func, &req, &res);
    else if (req.feat <= f->featureCount)
        rc = f->features[req.feat - 1].handler(f, func, &req, &res);
    else
        rc = ERR_INVALID_FEATURE_INDEX;

    if (rc == FAKE_SILENT)
        return;

    if (rc == 0 && f->garbage) {
        for (size_t k = 0; k < reply_capacity(&res); k++) {
            uint32_t x = f->garbageState ? f->garbageState : 0x2545F491u;
            x ^= x << 13;
            x ^= x >> 17;
            x ^= x << 5;
            f->garbageState = x;
            res.params[k] = (uint8_t)(x >> 11);
        }
    }

    if (rc > 0) {
        queue_error(f, &req, 0xFF, (uint8_t)rc);
        return;
    }

    res.func = (uint8_t)((req.func & 0xF0)
                         | ((req.func + f->swidOffset) & 0x0F));
    queue_packet(f, &res);
}

/* ------------------------------------------------------------------ */
/* Custom receiver callbacks                                          */
/* ------------------------------------------------------------------ */

static int fake_send(
    hidpp_receiver_t *rcv, const char *buffer, size_t size, void *user
) {
    (void)rcv;
    fake_t *f = user;

    if (f->failSend)
        return HIDPP_EIO;

    if (size < 4 || size > HIDPP_LEN_XLONG)
        return HIDPP_EIO;

    const uint8_t *raw = (const uint8_t *)buffer;

    /* the library must only ever write whole, valid reports */
    if (kind_length(raw[0]) != size)
        return HIDPP_EIO;

    process(f, raw, size);
    return HIDPP_OK;
}

static int fake_recv(
    hidpp_receiver_t *rcv, char *buffer, size_t size, void *user
) {
    (void)rcv;
    fake_t *f = user;

    if (f->failRecv)
        return HIDPP_EIO;

    if (f->head == f->tail)
        return 0; /* nothing arrived within the timeout */

    struct fake_packet *p = &f->queue[f->head % FAKE_MAX_QUEUE];
    size_t n = p->len < size ? p->len : size;
    memcpy(buffer, p->raw, n);
    f->head++;
    return (int)n;
}

/* ------------------------------------------------------------------ */
/* Construction                                                       */
/* ------------------------------------------------------------------ */

int fake_add_feature(
    fake_t *f, uint16_t id, uint8_t version, fake_handler_fn handler
) {
    if (f->featureCount >= FAKE_MAX_FEATURES)
        return -1;

    struct fake_feature *ft = &f->features[f->featureCount++];
    ft->id = id;
    ft->version = version;
    ft->type = 0;
    ft->handler = handler;
    return (int)f->featureCount; /* index 0 is the root */
}

int fake_add_battery(fake_t *f) {
    f->batLevel = 75;
    f->batNext = 50;
    f->batStatus = 0;
    f->batLevelCount = 4;
    f->batFlags = 0x06; /* mileage + rechargeable */
    f->batNominal = 3000;
    f->batCritical = 5;
    return fake_add_feature(f, 0x1000, 3, battery_handler);
}

int fake_add_device_name(fake_t *f) {
    strcpy(f->name, "MX Master 3S");
    f->deviceType = HIDPP_TYPE_MOUSE;
    return fake_add_feature(f, 0x0005, 0, device_name_handler);
}

int fake_add_firmware_info(fake_t *f) {
    f->fwEntityCount = 3;
    memcpy(f->fwUnitId, "\xDE\xAD\xBE\xEF", 4);
    f->fwTransport = 0x000D; /* USB + eQuad + Bluetooth */
    memcpy(f->fwModelId, "\x40\x82\xB0\x23\x00\x00", 6);
    f->fwExtModel = 0x03;
    f->fwCapabilities = 0x01; /* has a serial number */
    memcpy(f->fwSerial, "ABCDEF123456", 12);

    struct fake_entity *e = f->fwEntities;
    e[0] = (struct fake_entity) { .type = HIDPP_FIRMWARE_MAIN_APP,
                                  .prefix = { 'R', 'B', 'M' },
                                  .number = 0x12,
                                  .revision = 0x34,
                                  .build = 0x0567,
                                  .active = 1,
                                  .pid = 0xB023,
                                  .extra = { 1, 2, 3, 4, 5 } };
    e[1] = (struct fake_entity) { .type = HIDPP_FIRMWARE_BOOT_LOADER,
                                  .prefix = { 'B', 'L', '1' },
                                  .number = 0x03,
                                  .revision = 0x01,
                                  .build = 0x0010,
                                  .active = 1,
                                  .pid = 0xB023 };
    e[2] = (struct fake_entity) { .type = HIDPP_FIRMWARE_HARDWARE,
                                  .prefix = { 'H', 'W', 'R' },
                                  .number = 0x02,
                                  .revision = 0x00,
                                  .build = 0x0000,
                                  .active = 0,
                                  .pid = 0 };

    return fake_add_feature(f, 0x0003, 4, firmware_handler);
}

int fake_add_nickname(fake_t *f) {
    strcpy(f->nick, "Desk mouse");
    strcpy(f->nickDefault, "MX Master 3S");
    f->nickMax = 24;
    return fake_add_feature(f, 0x0007, 0, nickname_handler);
}

int fake_add_control(
    fake_t *f, uint16_t cid, uint16_t tid, uint8_t flags, uint8_t position
) {
    if (f->keymapCount >= FAKE_MAX_CONTROLS)
        return -1;

    struct fake_control *c = &f->controls[f->keymapCount];
    memset(c, 0, sizeof(*c));
    c->cid = cid;
    c->tid = tid;
    c->flags = flags;
    c->position = position;
    c->remap = cid;
    return (int)f->keymapCount++;
}

int fake_add_keymap(fake_t *f, uint16_t id) {
    /* flags: 0x01 mouse button, 0x10 reprogrammable, 0x20 divertable,
              0x40 persistable, 0x80 virtual */
    fake_add_control(f, 0x0050, 0x0038, 0x31, 0); /* left */
    fake_add_control(f, 0x0051, 0x0039, 0x31, 1); /* right */
    fake_add_control(f, 0x0052, 0x003A, 0x71, 2); /* middle */
    fake_add_control(f, 0x0053, 0x003C, 0x31, 3); /* back */
    fake_add_control(f, 0x00C3, 0x00B4, 0x30, 4); /* gesture */
    f->controls[4].group = 1;
    f->controls[4].groupMask = 0x03;
    f->controls[4].flags2 = 0x01; /* rawXY capable */
    return fake_add_feature(f, id, id == 0x1B04 ? 4 : 1, keymap_handler);
}

fake_t *fake_new_bare(void) {
    fake_t *f = calloc(1, sizeof(*f));

    if (!f)
        abort();

    f->index = 1;
    f->protoMajor = 4;
    f->protoMinor = 2;
    f->replyKind = HIDPP_KIND_LONG;
    f->errorKind = HIDPP_KIND_SHORT;
    f->featureSetCountOverride = -1;

    f->rcv = hidpp_open_custom(fake_send, fake_recv, f);

    if (!f->rcv)
        abort();

    fake_add_feature(f, 0x0001, 1, feature_set_handler);
    return f;
}

fake_t *fake_new(void) {
    fake_t *f = fake_new_bare();
    fake_add_firmware_info(f);  /* index 2 */
    fake_add_device_name(f);    /* index 3 */
    fake_add_nickname(f);       /* index 4 */
    fake_add_battery(f);        /* index 5 */
    fake_add_keymap(f, 0x1B04); /* index 6 */
    return f;
}

hidpp_device_t *fake_open(fake_t *f) {
    f->dev = hidpp_device_open(f->rcv, f->index);
    return f->dev;
}

void fake_free(fake_t *f) {
    if (!f)
        return;

    hidpp_close(f->rcv);
    free(f);
}
