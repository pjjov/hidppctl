/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "../common.h"

#include <ctype.h>

#define MAX_RECEIVERS 32

struct found {
    const struct hidpp_receiver_info *info;
    int unique; /* vid:pid and interface are enough to pick the receiver */
    struct hidpp_device_info device;
};

static struct hidppctl_alias aliases[HIDPPCTL_MAX_ALIASES];
static int aliasCount;

static int alias_index(const char *name) {
    for (int i = 0; i < aliasCount; i++)
        if (0 == strcmp(aliases[i].name, name))
            return i;

    return -1;
}

static int name_is_valid(const char *name) {
    if (!*name || strlen(name) >= sizeof(aliases[0].name))
        return 0;

    for (; *name; name++)
        if (!isalnum((unsigned char)*name) && !strchr("_-.", *name))
            return 0;

    return 1;
}

/* "MX Master 3" -> "mx-master-3", made unique among the aliases. */
static void suggest_name(const char *name, char *out, size_t size) {
    char base[48];
    size_t n = 0;

    for (; name && *name && n < sizeof(base) - 4; name++) {
        if (isalnum((unsigned char)*name))
            base[n++] = (char)tolower((unsigned char)*name);
        else if (n && base[n - 1] != '-')
            base[n++] = '-';
    }

    while (n && base[n - 1] == '-')
        n--;
    if (!n)
        n = (size_t)snprintf(base, sizeof(base), "device");
    base[n] = '\0';

    snprintf(out, size, "%s", base);
    for (int i = 2; alias_index(out) >= 0; i++)
        snprintf(out, size, "%s-%d", base, i);
}

static void build_alias(const struct found *f, struct hidppctl_alias *a) {
    const struct hidpp_receiver_info *r = f->info;
    char buf[32];

    hidppctl_alias_init(a, NULL);

    if (f->unique) {
        snprintf(buf, sizeof(buf), "%.4x:%.4x", r->vendorId, r->productId);
        hidppctl_alias_set(a, "receiver", buf);
        snprintf(buf, sizeof(buf), "%d", r->interfaceNumber);
        hidppctl_alias_set(a, "interface", buf);
    } else {
        hidppctl_alias_set(a, "receiver", r->path);
    }

    snprintf(buf, sizeof(buf), "%u", f->device.index);
    hidppctl_alias_set(a, "device", buf);
}

static int ask_alias(hidppctl_t *ctl, const struct found *f) {
    pf_cli_t *cli = ctl->cli;
    struct hidppctl_alias a;
    char suggestion[64], answer[64];

    if (!pf_cli_confirm(cli, "Create an alias for this device?", 1))
        return 0;

    suggest_name(f->device.name, suggestion, sizeof(suggestion));

    for (;;) {
        char prompt[160];

        snprintf(prompt, sizeof(prompt), "Alias name [%s]", suggestion);

        if (!pf_cli_ask(cli, prompt, answer, sizeof(answer)))
            return 0;
        if (!answer[0])
            snprintf(answer, sizeof(answer), "%s", suggestion);

        if (!name_is_valid(answer)) {
            pf_cli_printf(
                cli, "Use 1 to 63 letters, digits, '_', '-' or '.'.\n"
            );
            continue;
        }

        if (alias_index(answer) >= 0
            && !pf_cli_confirm(cli, "That alias exists. Replace it?", 0))
            continue;

        break;
    }

    build_alias(f, &a);
    snprintf(a.name, sizeof(a.name), "%s", answer);

    int index = alias_index(answer);
    if (index < 0) {
        if (aliasCount >= HIDPPCTL_MAX_ALIASES) {
            pf_cli_errorf(cli, "Too many aliases.\n");
            return 0;
        }
        index = aliasCount++;
    }

    aliases[index] = a;

    if (hidppctl_alias_save(ctl, aliases, aliasCount))
        return 0;

    pf_cli_printf(
        cli, "Saved alias '%s'. Use it with --alias %s.\n", answer, answer
    );
    return 1;
}

static void print_hint(hidppctl_t *ctl, const struct found *f) {
    struct hidppctl_alias a;
    char line[512];

    build_alias(f, &a);
    hidppctl_alias_format(&a, line, sizeof(line));
    pf_cli_printf(
        ctl->cli, "  To create an alias: hidppctl alias set <name>%s\n", line
    );
}

static void handle_device(
    hidppctl_t *ctl, hidpp_device_t *dev, const struct found *f
) {
    pf_cli_t *cli = ctl->cli;
    const struct hidpp_receiver_info *r = f->info;

    pf_cli_printf(
        cli,
        "Found '%s' (%s)\n",
        cli_str(f->device.name),
        cli_device_type(f->device.type)
    );
    pf_cli_printf(
        cli,
        "  Receiver: %.4x:%.4x '%ls' (%s), interface %d, device %u\n",
        r->vendorId,
        r->productId,
        r->product ? r->product : L"",
        r->path,
        r->interfaceNumber,
        f->device.index
    );

    if (!cli->isInteractive) {
        print_hint(ctl, f);
        return;
    }

    ask_alias(ctl, f);

    if (pf_cli_confirm(cli, "Collect the cache for this device now?", 1))
        hidppctl_cache_collect(ctl, dev);
}

int cmd_search(hidppctl_t *ctl) {
    pf_cli_t *cli = ctl->cli;
    struct hidpp_receiver_info all[MAX_RECEIVERS];
    size_t count = hidpp_enumerate(0, 0, all, MAX_RECEIVERS);
    int total = 0;

    if (count == 0) {
        pf_cli_printf(cli, "No HID++ receivers found!\n");
        return HIDPP_OK;
    }

    if (hidppctl_alias_load(ctl, aliases, HIDPPCTL_MAX_ALIASES, &aliasCount))
        aliasCount = 0;

    for (size_t i = 0; i < count; i++) {
        struct hidpp_receiver_info *r = &all[i];
        hidpp_receiver_t *rcv = hidpp_open_path(r->path);
        int found = 0;

        if (!rcv) {
            pf_cli_verbosef(cli, 1, "Unable to open '%s'.\n", r->path);
            continue;
        }

        if (ctl->options->setTimeout)
            hidpp_set_timeout(rcv, ctl->options->timeout);

        struct found f = { .info = r, .unique = r->interfaceNumber >= 0 };

        for (size_t j = 0; j < count && f.unique; j++)
            if (j != i && all[j].vendorId == r->vendorId
                && all[j].productId == r->productId
                && all[j].interfaceNumber == r->interfaceNumber)
                f.unique = 0;

        /* Devices which are connected directly use the index 0xFF. */
        for (int index = 1; index <= 7; index++) {
            uint8_t device = index == 7 ? 0xFF : index;

            if (index == 7 && found)
                break;

            hidpp_device_t *dev = hidpp_device_open(rcv, device);
            if (!dev)
                continue;

            if (!hidpp_device_info(dev, &f.device)) {
                f.device.index = device;
                found++;
                total++;
                handle_device(ctl, dev, &f);
            }

            hidpp_device_close(dev);
        }

        hidpp_close(rcv);
    }

    for (size_t i = 0; i < count; i++)
        hidpp_free_info(&all[i]);

    if (total == 0)
        pf_cli_printf(cli, "No HID++ devices found!\n");
    return HIDPP_OK;
}
