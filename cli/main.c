/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "common.h"

#include <pf_argparse.h>
#include <pf_process.h>
#include <pf_socket.h>
#include <time.h>

static pf_sock_t open_tcp_socket(const char *host, int port) {
    pf_addrinfo_t hints, *list, *ai;
    pf_sock_t sock = PF_INVALID_SOCKET;
    char service[16];

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(service, sizeof(service), "%d", port);

    if (pf_sock_init() != 0)
        return PF_INVALID_SOCKET;

    if (pf_getaddrinfo(host, service, &hints, &list) != 0)
        return PF_INVALID_SOCKET;

    for (ai = list; ai; ai = ai->ai_next) {
        sock = pf_sock_open(ai->ai_family, ai->ai_socktype, ai->ai_protocol);

        if (sock == PF_INVALID_SOCKET)
            continue;

        if (pf_sock_connect(sock, ai->ai_addr, (pf_socklen_t)ai->ai_addrlen)
            == 0)
            break;

        pf_sock_close(sock);
        sock = PF_INVALID_SOCKET;
    }

    pf_freeaddrinfo(list);

    if (sock != PF_INVALID_SOCKET)
        pf_sock_set_nodelay(sock, 1);
    return sock;
}

#ifndef _WIN32

    #include <sys/un.h>

static pf_sock_t open_domain_socket(const char *path) {
    pf_sock_t sock = pf_sock_open(AF_UNIX, SOCK_STREAM, 0);

    if (sock == PF_INVALID_SOCKET)
        return PF_INVALID_SOCKET;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;

    if (strlen(path) >= sizeof(addr.sun_path)) {
        pf_sock_close(sock);
        return PF_INVALID_SOCKET;
    }

    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    if (pf_sock_connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        pf_sock_close(sock);
        return PF_INVALID_SOCKET;
    }

    return sock;
}

#endif

extern void hidppctl_load_cache(hidppctl_t *ctl);
extern void hidppctl_save_cache(hidppctl_t *ctl);

extern void hidppctl_open_log(hidppctl_t *ctl);
extern void hidppctl_close_log(hidppctl_t *ctl);

hidpp_receiver_t *open_receiver(hidppctl_t *ctl, const char *name) {
    struct hidppctl_opt *opt = ctl->options;

    if (!name) {
        pf_cli_errorf(ctl->cli, "The HID++ receiver must be specified!\n");
        return NULL;
    }

    hidpp_receiver_t *rcv;

    char *colon, *end;
    long vid = strtol(name, &colon, 16);
    long pid = strtol(colon + 1, &end, 16);

    if (vid < 0 || pid < 0 || vid > UINT16_MAX || pid > UINT16_MAX) {
        pf_cli_errorf(
            ctl->cli,
            "Vendor and product ids must be between 0 and %u.\n",
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

    if (rcv == NULL) {
        pf_cli_errorf(ctl->cli, "Unable to open receiver '%s'.\n", name);
        return NULL;
    }

    if (opt->setTimeout)
        hidpp_set_timeout(rcv, opt->timeout);
    if (opt->setSwid)
        hidpp_set_swid(rcv, opt->swid);
    return rcv;
}

hidpp_receiver_t *open_socket(hidppctl_t *ctl, struct hidppctl_opt *opt) {
    pf_sock_t sock = PF_INVALID_SOCKET;
    hidpp_receiver_t *rcv;

    if (opt->socket.isPort) {
        sock = open_tcp_socket(opt->socket.host, opt->socket.port);

        if (sock == PF_INVALID_SOCKET) {
            pf_cli_errorf(
                ctl->cli,
                "Unable to connect to '%s' port %d.\n",
                opt->socket.host,
                opt->socket.port
            );
            return NULL;
        }
    } else {
#ifndef _WIN32
        sock = open_domain_socket(opt->socket.path);
#endif

        if (sock == PF_INVALID_SOCKET) {
            pf_cli_errorf(
                ctl->cli, "Unable to connect to '%s'.\n", opt->socket.path
            );
            return NULL;
        }
    }

    if (!(rcv = hidpp_open_socket((int)sock))) {
        pf_cli_errorf(ctl->cli, "Unable to open a receiver using a socket.\n");
        pf_sock_close(sock);
        return NULL;
    }

    if (opt->setTimeout)
        hidpp_set_timeout(rcv, opt->timeout);
    if (opt->setSwid)
        hidpp_set_swid(rcv, opt->swid);

    ctl->socket = (int)sock;
    return rcv;
}

/* Did opening the device fail because it is not connected (yet)? */
static int device_may_connect(hidpp_receiver_t *rcv) {
    switch (hidpp_status(rcv)) {
    case HIDPP_ETIMEDOUT:
    case HIDPP_ENODATA:
        return 1;
    case HIDPP_EIO:
        switch (hidpp_error_code(rcv)) {
        case HIDPP_ERR_BUSY:
        case HIDPP_ERR_HW_ERROR:
            return 1;
        case HIDPP_ERR_NONE:
            /* a stale hidapi handle; other receivers cannot recover */
            return hidpp_reopen(rcv) != HIDPP_ENOSYS;
        default:
            return 0;
        }
    default:
        return 0;
    }
}

static hidpp_device_t *open_device(
    hidppctl_t *ctl, hidpp_receiver_t *rcv, uint8_t index
) {
    hidpp_device_t *dev;

    if (index == 0) {
        pf_cli_errorf(ctl->cli, "The HID++ device must be specified!\n");
        return NULL;
    }

    /* A sleeping wireless device doesn't answer until something wakes it up.
       Only failures which can pass are retried; anything else is reported. */
    if (ctl->options->wait) {
        time_t start = time(NULL);

        cli_catch_signals();
        pf_cli_verbosef(ctl->cli, 1, "Waiting for device %d...\n", index);

        while (!(dev = hidpp_device_open(rcv, index)) && !cli_terminated()) {
            if (!device_may_connect(rcv))
                break;

            if (ctl->options->wait > 0
                && time(NULL) - start >= ctl->options->wait)
                break;

            sleep(1);
            hidpp_reopen(rcv);
        }
    } else {
        dev = hidpp_device_open(rcv, index);
    }

    if (!dev) {
        pf_cli_errorf(
            ctl->cli, "Unable to open device: %ls\n", hidpp_error(rcv)
        );
    }

    return dev;
}

static void hidppctl_free(hidppctl_t *ctl) {
    if (ctl->requiresCache && ctl->device)
        hidppctl_save_cache(ctl);
    if (ctl->requiresLog)
        hidppctl_close_log(ctl);
    if (ctl->input)
        hidpp_input_free(ctl->input);
    if (ctl->device)
        hidpp_device_close(ctl->device);
    if (ctl->receiver)
        hidpp_close(ctl->receiver);
    if (ctl->requiresSocket && ctl->socket)
        pf_sock_close((pf_sock_t)ctl->socket);
    if (ctl->cachePath)
        free(ctl->cachePath);
    if (ctl->logPath)
        free(ctl->logPath);
    hidpp_exit();
}

static int hidppctl_init(hidppctl_t *ctl) {
    if (hidpp_init(NULL)) {
        pf_cli_errorf(ctl->cli, "Unable to initialize the hidpp library!\n");
        return HIDPP_EIO;
    }

    if (ctl->requiresInput) {
        ctl->input = ctl->options->ydotool ? hidpp_input_new_ydotool(NULL, NULL)
                                           : hidpp_input_new(NULL);

        if (!ctl->input) {
            pf_cli_errorf(ctl->cli, "Unable to simulate input!\n");
            hidppctl_free(ctl);
            return HIDPP_EIO;
        }
    }

    if (ctl->requiresReceiver) {
        ctl->receiver = open_receiver(ctl, ctl->options->receiver);

        if (!ctl->receiver) {
            hidppctl_free(ctl);
            return HIDPP_EIO;
        }
    } else if (ctl->requiresSocket) {
        ctl->receiver = open_socket(ctl, ctl->options);

        if (!ctl->receiver) {
            hidppctl_free(ctl);
            return HIDPP_EIO;
        }
    }

    if (ctl->requiresDevice) {
        ctl->device = open_device(ctl, ctl->receiver, ctl->options->device);

        if (!ctl->device) {
            hidppctl_free(ctl);
            return HIDPP_EIO;
        }
    }

    if (ctl->requiresCache)
        hidppctl_load_cache(ctl);

    if (ctl->requiresLog)
        hidppctl_open_log(ctl);

    return HIDPP_OK;
}

static int hidppctl_run(hidppctl_t *ctl) {
    int rc;

    if ((rc = hidppctl_parse_args(ctl)))
        return rc;

    if (ctl->options->version)
        return HIDPP_OK;

    if (ctl->options->help)
        return hidppctl_print_help(ctl);

    if ((rc = hidppctl_init(ctl)))
        return rc;

    int result = hidppctl_commands[ctl->options->command].run(ctl);

    hidppctl_free(ctl);
    return result;
}

int main(int argc, char *argv[]) {
    struct hidppctl_opt options = { 0 };
    pf_argparser_t argparser = { 0 };
    pf_cli_t cli = { 0 };

    hidppctl_t state = { 0 };
    state.options = &options;
    state.argparser = &argparser;
    state.cli = &cli;

    pf_cli_init(state.cli, NULL);
    pf_argparser_init(state.argparser, argc, argv);

    int rc = hidppctl_run(&state);

    pf_argparser_free(state.argparser);
    return rc ? EXIT_FAILURE : EXIT_SUCCESS;
}