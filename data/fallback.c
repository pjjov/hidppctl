/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include <stddef.h>
#include <stdint.h>

/*
    This file provides fallback arrays when 'embed_tables' meson option is
    disabled since they are exposed through the 'hidpp.h' header file.
*/

struct hidpp_constant {
    uint16_t code;
    const char *name;
};

const struct hidpp_constant hidpp_constant_controls[1] = { 0 };
const size_t hidpp_constant_controls_count = 0;

const struct hidpp_constant hidpp_constant_features[1] = { 0 };
const size_t hidpp_constant_features_count = 0;
