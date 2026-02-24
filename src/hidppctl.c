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

static struct {
    struct pf_argparser *parser;
    const char *command;

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
        .usage = "usage: hidppctl info [OPTIONS]... <device path>\n"
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

    return HIDPP_OK;
}

int main(int argc, char *argv[]) {
    if (parse_args(argc, argv))
        return HIDPP_EINVAL;

    if (options.help) {
        pf_arghelp(options.parser, NULL);
        return HIDPP_OK;
    }

    return HIDPP_ENOSYS;
}
