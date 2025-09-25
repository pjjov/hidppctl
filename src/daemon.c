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

static struct {
    const char *dir;
    sig_atomic_t term;
    FILE *log;
    int lock;
    mqd_t mq;
} g_daemon;

#define QUEUE_NAME "hidppctl.mq"
#define LOCK_NAME "hidppctl.lock"
#define LOG_NAME "hidppctl.log"

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

    return mq_open(QUEUE_NAME, O_CREAT | O_RDWR, 0644, &attr);
}

void daemon_exit(int code) {
    log_printf("Shutting down!");
    mq_close(g_daemon.mq);
    mq_unlink(QUEUE_NAME);
    close(g_daemon.lock);
    unlink(LOCK_NAME);
    hid_exit();
    exit(code);
}
