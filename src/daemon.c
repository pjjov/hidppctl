/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2025 Предраг Јовановић
    SPDX-FileCopyrightText: 2025 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "hidapi.h"
#include "hidpp.h"
#include "protocol.h"

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <fcntl.h>
#include <mqueue.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define HANDLE_MAX 4
#define DEVICE_MAX 16
#define DIVERT_MAX 16

static struct {
    const char *dir;
    sig_atomic_t term;
    FILE *log;
    int lock;
    mqd_t mq;

    hid_device *handles[HANDLE_MAX];
    hidpp_device *devices[DEVICE_MAX];
    struct diversion diversions[DIVERT_MAX];
    int handleCount;
    int deviceCount;
    int divertCount;
} g_daemon;

#define QUEUE_NAME "/hidppctl.mq"
#define LOCK_NAME "hidppctl.lock"
#define LOG_NAME "hidppctl.log"
#define SWID 5

static void errorf(const char *fmt, ...) {
    fprintf(stderr, "hidppctl: ");

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fputc('\n', stderr);
}

static void log_printf(const char *fmt, ...) {
    if (!g_daemon.log)
        return;

    fprintf(g_daemon.log, "%lu: ", time(NULL));

    va_list args;
    va_start(args, fmt);
    vfprintf(g_daemon.log, fmt, args);
    va_end(args);

    fputc('\n', g_daemon.log);
    fflush(g_daemon.log);
}

static void daemonize(const char *dir, const char *file) {
    int err = 0;
    pid_t pid;

    if ((pid = fork()) > 0) {
        puts("hidppctl: Started the hidppctl daemon!");
        exit(HIDPP_OK);
    }

    /* start the daemon process */
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
    err = err || pid < 0;
    err = err || setsid() < 0;
    err = err || (pid = fork()) < 0;
    if (err || pid > 0)
        exit(err ? HIDPP_EINVAL : HIDPP_OK);

    umask(0);
    chdir(dir);

    /* close open file descriptors */
    for (int fd = sysconf(_SC_OPEN_MAX); fd >= 0; fd--)
        close(fd);

    /* open standard streams */
    int i = open("/dev/null", O_RDWR);
    dup(i);
    dup(i);

    /* single instance lock */
    int lock = open(file, O_RDWR | O_CREAT, 0640);
    if (lock < 0)
        exit(HIDPP_EIO);

    if (lockf(lock, F_TLOCK, 0) < 0)
        exit(HIDPP_OK);

    /* write the process id to the lock file */
    dprintf(lock, "%d\n", getpid());
    g_daemon.lock = lock;
    g_daemon.term = 0;
}

void signal_handler(int sig) {
    switch (sig) {
    case SIGHUP:
        break;

    case SIGTERM:
        g_daemon.term = 1;
        break;
    }
}

mqd_t open_queue(void) {
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(struct message);
    attr.mq_curmsgs = 0;

    int flags = O_CREAT | O_RDONLY | O_NONBLOCK;
    return mq_open(QUEUE_NAME, flags, 0644, &attr);
}

void daemon_exit(int code) {
    log_printf("Shutting down!");

    for (int i = 0; i < g_daemon.deviceCount; i++)
        hidpp_close(g_daemon.devices[i]);

    for (int i = 0; i < g_daemon.handleCount; i++)
        hid_close(g_daemon.handles[i]);

    mq_close(g_daemon.mq);
    mq_unlink(QUEUE_NAME);
    close(g_daemon.lock);
    unlink(LOCK_NAME);
    hid_exit();
    exit(code);
}

int daemon_pair(struct message *msg) {
    hid_device *handle;

    if (msg->kind == MSG_PAIR_PATH)
        handle = hid_open_path(msg->as.path);
    else
        handle = hid_open(msg->as.id.vendor, msg->as.id.product, NULL);

    if (!handle) {
        if (msg->kind == MSG_PAIR_PATH)
            log_printf("ERROR: Unable to pair to receiver '%s'", msg->as.path);
        else {
            log_printf(
                "ERROR: Unable to pair to receiver %.4x:%.4x",
                msg->as.id.vendor,
                msg->as.id.product
            );
        }

        return HIDPP_EINVAL;
    }

    if (g_daemon.handleCount >= HANDLE_MAX) {
        log_printf("ERROR: Too many paired receivers!");
        return HIDPP_ENOMEM;
    }

    g_daemon.handles[g_daemon.handleCount++] = handle;
    for (int i = 1; i < 6; i++) {
        if (g_daemon.deviceCount >= DEVICE_MAX) {
            log_printf("ERROR: Too many paired devices!");
            return HIDPP_ENOMEM;
        }

        hidpp_device *dev = hidpp_open(handle, i, SWID);
        if (dev) {
            char buffer[257];
            hidpp_device_name(dev, buffer, 257);
            log_printf("Paired device '%s'", buffer);
            g_daemon.devices[g_daemon.deviceCount++] = dev;
        }
    }

    return HIDPP_OK;
}

int daemon_divert(struct message *msg) {
    if (g_daemon.divertCount >= DIVERT_MAX) {
        log_printf("ERROR: Too many diverted buttons!");
        return HIDPP_ENOMEM;
    }

    g_daemon.diversions[g_daemon.divertCount++] = msg->as.divert;
    return HIDPP_OK;
}

int daemon_handle_message() {
    struct message msg;
    ssize_t read = mq_receive(g_daemon.mq, (char *)&msg, sizeof(msg), 0);
    if (read == -1)
        return HIDPP_EAGAIN;

    if (read != sizeof(msg)) {
        log_printf(
            "ERROR: incomplete message received (%lu/%lu bytes)",
            read,
            sizeof(msg)
        );

        return HIDPP_EIO;
    }

    log_printf("Received message kind %d", msg.kind);

    switch (msg.kind) {
    case MSG_SHUTDOWN:
        g_daemon.term = 1;
        break;
    case MSG_PAIR_ID:
    case MSG_PAIR_PATH:
        daemon_pair(&msg);
        break;
    }

    return HIDPP_OK;
}

void daemon_poll(void) { daemon_handle_message(); }

void cmd_start(void) {
    const char *dir = getenv("XDG_RUNTIME_DIR");
    daemonize(dir ? dir : "/tmp", LOCK_NAME);

    if (!(g_daemon.log = fopen(LOG_NAME, "w")))
        daemon_exit(HIDPP_EIO);

    if (-1 == (g_daemon.mq = open_queue())) {
        log_printf("ERROR: Unable to open the message queue!");
        daemon_exit(HIDPP_EIO);
    }

    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTERM, signal_handler);
    signal(SIGHUP, signal_handler);

    g_daemon.handleCount = 0;
    g_daemon.deviceCount = 0;
    g_daemon.divertCount = 0;

    while (!g_daemon.term) {
        daemon_poll();
        sleep(1);
    }

    daemon_exit(HIDPP_OK);
}

void make_path(char *out, const char *file) {
    const char *dir;
    char path[PATH_MAX];

    if (!(dir = getenv("XDG_RUNTIME_DIR")))
        dir = "/tmp";

    size_t len = strlen(dir);
    memcpy(path, dir, len);
    path[len] = '/';
    memcpy(&path[len + 1], file, strlen(file));
}

int send_message(struct message *msg) {
    mqd_t mq = mq_open(QUEUE_NAME, O_WRONLY | O_NONBLOCK, 0, NULL);
    if (mq == -1) {
        errorf("Unable to open the daemon message queue.\n");
        return HIDPP_EIO;
    }

    if (mq_send(mq, (char *)msg, sizeof(struct message), 0)) {
        errorf("Unable to send a message to the daemon.\n");
        mq_close(mq);
        return HIDPP_EIO;
    }

    mq_close(mq);
    return HIDPP_OK;
}
