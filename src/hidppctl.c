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
#include <string.h>

static struct {
    int paramc;
    int argc;
    const char **argv;
    unsigned int failed : 1;

    unsigned int help : 1;
    unsigned int info : 1;
    unsigned int version : 1;
    const char *config;
} g_args = { 0 };

static void errorf(const char *fmt, ...) {
    fprintf(stderr, "hidppctl: ");

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fputc('\n', stderr);

    hid_exit();
    exit(1);
}

static void print_help() {
    printf(
        "usage: hidppctl [--help] [...]\n"
        "       hidppctl <vendor id> <product id> [...]\n"
        "       hidppctl <raw device path> [...]\n"
        "options:\n"
        "  -c, --config <path>    use the following configuration file\n"
        "  -i, --info             show device information and exit\n"
        "  -h, --help             show this help message and exit\n"
        "  --version              show program's version and exit\n\n"
        "hidppctl  Copyright (C) 2025  Предраг Јовановић\n"
    );
}

static int parse_long(int a) {
    const char *arg = g_args.argv[a];

    if (0 == strcmp(arg, "--help"))
        g_args.help = 1;
    else if (0 == strcmp(arg, "--version"))
        g_args.version = 1;
    else if (0 == strcmp(arg, "--config")) {
        if (a + 1 >= g_args.argc) {
            errorf("command-line option '--config' requires a path\n");
            g_args.failed = 1;
        }

        g_args.config = g_args.argv[a + 1];
        return 1;
    } else {
        errorf("unknown command-line option: %s\n", arg);
        g_args.failed = 1;
    }

    return 0;
}

static int parse_short(int a) {
    int out = 0;

    for (int i = a + 1; g_args.argv[i]; i++) {
        const char *arg = g_args.argv[i];

        switch (arg[i]) {
        case 'h':
            g_args.help = 1;
            break;
        case 'i':
            g_args.info = 1;
            break;
        case 'c':
            if (a + ++out >= g_args.argc) {
                errorf("command-line option '--config' requires a path\n");
                g_args.failed = 1;
            }

            g_args.config = g_args.argv[a + out];
            break;
        }
    }

    return out;
}

static void parse_args(int argc, const char *argv[]) {
    g_args.argc = argc;
    g_args.argv = argv;
    int i = 1, paramc = 0;

    for (; i < argc && argv[i]; i++) {
        const char *arg = argv[i];

        if (arg[0] == '-' && arg[1] == '-' && arg[2] == '\0')
            break;
        else if (arg[0] == '-' && arg[1] == '-')
            i += parse_long(i);
        else if (arg[0] == '-' && arg[2] != '\0')
            i += parse_short(i);
        else
            argv[paramc++] = arg;
    }

    for (; i < argc; i++)
        argv[paramc++] = argv[i];

    g_args.paramc = paramc;
}

static void device_from_id(int vid, int pid) { }

static void device_from_path(const char *path) { }

static void device_each() { }

int main(int argc, const char *argv[]) {
    parse_args(argc, argv);
    if (g_args.failed)
        return -1;

    if (g_args.help) {
        print_help();
        return 0;
    }

    if (g_args.version) {
        puts("hidppctl 1.0");
        return 0;
    }

    if (hid_init())
        errorf("Unable to initialize 'hidapi'!");

    if (g_args.paramc < 0 || g_args.paramc > 2)
        print_help();
    else if (g_args.paramc == 0)
        device_each();
    else if (g_args.paramc == 1)
        device_from_path(g_args.argv[0]);
    else if (g_args.paramc == 2) {
        device_from_id(
            strtol(g_args.argv[0], NULL, 0), strtol(g_args.argv[1], NULL, 0)
        );
    }

    hid_exit();
    return 0;
}
