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

/* MAC parameter, reverse (little-endian) */
#ifndef MAC_PARR
#define MAC_PARR(x) x[5], x[4], x[3], x[2], x[1], x[0]
#endif

#ifndef MAC_NOT_EMPTY
#define MAC_NOT_EMPTY(x) (x[0] || x[1] || x[2] || x[3] || x[4] || x[5])
#endif

#ifndef MAC_EMPTY
#define MAC_EMPTY(x) (!x[0] && !x[1] && !x[2] && !x[3] && !x[4] && !x[5])
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

#define STD_BASE_UUID                                                          \
	"\xfb\x34\x9b\x5f\x80\x00\x00\x80\x00\x10\x00\x00\x00\x00\x00\x00"

/* big endian human readable with ':' to little endian */
bool blz_string_to_mac(const char* str, uint8_t mac[6]);
uint8_t* blz_string_to_mac_s(const char* str);
const char* blz_mac_to_string_s(const uint8_t mac[6]);

/* big endian human readable UUID with dashes to little endian*/
bool blz_string_to_uuid(const char* str, uint8_t uuid[16]);
uint8_t* blz_string_to_uuid_s(const char* str);

/* little endian UUID128 to human readable string ALLOCATE */
char* blz_uuid_to_string_s(const uint8_t* uuid);
char* blz_uuid_to_string_a(const uint8_t* uuid);

/* UUID16 to human readable string ALLOCATE */
char* blz_uuid16_to_string_a(uint16_t uuid16);

void blz_uuid16_to_uuid(uint8_t dst[16], uint16_t uuid16);

const char* blz_addr_type_str(enum blz_addr_type atype);

void hex_dump(const char* txt, const uint8_t* data, size_t len);

#ifdef __cplusplus
}
#endif

#endif
