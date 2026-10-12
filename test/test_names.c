/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

/*  Feature and control name lookups backed by the generated tables. */

#include "test.h"

static int test_feature_known(int seed, int i) {
    (void)seed;
    (void)i;
    T_STR(hidpp_feature_name(0x0000), "Root feature");
    T_STR(hidpp_feature_name(0x0001), "Feature set");
    T_STR(hidpp_feature_name(0x0003), "Device firmware information");
    T_STR(hidpp_feature_name(0x0005), "Device name");
    T_STR(hidpp_feature_name(0x0007), "Device friendly name");
    T_STR(hidpp_feature_name(0x1000), "Battery status");
    T_STR(hidpp_feature_name(0x1B04), "Reprogrammable controls 4.0");
    return 0;
}

static int test_feature_unknown(int seed, int i) {
    (void)seed;
    (void)i;
    T_STR(hidpp_feature_name(0xFFFF), "Unknown HID++ feature");
    T_STR(hidpp_feature_name(0x7777), "Unknown HID++ feature");
    return 0;
}

static int test_feature_table_consistent(int seed, int i) {
    (void)seed;
    (void)i;
    T_TRUE(hidpp_constant_features_count > 50);

    for (size_t n = 0; n < hidpp_constant_features_count; n++) {
        const struct hidpp_constant *c = &hidpp_constant_features[n];
        T_NOT_NULL(c->name);
        T_TRUE(c->name[0] != '\0');
        /* the lookup returns the entry of the table it was derived from */
        T_STR(hidpp_feature_name(c->code), c->name);
    }
    return 0;
}

static int test_feature_codes_unique(int seed, int i) {
    (void)seed;
    (void)i;

    for (size_t a = 0; a < hidpp_constant_features_count; a++)
        for (size_t b = a + 1; b < hidpp_constant_features_count; b++)
            T_NE(
                hidpp_constant_features[a].code, hidpp_constant_features[b].code
            );
    return 0;
}

static int test_control_known(int seed, int i) {
    (void)seed;
    (void)i;
    T_STR(hidpp_keymap_name(0x0001), "Volume_Up");
    T_STR(hidpp_keymap_name(0x0050), "Left_Button");
    T_STR(hidpp_keymap_name(0x0051), "Right_Button");
    T_STR(hidpp_keymap_name(0x0052), "Middle_Button");
    T_STR(hidpp_keymap_name(0x00C3), "Mouse_Gesture_Button");
    return 0;
}

static int test_control_unknown(int seed, int i) {
    (void)seed;
    (void)i;
    T_STR(hidpp_keymap_name(0xFFFF), "Unknown HID++ button");
    T_STR(hidpp_keymap_name(0x0000), "Unknown HID++ button");
    return 0;
}

static int test_control_table_roundtrip(int seed, int i) {
    (void)seed;
    (void)i;
    T_TRUE(hidpp_constant_controls_count > 100);

    for (size_t n = 0; n < hidpp_constant_controls_count; n++) {
        const struct hidpp_constant *c = &hidpp_constant_controls[n];
        T_NOT_NULL(c->name);
        T_STR(hidpp_keymap_name(c->code), c->name);
        T_EQ(hidpp_keymap_from_name(c->name), c->code);
    }
    return 0;
}

static int test_control_codes_unique(int seed, int i) {
    (void)seed;
    (void)i;

    for (size_t a = 0; a < hidpp_constant_controls_count; a++)
        for (size_t b = a + 1; b < hidpp_constant_controls_count; b++)
            T_NE(
                hidpp_constant_controls[a].code, hidpp_constant_controls[b].code
            );
    return 0;
}

static int test_control_names_unique(int seed, int i) {
    (void)seed;
    (void)i;

    /* from_name() could not be unambiguous otherwise */
    for (size_t a = 0; a < hidpp_constant_controls_count; a++)
        for (size_t b = a + 1; b < hidpp_constant_controls_count; b++)
            T_TRUE(
                strcmp(
                    hidpp_constant_controls[a].name,
                    hidpp_constant_controls[b].name
                )
                != 0
            );
    return 0;
}

static int test_from_name_unknown(int seed, int i) {
    (void)seed;
    (void)i;
    T_ERR(hidpp_keymap_from_name("No_Such_Button"), HIDPP_ENOENT);
    T_ERR(hidpp_keymap_from_name(""), HIDPP_ENOENT);
    return 0;
}

static int test_from_name_null(int seed, int i) {
    (void)seed;
    (void)i;
    T_ERR(hidpp_keymap_from_name(NULL), HIDPP_EINVAL);
    return 0;
}

static int test_from_name_case_sensitive(int seed, int i) {
    (void)seed;
    (void)i;
    T_EQ(hidpp_keymap_from_name("Left_Button"), 0x0050);
    T_ERR(hidpp_keymap_from_name("left_button"), HIDPP_ENOENT);
    T_ERR(hidpp_keymap_from_name("Left_Button "), HIDPP_ENOENT);
    return 0;
}

static int test_from_name_double_underscore(int seed, int i) {
    (void)seed;
    (void)i;
    /* names are taken verbatim from data/controls.txt */
    T_EQ(hidpp_keymap_from_name("Play__Pause"), 0x0004);
    return 0;
}

static const pf_test_t names_tests[] = {
    { test_feature_known, "feature_known" },
    { test_feature_unknown, "feature_unknown" },
    { test_feature_table_consistent, "feature_table_consistent" },
    { test_feature_codes_unique, "feature_codes_unique" },
    { test_control_known, "control_known" },
    { test_control_unknown, "control_unknown" },
    { test_control_table_roundtrip, "control_table_roundtrip" },
    { test_control_codes_unique, "control_codes_unique" },
    { test_control_names_unique, "control_names_unique" },
    { test_from_name_unknown, "from_name_unknown" },
    { test_from_name_null, "from_name_null" },
    { test_from_name_case_sensitive, "from_name_case_sensitive" },
    { test_from_name_double_underscore, "from_name_double_underscore" },
    { 0 },
};

const pf_suite_t suite_names = { "names", 1, names_tests };
