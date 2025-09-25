/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2025 Предраг Јовановић
    SPDX-FileCopyrightText: 2025 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "hidpp.h"
#include "protocol.h"
#include <hidapi.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SWID 4

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
        "usage: hidppctl [--help] <subcommand> [...]\n"
        "       hidppctl info <vendor id> <product id> [...]\n"
        "       hidppctl info <raw device path> [...]\n"
        "       hidppctl start\n"
        "       hidppctl stop\n"
        "\n"
        "subcommands:\n"
        "  info        search for and show information of HID++ devices\n"
        "\n"
        "options:\n"
        "  -c, --config <path>    use the following configuration file\n"
        "  -h, --help             show this help message and exit\n"
        "  --version              show program's version and exit\n"
        "\n"
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

void print_device(hidpp_device *dev) {
    uint16_t version;
    char buf[256];

    version = hidpp_version(dev);
    hidpp_device_name(dev, buf, 256);

    printf("Found HID++ device '%.256s':\n", buf);
    printf("\tID: %d\n", hidpp_device_id(dev));
    printf("\tHID++ version: %d.%d\n", version >> 8, version & 0xF);

    printf("\t%d available features:\n", hidpp_feat_count(dev));
    for (int i = 0; i < hidpp_feat_count(dev); i++) {
        uint16_t id = hidpp_feat_id(dev, i);
        printf("\t\t[0x%.4x] %s\n", id, hidpp_feat_name(id));
    }

    printf("\t%d reprogrammable buttons:\n", hidpp_button_count(dev));
    for (int i = 0; i < hidpp_button_count(dev); i++) {
        uint16_t id = hidpp_button_id(dev, i);
        printf("\t\t[0x%.4x] %s\n", id, hidpp_button_name(id));
    }
}

int print_handle(hid_device *handle) {
    int found = 0;

    for (int i = 1; i <= 6; i++) {
        hidpp_device *dev = hidpp_open(handle, i, SWID);
        if (dev) {
            print_device(dev);
            hidpp_close(dev);
            found = 1;
        }
    }

    return found;
}

static void print_from_id(int vid, int pid) {
    hid_device *handle = hid_open(vid, pid, NULL);
    if (!handle) {
        printf("Unable to open HID device %d:%d.\n", vid, pid);
        return;
    }

    if (print_handle(handle))
        printf("Found a HID++ device for HID %d:%d!\n", vid, pid);
    hid_close(handle);
}

static void print_from_path(const char *path) {
    hid_device *handle = hid_open_path(path);
    if (!handle) {
        printf("Unable to open HID device '%s'.\n", path);
        return;
    }

    if (print_handle(handle))
        printf("Found a HID++ device for HID '%s'!\n", path);
    hid_close(handle);
}

static void print_each() {
    struct hid_device_info *info = hid_enumerate(0, 0);
    if (!info)
        errorf("No HID devices found!");

    for (; info; info = info->next)
        print_from_path(info->path);

    hid_free_enumeration(info);
}

void cmd_info() {
    if (g_args.paramc == 1)
        print_each();
    else if (g_args.paramc == 2)
        print_from_path(g_args.argv[1]);
    else {
        const char *vendor = g_args.argv[1];
        const char *product = g_args.argv[2];
        char *err;
        int vid = strtol(vendor, &err, 16);
        if (err == vendor)
            errorf("HID device vendor id '%s:%s' is not valid.", vendor);

        int pid = strtol(product, &err, 16);
        if (err == product)
            errorf("HID device product id '%s:%s' is not valid.", product);
        print_from_id(vid, pid);
    }
}

/* daemon.c */
extern void cmd_start(void);
extern int send_message(struct message *msg);

void cmd_stop(void) {
    struct message msg;
    msg.kind = MSG_SHUTDOWN;
    if (send_message(&msg))
        errorf("Unable to stop the daemon!");
    else
        printf("Stopped the daemon!");
}

int cmd_run(const char *name) {
    static struct {
        const char *name;
        void (*handler)(void);
    } commands[] = {
        { "info", cmd_info },
        { "start", cmd_start },
        { "stop", cmd_stop },
        { 0 },
    };

    for (int i = 0; commands[i].name; i++) {
        if (0 == strcmp(name, commands[i].name)) {
            commands[i].handler();
            return HIDPP_OK;
        }
    }

    return HIDPP_EINVAL;
}

int main(int argc, const char *argv[]) {
    parse_args(argc, argv);
    if (g_args.failed)
        return HIDPP_EINVAL;

    if (g_args.help) {
        print_help();
        return HIDPP_OK;
    }

    if (g_args.version) {
        puts("hidppctl 1.0");
        return HIDPP_OK;
    }

    if (hid_init())
        errorf("Unable to initialize 'hidapi'!");

    if (g_args.paramc < 1) {
        print_help();
        return HIDPP_EINVAL;
    }

    if (cmd_run(g_args.argv[0]))
        errorf("Unknown subcommand '%s'!", g_args.argv[0]);

    hid_exit();
    return 0;
}
