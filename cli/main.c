/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "common.h"

#include <pf_argparse.h>

extern int cmd_cache(hidppctl_t *ctl);
extern int cmd_divert(hidppctl_t *ctl);
extern int cmd_keymap(hidppctl_t *ctl);
extern int cmd_list_events(hidppctl_t *ctl);
extern int cmd_list_features(hidppctl_t *ctl);
extern int cmd_list_keycodes(hidppctl_t *ctl);
extern int cmd_poll(hidppctl_t *ctl);
extern int cmd_remap(hidppctl_t *ctl);
extern int cmd_status(hidppctl_t *ctl);

extern void hidppctl_load_cache(hidppctl_t *ctl);
extern void hidppctl_save_cache(hidppctl_t *ctl);

extern void hidppctl_open_log(hidppctl_t *ctl);
extern void hidppctl_close_log(hidppctl_t *ctl);

hidpp_receiver_t *open_receiver(hidppctl_t *ctl, const char *name) {
    struct hidppctl_opt *opt = ctl->options;

    if (!name) {
        pf_cli_errorf(ctl->cli, "The HID++ receiver must be specified!");
        return NULL;
    }

    hidpp_receiver_t *rcv;

    char *colon, *end;
    long vid = strtol(name, &colon, 16);
    long pid = strtol(colon + 1, &end, 16);

    if (vid < 0 || pid < 0 || vid > UINT16_MAX || pid > UINT16_MAX) {
        pf_cli_errorf(
            ctl->cli,
            "Vendor and product ids must be between 0 and %u.",
            UINT16_MAX
        );
        return NULL;
    }

    if (*colon == ':' && (*end == ':' || *end == '\0')) {
        if (opt->setInterface)
            rcv = hidpp_open_interface(vid, pid, opt->interface);
        else
            rcv = hidpp_open(vid, pid, NULL);
    } else {
        rcv = hidpp_open_path(name);
    }

    if (rcv == NULL)
        pf_cli_errorf(ctl->cli, "Unable to open receiver '%s'.", name);

    if (opt->setTimeout)
        hidpp_set_timeout(rcv, opt->timeout);
    if (opt->setSwid)
        hidpp_set_swid(rcv, opt->swid);
    return rcv;
}

static hidpp_device_t *open_device(
    hidppctl_t *ctl, hidpp_receiver_t *rcv, uint8_t index
) {
    hidpp_device_t *dev;

    if (index == 0) {
        pf_cli_errorf(ctl->cli, "The HID++ device must be specified!");
        return NULL;
    }

    if (!(dev = hidpp_device_open(rcv, index)))
        pf_cli_errorf(ctl->cli, "Unable to open device: %ls", hidpp_error(rcv));
    return dev;
}

static void hidppctl_free(hidppctl_t *ctl) {
    if (ctl->options->requiresCache)
        hidppctl_save_cache(ctl);
    if (ctl->options->requiresLog)
        hidppctl_close_log(ctl);
    if (ctl->options->requiresInput)
        hidpp_input_free(ctl->input);
    if (ctl->options->requiresDevice)
        hidpp_device_close(ctl->device);
    if (ctl->options->requiresReceiver)
        hidpp_close(ctl->receiver);
    if (ctl->cachePath)
        free(ctl->cachePath);
    if (ctl->logPath)
        free(ctl->logPath);
    hidpp_exit();
}

static int hidppctl_init(hidppctl_t *ctl) {
    if (hidpp_init(NULL)) {
        pf_cli_errorf(ctl->cli, "Unable to initialize the hidpp library!");
        return HIDPP_EIO;
    }

    if (ctl->options->requiresInput) {
        if (!(ctl->input = hidpp_input_new(NULL))) {
            pf_cli_errorf(ctl->cli, "Unable to simulate input!");
            hidppctl_free(ctl);
            return HIDPP_EIO;
        }
    }

    if (ctl->options->requiresReceiver) {
        ctl->receiver = open_receiver(ctl, ctl->options->receiver);

        if (!ctl->receiver) {
            hidppctl_free(ctl);
            return HIDPP_EIO;
        }
    }

    if (ctl->options->requiresDevice) {
        ctl->device = open_device(ctl, ctl->receiver, ctl->options->device);

        if (!ctl->device) {
            hidppctl_free(ctl);
            return HIDPP_EIO;
        }
    }

    if (ctl->options->requiresCache)
        hidppctl_load_cache(ctl);

    if (ctl->options->requiresLog)
        hidppctl_open_log(ctl);

    return HIDPP_OK;
}

static int hidppctl_run(hidppctl_t *ctl) {
    int rc;

    if ((rc = hidppctl_parse_args(ctl)))
        return rc;

    if (ctl->options->help)
        return hidppctl_print_help(ctl);

    if ((rc = hidppctl_init(ctl)))
        return rc;

    int result = HIDPP_ENOSYS;

    switch (ctl->options->command) {
        /* clang-format off */
        PF_ENUM_GUARD(hidppctl_command, 2);
    case HIDPPCTL_CACHE:         result = cmd_cache(ctl);         break;
    case HIDPPCTL_DIVERT:        result = cmd_divert(ctl);        break;
    case HIDPPCTL_KEYMAP:   result = cmd_keymap(ctl);   break;
    case HIDPPCTL_LIST_EVENTS:   result = cmd_list_events(ctl);   break;
    case HIDPPCTL_LIST_FEATURES: result = cmd_list_features(ctl); break;
    case HIDPPCTL_LIST_KEYCODES: result = cmd_list_keycodes(ctl); break;
    case HIDPPCTL_POLL:          result = cmd_poll(ctl);          break;
    case HIDPPCTL_REMAP:         result = cmd_remap(ctl);         break;
    case HIDPPCTL_STATUS:        result = cmd_status(ctl);        break;
    default:                     result = HIDPP_ENOSYS;           break;
        /* clang-format off */
    }

    hidppctl_free(ctl);
    return result;
}

int main(int argc, char *argv[]) {
    struct hidppctl_opt options = {0};
    pf_argparser_t argparser = {0};
    pf_cli_t cli = {0};

    hidppctl_t state = {0};
    state.options = &options;
    state.argparser = &argparser;
    state.cli = &cli;

    pf_cli_init(state.cli, NULL);
    pf_argparser_init(state.argparser, argc, argv);

    int rc = hidppctl_run(&state);

    pf_argparser_free(state.argparser);
    return rc;
}