#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

#include <systemd/sd-bus.h>

#include <uwifi/log.h>

#include "blzlib.h"

void notify_handler(const char* data, size_t len, blz_char* ch)
{
	LOG_INF("RX: '%.*s' %zd", (int)len - 1, data, len);
}

int main(int argc, char** argv)
{
	log_open("sdgatt");

	blz* blz = blz_init();

	blz_resolve_services(blz);
	blz_char* ch = blz_get_char_from_uuid(blz, "");

	char* send = "Hallo";
	blz_char_write(ch, send, 5);

	blz_char_notify(ch, notify_handler);

	int fd = blz_char_write_fd_acquire(ch);
	write(fd, "test", 4);

	while (true) {
		blz_loop(blz);
	}

	log_close();

	return EXIT_SUCCESS;
}
