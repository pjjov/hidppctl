/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

/*  Test runner. All suites live in one executable so that they share the
    fake device; every suite is additionally registered as its own meson test.

        meson test -C build                     # everything
        build/test/hidpp-tests                  # everything, verbose
        build/test/hidpp-tests keymap           # one suite
        build/test/hidpp-tests keymap:remap     # one test
        build/test/hidpp-tests --list           # what exists
        build/test/hidpp-tests --seed 1234      # reproduce a fuzz failure
*/

#include "test.h"

static const pf_suite_t *all[] = {
    &suite_packet,   &suite_names,       &suite_receiver,
    &suite_hook,     &suite_device,      &suite_root,
    &suite_firmware, &suite_device_name, &suite_nickname,
    &suite_battery,  &suite_keymap,      &suite_poll,
    &suite_cache,    &suite_robustness,  &suite_hosts,
    &suite_backlight, &suite_illumination, &suite_action,
    &suite_retry,
    NULL,
};

int main(int argc, char **argv) {
    /* pf_test_main wants a contiguous, zero-terminated array of suites */
    size_t count = 0;
    while (all[count])
        count++;

    pf_suite_t *suites = calloc(count + 1, sizeof(*suites));

    if (!suites)
        return 2;

    for (size_t i = 0; i < count; i++)
        suites[i] = *all[i];

    int rc = pf_test_main(argc, argv, suites);
    free(suites);
    return rc;
}
