#ifndef BLZLIB_UTIL_H
#define BLZLIB_UTIL_H

#include <stdbool.h>
#include <stdint.h>

/* for use in printf-like functions */
#define MAC_FMT "%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC_PAR(x) x[0], x[1], x[2], x[3], x[4], x[5]

bool blz_string_to_mac(const char* str, uint8_t mac[6]);
uint8_t* blz_string_to_mac_s(const char* str);

#endif
