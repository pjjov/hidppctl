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

/* daemon.c */
extern void daemon_start(void);
extern int send_message(struct message *msg);

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
        "                info <vendor id> <product id> [...]\n"
        "                info <raw device path> [...]\n"
        "                start\n"
        "                stop\n"
        "                pair <raw device path>\n"
        "                pair <vendor id> <product id>\n"
        "                divert <button name or id> <key sequence>\n"
        "                refresh"
        "\n"
        "subcommands:\n"
        "  info        search for and show information of HID++ devices\n"
        "  start       start the daemon server for controlling devices\n"
        "  stop        stops the daemon server\n"
        "  pair        pairs the HID receiver and daemon server\n"
        "  divert      maps a button to the specified X11 keysequence\n"
        "  refresh     refreshes device configuration\n"
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

static void print_from_id(int vid, int pid, const wchar_t *serial) {
    hid_device *handle = hid_open(vid, pid, serial);
    if (!handle) {
        printf("Unable to open HID device %.4x:%.4x.\n", vid, pid);
        return;
    }

    if (print_handle(handle))
        printf("Found a HID++ device for HID %.4x:%.4x!\n", vid, pid);
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
        print_from_id(info->vendor_id, info->product_id, info->serial_number);

    hid_free_enumeration(info);
}

void parse_ids(const char *vendor, const char *product, int *vid, int *pid) {
    char *err;
    *vid = strtol(vendor, &err, 16);
    if (err == vendor)
        errorf("HID device vendor id '%s' is not valid.", vendor);

    *pid = strtol(product, &err, 16);
    if (err == product)
        errorf("HID device product id '%s' is not valid.", product);
}

void cmd_info() {
    if (g_args.paramc == 1)
        print_each();
    else if (g_args.paramc == 2)
        print_from_path(g_args.argv[1]);
    else {
        int vid, pid;
        parse_ids(g_args.argv[1], g_args.argv[2], &vid, &pid);
        print_from_id(vid, pid, NULL);
    }
}

void cmd_start(void) { daemon_start(); }

void cmd_stop(void) {
    struct message msg;
    msg.kind = MSG_SHUTDOWN;
    if (send_message(&msg))
        errorf("Unable to stop the daemon!");
    else
        printf("hidppctl: Stopped the daemon!\n");
}

void cmd_pair(void) {
    struct message msg;

    if (g_args.paramc == 2) {
        size_t len = strlen(g_args.argv[2]);
        if (len + 1 > sizeof(msg.as.path))
            errorf("Path too long!");
        msg.kind = MSG_PAIR_PATH;
        memcpy(msg.as.path, g_args.argv[2], len + 1);
    } else if (g_args.paramc == 3) {
        int vid, pid;
        parse_ids(g_args.argv[1], g_args.argv[2], &vid, &pid);
        msg.kind = MSG_PAIR_ID;
        msg.as.id.vendor = vid;
        msg.as.id.product = pid;
    } else {
        print_help();
        return;
    }

    if (send_message(&msg))
        errorf("Unable to pair device!");
    else
        printf("hidppctl: Sent the pairing request to the server!\n");
}

void cmd_divert(void) {
    if (g_args.paramc != 3) {
        print_help();
        return;
    }

    struct message msg;
    msg.kind = MSG_DIVERT;
    uint16_t ctrlid = hidpp_button_from_name(g_args.argv[1]);
    size_t len = strlen(g_args.argv[2]);

    if (!ctrlid && !(ctrlid = strtol(g_args.argv[1], NULL, 0)))
        errorf("Unknown HID++ button '%s'.", g_args.argv[1]);

    if (len + 1 > sizeof(msg.as.divert.keyseq))
        errorf("Key sequence '%s' is too long!", g_args.argv[2]);

    msg.as.divert.ctrlid = ctrlid;
    memcpy(msg.as.divert.keyseq, g_args.argv[2], len + 1);

    if (send_message(&msg))
        errorf("Unable to divert button; daemon is not running!");
    else
        printf("hidppctl: Sent the diversion request to the server!\n");
}

void cmd_refresh(void) {
    printf("hidppctl: Refreshing device configuration!");
    struct message msg = { MSG_REFRESH };

    if (send_message(&msg))
        errorf("Unable to refresh configuration; daemon is not running!");
    else
        printf("hidppctl: Sent the refresh request to the server!\n");
}

int cmd_run(const char *name) {
    static struct {
        const char *name;
        void (*handler)(void);
    } commands[] = {
        { "info", cmd_info },
        { "start", cmd_start },
        { "stop", cmd_stop },
        { "pair", cmd_pair },
        { "divert", cmd_divert },
        { "refresh", cmd_refresh },
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
