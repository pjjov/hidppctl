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

#define HIDPP_MAX_DIVERT 32
#define HIDPP_MAX_MODS 8

struct diversion {
    uint16_t ctrl;
    char state;
    int key;
    int count;
    int mods[HIDPP_MAX_MODS];
} diversions[HIDPP_MAX_DIVERT] = { 0 };

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

    static struct pf_option_info divertInfo[] = {
        { "-?, --help", "Shows this information." },
        { 0 },
    };

    static struct pf_option divertDef[] = {
        { "help", '?', PF_OPT_BOOL, &options.help },
        { 0 },
    };

    static struct pf_argparser divertParser = {
        .name = "hidppctl divert",
        .description
        = "\nDiverts specified device's buttons and prints associated events."
          "\nYou can also rebind device's buttons using an argument like this:"
          "\n    '<button code>=<key code>[+<modifier>]' "
          "\nFor example: '0x0104=home+lshift'."
          "\n\nOptions:\n",
        .usage = "usage: hidppctl [OPTIONS]... divert [buttons]...\n",
        .errorInfo = errorInfo,
        .epilog = epilog,
        .infos = divertInfo,
        .options = divertDef,
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
    if (0 == strcmp(cmd, "divert"))
        options.parser = &divertParser;

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

    if (!map || hidpp_keymap_info(map, &info, 0)) {
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

extern struct {
    const char name[12];
    int code;
} hidpp_input_table[];

static int cmd_info_keycodes(void) {
    printf("Aside from numeric values, key codes also have following aliases:");

    const char *columns = getenv("COLUMNS");
    size_t max = 0;

    if (columns)
        max = strtol(columns, NULL, 0);
    if (max == 0)
        max = 80;

    size_t length = max;
    for (size_t i = 0; hidpp_input_table[i].code; i++) {
        size_t curr = strlen(hidpp_input_table[i].name) + 4;
        length += curr;

        if (length >= max) {
            printf("\n  ");
            length = 2 + curr;
        }

        printf("'%s', ", hidpp_input_table[i].name);
    }

    putc('\n', stdout);
    return HIDPP_OK;
}

static int cmd_info(void) {
    if (options.argc == 1) {
        if (0 == strcmp(options.argv[0], "keymap"))
            return cmd_info_keymap();
        if (0 == strcmp(options.argv[0], "features"))
            return cmd_info_features();
        if (0 == strcmp(options.argv[0], "keycodes"))
            return cmd_info_keycodes();

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

static char *find_next_chr(char *str, char chr) {
    char *out = strchr(str, chr);
    if (out)
        *out = '\0';
    return out;
}

static int parse_diversion_ctrl(char *str, uint16_t *out) {
    char *end;
    long ctrl = strtol(str, &end, 0);

    if (end == str) {
        ctrl = hidpp_keymap_from_name(str);

        if (ctrl == 0) {
            errorf("Unknown control name '%s'", str);
            return HIDPP_EINVAL;
        }
    }

    if (ctrl < 0 || ctrl > UINT16_MAX) {
        errorf(
            "Control codes must be between 0 and %u; got %ld", UINT16_MAX, ctrl
        );
        return HIDPP_EINVAL;
    }

    *out = ctrl;
    return HIDPP_OK;
}

static int parse_diversion_keys(char *str, int i) {
    char *start = str;
    char *end;
    int count = -1;

    do {
        if (count > HIDPP_MAX_MODS) {
            errorf("Too many modifiers!");
            return HIDPP_ENOMEM;
        }

        end = find_next_chr(start, '+');
        int key = hidpp_input_key(start);

        if (key < 0) {
            errorf("Unknown key code '%s'.", start);
            return HIDPP_EINVAL;
        }

        if (count == -1)
            diversions[i].key = key;
        else
            diversions[i].mods[count] = key;

        count++;
        start = end + 1;
    } while (end);

    diversions[i].count = count;
    return HIDPP_OK;
}

static int parse_diversion(int i, char *arg) {
    char *ctrlEnd = find_next_chr(arg, '=');

    if (parse_diversion_ctrl(arg, &diversions[i].ctrl))
        return HIDPP_EINVAL;

    if (ctrlEnd && parse_diversion_keys(&ctrlEnd[1], i))
        return HIDPP_EINVAL;

    return HIDPP_OK;
}

static int cmd_divert_init(hidpp_keymap_t *map) {
    if (map) {
        errorf("Specified device doesn't support diversion!");
        return HIDPP_EIO;
    }

    for (int i = 0; i < options.argc; i++) {
        uint16_t ctrl = diversions[i].ctrl;

        if (hidpp_keymap_divert(map, ctrl, HIDPP_TRUE)) {
            errorf("Unable to divert the control with id 0x%.4x!", ctrl);
            return HIDPP_EIO;
        }
    }

    return HIDPP_OK;
}

static int cmd_divert_term(hidpp_keymap_t *map) {
    for (int i = 0; i < options.argc; i++) {
        uint16_t ctrl = diversions[i].ctrl;

        if (hidpp_keymap_divert(map, ctrl, HIDPP_FALSE))
            errorf("Unable to undivert the control with id 0x%.4x!", ctrl);
    }

    return HIDPP_OK;
}

static void cmd_divert_set(struct diversion *div, int input, int value) {
    if (div->state == value)
        return;

    hidpp_input_set(input, div->key, div->mods, div->count, value);
    div->state = value;
}

static int cmd_divert_poll(
    hidpp_device_t *dev, hidpp_keymap_t *map, int input
) {
    struct hidpp_event e;

    if (hidpp_device_poll(dev, &e))
        return HIDPP_EIO;

    for (int i = 0; i < options.argc; i++) {
        struct diversion *div = &diversions[i];
        int value = 0;

        for (int i = 0; i < 4; i++)
            if (e.as.diverted[i] == div->ctrl)
                value = 1;

        if (value != div->state)
            cmd_divert_set(div, input, value);
    }

    return HIDPP_OK;
}

static int cmd_divert_loop(int needsInput) {
    int input = -1;

    if (needsInput && -1 == (input = hidpp_input_open())) {
        errorf("Unable to simulate input!");
        return HIDPP_EIO;
    }

    hidpp_receiver_t *rcv = open_receiver(options.receiver);
    hidpp_device_t *dev = open_device(rcv, options.devId);
    hidpp_keymap_t *map = hidpp_keymap(dev);

    if (!dev || !rcv || cmd_divert_init(map))
        return HIDPP_EIO;

    cmd_divert_poll(dev, map, input);

    cmd_divert_term(map);
    hidpp_device_close(dev);
    hidpp_close(rcv);

    if (needsInput)
        hidpp_input_close(input);
    return HIDPP_OK;
}

static int cmd_divert(void) {
    if (options.argc == 0)
        return HIDPP_EINVAL;

    if (options.argc > HIDPP_MAX_DIVERT) {
        errorf("Too many diversions specified!");
        return HIDPP_ENOMEM;
    }

    if (!options.receiver || options.devId == 0) {
        errorf("Subcommand requires both a receiver and a device specified!");
        return HIDPP_EINVAL;
    }

    int hasInput = HIDPP_FALSE;

    for (int i = 0; i < options.argc; i++) {
        if (parse_diversion(i, options.argv[i]))
            return HIDPP_EINVAL;
        if (diversions[i].key)
            hasInput = HIDPP_TRUE;
    }

    return cmd_divert_loop(hasInput);
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
    else if (0 == strcmp(options.command, "divert"))
        result = cmd_divert();

    hidpp_exit();
    return result;
}
