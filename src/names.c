/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include <hidpp.h>

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int compare_name(const void *l, const void *r) {
    const struct hidpp_constant *_l = l, *_r = r;
    return _l->code - _r->code;
}

static const char *find_constant_name(
    const struct hidpp_constant *constants, size_t count, const char *fallback
) {
    const char *result = bsearch(
        &index, constants, count, sizeof(struct hidpp_constant), compare_name
    );

    return result ? result : fallback;
}

static int find_constant_code(
    const struct hidpp_constant *constants, size_t count, const char *name
) {
    if (!name)
        return HIDPP_EINVAL;

    for (size_t i = 0; i < count; i++)
        if (0 == strcmp(constants[i].name, name))
            return constants[i].code;
    return HIDPP_ENOENT;
}

const char *hidpp_feature_name(uint16_t index) {
    return find_constant_name(
        hidpp_constant_features,
        hidpp_constant_features_count,
        "Unknown HID++ feature"
    );
}

const char *hidpp_keymap_name(uint16_t index) {
    return find_constant_name(
        hidpp_constant_controls,
        hidpp_constant_controls_count,
        "Unknown HID++ button"
    );
}

int hidpp_keymap_from_name(const char *name) {
    return find_constant_code(
        hidpp_constant_controls, hidpp_constant_controls_count, name
    );
}
