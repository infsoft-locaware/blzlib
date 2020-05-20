/*
 * blzlib - Copyright (C) 2019 Bruno Randolf (br1@einfach.org)
 *
 * This source code is licensed under the GNU Lesser General Public License,
 * Version 3. See the file COPYING for more details.
 */

#ifndef BLZLIB_H
#define BLZLIB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum blz_addr_type { BLZ_ADDR_UNKNOWN, BLZ_ADDR_PUBLIC, BLZ_ADDR_RANDOM };

typedef struct blz_context blz;
typedef struct blz_dev blz_dev;
typedef struct blz_char blz_char;
typedef struct blz_serv blz_serv;

typedef void (*blz_notify_handler_t)(const uint8_t* data, size_t len,
									 blz_char* ch);
typedef void (*blz_scan_handler_t)(const uint8_t* mac, int8_t rssi,
								   const uint8_t* data, size_t len);
typedef void (*blz_disconn_handler_t)(void);

blz* blz_init(const char* dev);
void blz_fini(blz* ctx);

bool blz_known_devices(blz* ctx, blz_scan_handler_t cb);
bool blz_scan_start(blz* ctx, blz_scan_handler_t cb);
bool blz_scan_stop(blz* ctx);

blz_dev* blz_connect(blz* ctx, const char* macstr, enum blz_addr_type atype,
					 blz_disconn_handler_t cb);
/* this frees dev */
void blz_disconnect(blz_dev* dev);

bool blz_discover_services(blz_dev* dev);
blz_serv* blz_get_serv_from_uuid(blz_dev* dev, const char* uuid_srv);
/** returns NULL terminated list of service UUID strings, don't free them */
char** blz_list_service_uuids(blz_dev* dev);

/** returns NULL terminated list of characteristic UUID strings, don't free them
 */
char** blz_list_char_uuids(blz_serv* dev);
bool blz_discover_characteristics(blz_serv* serv);
blz_char* blz_get_char_from_uuid(blz_serv* serv, const char* uuid_char);

bool blz_char_write(blz_char* ch, const uint8_t* data, size_t len);
bool blz_char_write_cmd(blz_char* ch, const uint8_t* data, size_t len);
int blz_char_read(blz_char* ch, uint8_t* data, size_t len);
bool blz_char_notify_start(blz_char* ch, blz_notify_handler_t cb);
bool blz_char_notify_stop(blz_char* ch);
/** returns fd or -1 on error. need to close(fd) to release */
int blz_char_write_fd_acquire(blz_char* ch);

void blz_loop(blz* ctx, uint64_t timeout_us);
int blz_loop_timeout(blz* ctx, bool* check, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif
