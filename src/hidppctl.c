/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2025 Предраг Јовановић
    SPDX-FileCopyrightText: 2025 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include <hidapi.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

void errorf(const char *fmt, ...) {
    printf("hidppctl: ");

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    fputc('\n', stdout);

    hid_exit();
    abort();
}

int main() {
    if (hid_init())
        errorf("Unable to initialize 'hidapi'!");

    hid_exit();
    return 0;
}
