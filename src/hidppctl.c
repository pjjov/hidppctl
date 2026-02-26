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
    const char *receiver;
    const char *device;
    const char *timeout;

    uint8_t devId;
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
        { "-r, --receiver", "Specifies which HID++ receiver to use." },
        { "-d, --device", "Specifies which HID++ device index to use." },
        { "--timeout", "Sets the timeout for IO operations in milliseconds." },
        { 0 },
    };

    static struct pf_option mainDef[] = {
        { "help", '?', PF_OPT_BOOL, &options.help },
        { "timeout", 0, PF_OPT_STR, &options.timeout },
        { "receiver", 'r', PF_OPT_STR, &options.receiver },
        { "device", 'd', PF_OPT_STR, &options.device },
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
        .description = "\nShows information about HID++ devices.\n\nOptions:\n",
        .usage = "usage: hidppctl [OPTIONS]... info\n",
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

static hidpp_device_t *open_device(hidpp_receiver_t *rcv, uint8_t index) {
    hidpp_device_t *dev;
    if (!(dev = hidpp_device_open(rcv, index)))
        errorf("Unable to open device: %ls", hidpp_error(rcv));
    return dev;
}

int cmd_info_all(void) {
    struct hidpp_receiver_info *info, all[32];
    size_t len = hidpp_enumerate(0, 0, all, 32);

    if (len == 0) {
        printf("No HID++ receivers found!\n");
        return HIDPP_OK;
    }

    for (size_t i = 0; i < len; i++) {
        info = &all[i];

        printf(
            "HID++ receiver '%ls' from '%ls'\n",
            info->product,
            info->manufacturer
        );
        printf(
            "  ID: %.4x:%.4x (%s)\n",
            info->vendorId,
            info->productId,
            info->path
        );

        hidpp_free_info(info);
    }

    return HIDPP_OK;
}

static int parse_device_index(const char *arg, uint8_t *out) {
    char *end;
    long i = strtol(arg, &end, 0);

    if (end == arg || i < 1 || (i > 6 && i != 0xFF))
        return HIDPP_EINVAL;

    *out = i;
    return HIDPP_OK;
}

static int cmd_info_rcv(void) {
    hidpp_receiver_t *rcv = open_receiver(options.receiver);

    hidpp_device_t *dev;
    struct hidpp_receiver_info info;
    struct hidpp_device_info dinfo;

    if (!rcv || hidpp_receiver_info(rcv, &info))
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
        printf("  Device %d connected '%s'\n", i, dinfo.name);
        printf("    Version: %u.%u\n", dinfo.major, dinfo.minor);
        printf("    Type: %u\n", dinfo.type);

        hidpp_device_close(dev);
    }

    hidpp_free_info(&info);
    hidpp_close(rcv);
    return HIDPP_OK;
}

static int cmd_info_dev(void) {
    hidpp_receiver_t *rcv = open_receiver(options.receiver);
    hidpp_device_t *dev = open_device(rcv, options.devId);
    struct hidpp_device_info info;

    if (!rcv || !dev)
        return HIDPP_EIO;

    if (hidpp_device_info(dev, &info)) {
        errorf("Unable to read device information: %ls", hidpp_error(rcv));
        hidpp_device_close(dev);
        hidpp_close(rcv);
        return HIDPP_EIO;
    }

    printf("Device %d connected '%s'\n", info.index, info.name);
    printf("  Version: %u.%u\n", info.major, info.minor);
    printf("  Type: %u\n", info.type);

    hidpp_device_close(dev);
    hidpp_close(rcv);
    return HIDPP_OK;
}

static int cmd_info_features(void) {
    hidpp_receiver_t *rcv = open_receiver(options.receiver);
    hidpp_device_t *dev = open_device(rcv, options.devId);
    struct hidpp_device_info info;

    if (!rcv || !dev)
        return HIDPP_EIO;

    if (hidpp_device_info(dev, &info)) {
        errorf("Unable to read device information: %ls", hidpp_error(rcv));
        hidpp_device_close(dev);
        hidpp_close(rcv);
        return HIDPP_EIO;
    }

    printf("Device supports %u HID++ features:\n", info.numFeatures);

    for (int i = 0; i < info.numFeatures; i++) {
        uint16_t feat = hidpp_feature_id(dev, i);
        printf("  [0x%.4x] %s\n", feat, hidpp_feature_name(feat));
    }

    hidpp_device_close(dev);
    hidpp_close(rcv);
    return HIDPP_OK;
}

static int cmd_info_keymap(void) {
    hidpp_receiver_t *rcv = open_receiver(options.receiver);
    hidpp_device_t *dev = open_device(rcv, options.devId);
    hidpp_keymap_t *map = hidpp_keymap(dev);
    struct hidpp_keymap_info info;

    if (!rcv || !dev)
        return HIDPP_EIO;

    if (!map || hidpp_keymap_info(map, &info)) {
        printf("Selected device doesn't support keymap features!\n");
        return HIDPP_OK;
    }

    printf("Device has %u remappable controls:\n", info.numControls);

    for (int i = 0; i < info.numControls; i++) {
        uint16_t ctrl = hidpp_keymap_id(map, i);
        printf("  [0x%.4x] %s\n", ctrl, hidpp_keymap_name(ctrl));
    }

    hidpp_device_close(dev);
    hidpp_close(rcv);
    return HIDPP_OK;
}

static int cmd_info(void) {
    if (options.argc == 1) {
        if (0 == strcmp(options.argv[0], "keymap"))
            return cmd_info_keymap();
        if (0 == strcmp(options.argv[0], "features"))
            return cmd_info_features();

        errorf(
            "Unsupported argument '%s'; Valid values are:\n"
            "'keymap', 'features'.",
            options.argv[0]
        );
        return HIDPP_EINVAL;
    } else if (options.argc > 0) {
        errorf("Subcommand 'info' takes 1 argument; found %d", options.argc);
        return HIDPP_EINVAL;
    }

    if (options.receiver == NULL)
        return cmd_info_all();
    if (options.devId != 0)
        return cmd_info_dev();

    return cmd_info_rcv();
}

int main(int argc, char *argv[]) {
    if (parse_args(argc, argv))
        return HIDPP_EINVAL;

    if (options.help) {
        pf_arghelp(options.parser, NULL);
        return HIDPP_OK;
    }

    if (options.device && parse_device_index(options.device, &options.devId)) {
        errorf("Invalid device index; must be 256 or between 1 and 6.");
        return HIDPP_EIO;
    }

    if (hidpp_init(NULL)) {
        errorf("Unable to initialize the hidpp library!");
        return HIDPP_EIO;
    }

    int result = HIDPP_ENOSYS;

    if (0 == strcmp(options.command, "info"))
        result = cmd_info();

    hidpp_exit();
    return result;
}
