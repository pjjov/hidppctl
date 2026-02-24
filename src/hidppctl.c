/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include <hidpp.h>
#include <pf_argparse.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static struct {
    struct pf_argparser *parser;
    const char *command;

    char **argv;
    int argc;

    char help;
    const char *timeout;
} options = { 0 };

static void errorf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fputs("hidppctl: ", stderr);
    vfprintf(stderr, fmt, args);
    fputc('\n', stderr);
    va_end(args);
}

int parse_args(int argc, char *argv[]) {
    static const char errorInfo[] = "Run `hidppctl --help' for more "
                                    "information\n";
    static const char epilog[] = "\nFor more information, run `man nanodoc'.\n";

    static struct pf_option_info mainInfo[] = {
        { "-?, --help", "Shows this information." },
        { "--timeout", "Sets the timeout for IO operations in milliseconds." },
        { 0 },
    };

    static struct pf_option mainDef[] = {
        { "help", '?', PF_OPT_BOOL, &options.help },
        { "timeout", 0, PF_OPT_STR, &options.timeout },
        { 0 },
    };

    static struct pf_argparser mainParser = {
        .name = "hidppctl",
        .description = "\nConfigure HID++ compatible devices.\n\nOptions:\n",
        .usage = "usage: hidppctl [OPTIONS]... <command>\n",
        .errorInfo = errorInfo,
        .epilog = "\nSubcommands:\n"
                  "  info            shows information about HID++ devices.\n"
                  "\nFor more information, run `man nanodoc'.\n",
        .infos = mainInfo,
        .options = mainDef,
        .stopAtFirst = PF_ARGPARSE_TRUE,
    };

    static struct pf_option_info infoInfo[] = {
        { "-?, --help", "Shows this information." },
        { 0 },
    };

    static struct pf_option infoDef[] = {
        { "help", '?', PF_OPT_BOOL, &options.help },
        { 0 },
    };

    static struct pf_argparser infoParser = {
        .name = "hidppctl info",
        .description = "\nShows information about HID++ devices\n\nOptions:\n",
        .usage = "usage: hidppctl info [OPTIONS]...\n"
                 "       hidppctl info [OPTIONS]... <device path>\n"
                 "       hidppctl info [OPTIONS]... <vendor_id:product_id>\n",
        .errorInfo = errorInfo,
        .epilog = epilog,
        .infos = infoInfo,
        .options = infoDef,
        .parent = &mainParser
    };

    if (pf_argparse(&mainParser, argc, argv) < 0)
        return HIDPP_EINVAL;

    options.parser = &mainParser;

    if (mainParser.argc == 0) {
        options.help = PF_ARGPARSE_TRUE;
        return HIDPP_OK;
    }

    options.command = mainParser.argv[0];
    const char *cmd = options.command;

    if (0 == strcmp(cmd, "info"))
        options.parser = &infoParser;

    if (options.parser == &mainParser) {
        errorf("Unknown subcommand '%s'!", cmd);
        fprintf(stderr, "%s", mainParser.errorInfo);
        return HIDPP_EINVAL;
    }

    if (pf_argparse(options.parser, mainParser.argc, &argv[1]) < 0)
        return HIDPP_EINVAL;

    options.argc = options.parser->argc;
    options.argv = options.parser->argv;
    return HIDPP_OK;
}

hidpp_receiver_t *open_receiver(const char *name) {
    hidpp_receiver_t *rcv;

    char *colon, *end;
    long vid = strtol(name, &colon, 16);
    long pid = strtol(colon + 1, &end, 16);

    if (vid < 0 || pid < 0 || vid > UINT16_MAX || pid > UINT16_MAX) {
        errorf("Vendor and product ids must be between 0 and %u.", UINT16_MAX);
        return NULL;
    }

    if (*colon == ':' && (*end == ':' || *end == '\0'))
        rcv = hidpp_open(vid, pid, NULL);
    else
        rcv = hidpp_open_path(name);

    if (rcv == NULL)
        errorf("Unable to open receiver '%s'.", name);
    return rcv;
}

int cmd_info_all(void) { return HIDPP_ENOSYS; }

int cmd_info(void) {
    if (options.argc == 0)
        return cmd_info_all();

    hidpp_receiver_t *rcv;
    hidpp_device_t *dev;
    struct hidpp_receiver_info info;
    struct hidpp_device_info dinfo;

    if (!(rcv = open_receiver(options.argv[0])))
        return HIDPP_EIO;

    if (hidpp_receiver_info(rcv, &info))
        return HIDPP_EIO;

    printf("HID++ receiver '%ls'\n", info.product);
    printf("  ID: %.4x:%.4x (%s)\n", info.vendorId, info.productId, info.path);
    printf("  Serial number: %ls\n", info.serial);
    printf("  Manufacturer: %ls\n", info.manufacturer);
    printf("  Release number %u\n", info.releaseNumber);
    printf("  Usage and page: %u, %u\n", info.usage, info.usagePage);
    printf("  Interface: %d\n", info.interfaceNumber);

    for (int i = 1; i < 7; i++) {
        if (!(dev = hidpp_device_open(rcv, i))) {
            printf("  Device %d disconnected\n", i);
            continue;
        }

        hidpp_device_info(dev, &dinfo);
        printf("  Device '%s'\n", dinfo.name);
        printf("    Version: %u.%u\n", dinfo.major, dinfo.minor);
        printf("    Type: %u\n", dinfo.type);

        hidpp_device_close(dev);
    }

    hidpp_close(rcv);
    return HIDPP_OK;
}

int main(int argc, char *argv[]) {
    if (parse_args(argc, argv))
        return HIDPP_EINVAL;

    if (options.help) {
        pf_arghelp(options.parser, NULL);
        return HIDPP_OK;
    }

    if (hidpp_init()) {
        errorf("Unable to initialize the hidpp library!");
        return HIDPP_EIO;
    }

    int result = HIDPP_ENOSYS;

    if (0 == strcmp(options.command, "info"))
        result = cmd_info();

    hidpp_exit();
    return result;
}
