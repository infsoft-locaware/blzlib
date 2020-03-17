/*
 * blzlib - Copyright (C) 2019 Bruno Randolf (br1@einfach.org)
 *
 * This source code is licensed under the GNU Lesser General Public License,
 * Version 3. See the file COPYING for more details.
 */

#ifndef BLZLIB_UTIL_H
#define BLZLIB_UTIL_H

#include <stdbool.h>
#include <stdint.h>

#include "blzlib.h"

#ifdef __cplusplus
extern "C" {
#endif

/* for use in printf-like functions */
#ifndef MAC_FMT
#define MAC_FMT "%02x:%02x:%02x:%02x:%02x:%02x"
#endif

#ifndef MAC_PAR
#define MAC_PAR(x) x[0], x[1], x[2], x[3], x[4], x[5]
#endif

#ifndef MAX
#define MAX(_x, _y) ((_x) > (_y) ? (_x) : (_y))
#endif

#ifndef MIN
#define MIN(_x, _y) ((_x) < (_y) ? (_x) : (_y))
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*(x)))
#endif

bool blz_string_to_mac(const char* str, uint8_t mac[6]);
uint8_t* blz_string_to_mac_s(const char* str);
const char* blz_addr_type_str(enum blz_addr_type atype);

#ifdef __cplusplus
}
#endif

#endif
