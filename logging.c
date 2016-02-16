/*
 * Copyright (C) 2016 Ingemar Ådahl
 *
 * This file is part of remote-input.
 *
 * remote-input is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * remote-input is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with remote-input.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "logging.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <linux/limits.h>

#define CLAMP(val, low, high) (val > high ? high : (val < low ? low : val))

int log_level = LOG_NOTICE;

enum log_target selected_target = STDIO;

void log_set_level(int level) {
    log_level = CLAMP(level, LOG_ALERT, LOG_DEBUG);
    if (selected_target == SYSLOG) {
        setlogmask(LOG_UPTO(log_level));
    }
}

PRINTF_TYPE(2,3)
void stdio_log(int priority, const char* format, ...) {
    if (priority > log_level) {
        return;
    }

    va_list args;
    va_start(args, format);

    FILE* stream = priority < LOG_WARNING ? stderr : stdout;
    fflush(stdout);
    fflush(stderr);

    vfprintf(stream, format, args);
    fprintf(stream, "\n");

    va_end(args);
}

PRINTF_TYPE(2,3)
void(*__log_function)(int priority, const char* format, ...) = &stdio_log;

void log_set_target(enum log_target target) {
    if (selected_target == target) {
        return;
    }

    selected_target = target;

    if (selected_target == STDIO) {
        closelog();
        __log_function = &stdio_log;
        return;
    }

    if (selected_target == SYSLOG) {
        openlog(NULL, LOG_NDELAY | LOG_PID, LOG_USER);
        setlogmask(LOG_UPTO(log_level));
        __log_function = &syslog;
    }
}

void __log_errno(const char* file, int line, const char* format, ...) {
    char msg_buf[128];
    va_list args;
    va_start(args, format);
    vsnprintf(msg_buf, sizeof(msg_buf), format, args);
    va_end(args);

    if (file != NULL && line >= 0) {
        LOG(ERROR, "%s:%d: %s: %s", file, line, msg_buf, strerror(errno));
        return;
    }

    LOG(ERROR, "%s: %s", msg_buf, strerror(errno));
}
