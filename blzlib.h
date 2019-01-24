#ifndef BLZLIB_H
#define BLZLIB_H

#include <stdbool.h>
#include <stddef.h>

typedef struct blz_context blz;
typedef struct blz_char blz_char;

typedef void (*blz_notify_handler_t)(const char* data, size_t len, blz_char* ch);

blz* blz_init(void);

bool blz_connect(blz* ctx, const char* mac);
void blz_disconnect(blz* ctx);

bool blz_resolve_services(blz* ctx);
blz_char* blz_get_char_from_uuid(blz* ctx, const char* uuid);

bool blz_char_write(blz_char* ch, const char* data, size_t len);
bool blz_char_read(blz_char* ch, const char* data, size_t len);
bool blz_char_notify(blz_char* ch, blz_notify_handler_t cb);
int blz_char_write_fd_acquire(blz_char* ch);
int blz_char_write_fd_close(blz_char* ch);

void blz_loop(blz* ctx);

#endif
