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
#ifndef _LOGGING_H_
#define _LOGGING_H_

#include <stddef.h>
#include <syslog.h>

#ifdef __GNUC__
#define __notnull(args) __attribute__((__nonnull__ args))
#define PRINTF_TYPE(x, y) \
    __attribute__((__format__(__printf__, x, y))) __notnull((x))
#else
#define PRINTF_TYPE(X, y)
#endif

enum log_target {
    STDIO,
    SYSLOG
};

void log_set_target(enum log_target target);

void log_set_level(int level);

PRINTF_TYPE(2, 3)
extern void(*__log_function)(int priority, const char* format, ...);

PRINTF_TYPE(3, 4)
void __log_errno(const char* file, int line, const char* format, ...);

#define __LOG_FATAL(format, ...) \
    __log_function(LOG_ALERT, format, ## __VA_ARGS__)
#define __LOG_CRIT(format, ...) \
    __log_function(LOG_CRIT, format, ## __VA_ARGS__)
#define __LOG_ERROR(format, ...) \
    __log_function(LOG_ERR, format, ## __VA_ARGS__)
#define __LOG_WARNING(format, ...) \
    __log_function(LOG_WARNING, format, ## __VA_ARGS__)
#define __LOG_NOTICE(format, ...) \
    __log_function(LOG_NOTICE, format, ## __VA_ARGS__)
#define __LOG_INFO(format, ...) \
    __log_function(LOG_INFO, format, ## __VA_ARGS__)

#ifndef NDEBUG
#define __LOG_DEBUG(format, ...) \
    __log_function(LOG_DEBUG, "%s:%d: " format, __FILE__, \
            __LINE__, ## __VA_ARGS__)
#else
#define __LOG_DEBUG(format, ...)
#endif

#define LOG(priority, format, ...) __LOG_##priority(format, ## __VA_ARGS__)
#define LOG_ERRNO(format, ...) __log_errno(NULL, -1, format, ## __VA_ARGS__)
#define LOG_ERRNO_HERE(format, ...) \
    __log_errno(__FILE__, __LINE__, format, ## __VA_ARGS__)

#endif /* _LOGGING_H_ */
