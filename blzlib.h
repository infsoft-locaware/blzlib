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

typedef struct blz_context blz_ctx;
typedef struct blz_dev blz_dev;
typedef struct blz_char blz_char;
typedef struct blz_serv blz_serv;

typedef void (*blz_notify_handler_t)(const uint8_t* data, size_t len,
									 blz_char* ch, void* user);
typedef void (*blz_scan_handler_t)(const uint8_t* mac, enum blz_addr_type atype,
								   int8_t rssi, const uint8_t* data, size_t len,
								   void* user);
typedef void (*blz_disconn_handler_t)(void* user);

blz_ctx* blz_init(const char* dev);
void blz_fini(blz_ctx* ctx);

bool blz_known_devices(blz_ctx* ctx, blz_scan_handler_t cb, void* user);
bool blz_scan_start(blz_ctx* ctx, blz_scan_handler_t cb, void* user);
bool blz_scan_stop(blz_ctx* ctx);

blz_dev* blz_connect(blz_ctx* ctx, const char* macstr, enum blz_addr_type atype);

void blz_set_disconnect_handler(blz_dev* dev, blz_disconn_handler_t cb,
								void* user);

/** returns NULL terminated list of service UUID strings, don't free them */
char** blz_list_service_uuids(blz_dev* dev);
blz_serv* blz_get_serv_from_uuid(blz_dev* dev, const char* uuid_srv);

/** returns NULL terminated list of char UUID strings, don't free them */
char** blz_list_char_uuids(blz_serv* srv);
blz_char* blz_get_char_from_uuid(blz_serv* srv, const char* uuid_char);

bool blz_char_write(blz_char* ch, const uint8_t* data, size_t len);
bool blz_char_write_cmd(blz_char* ch, const uint8_t* data, size_t len);
int blz_char_read(blz_char* ch, uint8_t* data, size_t len);
bool blz_char_notify_start(blz_char* ch, blz_notify_handler_t cb, void* user);
bool blz_char_indicate_start(blz_char* ch, blz_notify_handler_t cb, void* user);
bool blz_char_notify_stop(blz_char* ch);
/** returns fd or -1 on error. need to close(fd) to release */
int blz_char_write_fd_acquire(blz_char* ch);

bool blz_loop_one(blz_ctx* ctx, uint32_t timeout_ms);
int blz_loop_wait(blz_ctx* ctx, bool* check, uint32_t timeout_ms);
int blz_get_fd(blz_ctx* ctx);
void blz_handle_read(blz_ctx* ctx);

/* this frees dev */
void blz_disconnect(blz_dev* dev);
void blz_serv_free(blz_serv* srv);
void blz_char_free(blz_char* ch);



#ifdef __cplusplus
}
#endif

#endif
