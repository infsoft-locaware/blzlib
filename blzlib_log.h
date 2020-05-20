/*
 * blzlib - Copyright (C) 2019 Bruno Randolf (br1@einfach.org)
 *
 * This source code is licensed under the GNU Lesser General Public License,
 * Version 3. See the file COPYING for more details.
 */

#ifndef BLZ_LOG_H
#define BLZ_LOG_H

#include <stdarg.h>

/* these conincide with syslog levels for convenience */
enum loglevel { LL_CRIT = 2, LL_ERR, LL_WARN, LL_NOTICE, LL_INFO, LL_DEBUG };

void __attribute__((format(printf, 2, 3)))
blz_log_out(enum loglevel ll, const char* fmt, ...);

void blz_set_log_handler(void (*cb)(enum loglevel ll, const char* fmt,
									va_list ap));

#ifndef DEBUG
#define DEBUG 0
#endif

#define LOG_CRIT(...) blz_log_out(LL_CRIT, __VA_ARGS__)
#define LOG_ERR(...)  blz_log_out(LL_ERR, __VA_ARGS__)
#define LOG_WARN(...) blz_log_out(LL_WARN, __VA_ARGS__)
#define LOG_NOTI(...) blz_log_out(LL_NOTICE, __VA_ARGS__)
#define LOG_INF(...)  blz_log_out(LL_INFO, __VA_ARGS__)
#define LOG_DBG(...)                                                           \
	do {                                                                       \
		if (DEBUG)                                                             \
			blz_log_out(LL_DEBUG, __VA_ARGS__);                                \
	} while (0)

#endif
