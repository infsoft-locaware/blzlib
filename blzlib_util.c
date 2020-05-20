/*
 * blzlib - Copyright (C) 2019 Bruno Randolf (br1@einfach.org)
 *
 * This source code is licensed under the GNU Lesser General Public License,
 * Version 3. See the file COPYING for more details.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blzlib_log.h"
#include "blzlib_util.h"

bool blz_string_to_mac(const char* str, uint8_t mac[6])
{
	if (str == NULL || mac == NULL)
		return false;

	int n = sscanf(str, "%2hhx:%2hhx:%2hhx:%2hhx:%2hhx:%2hhx", mac + 5, mac + 4,
				   mac + 3, mac + 2, mac + 1, mac);
	return (n == 6);
}

uint8_t* blz_string_to_mac_s(const char* str)
{
	static uint8_t mac[6];
	blz_string_to_mac(str, mac);
	return mac;
}

const char* blz_mac_to_string_s(const uint8_t mac[6])
{
	static char buf[18];
	sprintf(buf, MAC_FMT, MAC_PARR(mac));
	return buf;
}

bool blz_string_to_uuid(const char* str, uint8_t uuid[16])
{
	int n = sscanf(str,
				   "%2hhx%2hhx%2hhx%2hhx-%2hhx%2hhx-%2hhx%2hhx-%2hhx%2hhx-%"
				   "2hhx%2hhx%2hhx%2hhx%2hhx%2hhx",
				   uuid + 15, uuid + 14, uuid + 13, uuid + 12, uuid + 11,
				   uuid + 10, uuid + 9, uuid + 8, uuid + 7, uuid + 6, uuid + 5,
				   uuid + 4, uuid + 3, uuid + 2, uuid + 1, uuid);
	return (n == 16);
}

uint8_t* blz_string_to_uuid_s(const char* str)
{
	static uint8_t uuid[16];
	blz_string_to_uuid(str, uuid);
	return uuid;
}

char* blz_uuid_to_string_s(const uint8_t* uuid)
{
	static char buf[37];
	int r = sprintf(
		buf,
		"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
		uuid[15], uuid[14], uuid[13], uuid[12], uuid[11], uuid[10], uuid[9],
		uuid[8], uuid[7], uuid[6], uuid[5], uuid[4], uuid[3], uuid[2], uuid[1],
		uuid[0]);
	if (r <= 0) {
		return NULL;
	}
	return buf;
}

char* blz_uuid_to_string_a(const uint8_t* uuid)
{
	char* str;
	int r = asprintf(
		&str,
		"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
		uuid[15], uuid[14], uuid[13], uuid[12], uuid[11], uuid[10], uuid[9],
		uuid[8], uuid[7], uuid[6], uuid[5], uuid[4], uuid[3], uuid[2], uuid[1],
		uuid[0]);
	if (r <= 0) {
		return NULL;
	}
	return str;
}

char* blz_uuid16_to_string_a(uint16_t uuid)
{
	char* str;
	int r = asprintf(&str, "0000%04x-0000-1000-8000-00805f9b34fb", uuid);
	if (r <= 0) {
		return NULL;
	}
	return str;
}

void blz_uuid16_to_uuid(uint8_t dst[16], uint16_t uuid16)
{
	memcpy(dst, STD_BASE_UUID, 16);
	dst[12] = uuid16;
	dst[13] = uuid16 >> 8;
}

const char* blz_addr_type_str(enum blz_addr_type atype)
{
	switch (atype) {
	case BLZ_ADDR_UNKNOWN:
		return "unknown";
	case BLZ_ADDR_PUBLIC:
		return "public";
	case BLZ_ADDR_RANDOM:
		return "random";
	}
	return "-invalid-";
}

void hex_dump(const char* txt, const uint8_t* data, size_t len)
{
	printf("%s", txt);
	for (int i = 0; i < len; i++) {
		printf("%02x ", *(data + i));
	}
	printf("\n");
}
