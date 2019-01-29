#include <stdio.h>

#include "blzlib_util.h"

bool blz_string_to_mac(const char* str, uint8_t mac[6])
{
	if (str == NULL || mac == NULL)
		return false;

	int n = sscanf(str, "%2hhx:%2hhx:%2hhx:%2hhx:%2hhx:%2hhx",
			mac, mac+1, mac+2, mac+3, mac+4, mac+5);
	return (n == 6);
}

uint8_t* blz_string_to_mac_s(const char* str)
{
	static uint8_t mac[6];
	blz_string_to_mac(str, mac);
	return mac;
}

