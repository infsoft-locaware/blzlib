#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>

#include <systemd/sd-bus.h>

#include <uwifi/log.h>

#include "blzlib.h"

static bool terminate = false;

void notify_handler(const char* data, size_t len, blz_char* ch)
{
	LOG_INF("RX: '%.*s' %zd", (int)len - 1, data, len);
}

static void signal_handler(__attribute__((unused)) int signo)
{
	terminate = true;
}

int main(int argc, char** argv)
{
	bool r = false;
	int fd;

	/* register the signal SIGINT handler */
	struct sigaction act;
	act.sa_handler = signal_handler;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	sigaction(SIGINT, &act, NULL);

	log_open("sdgatt");

	blz* blz = blz_init();

	LOG_INF("Connecting...");
	r = blz_connect(blz, "CF:D6:E8:4B:A0:D2"); // C7:2D:19:62:10:C1
	if (!r)
		goto exit;

	blz_resolve_services(blz);
	blz_char* ch = blz_get_char_from_uuid(blz, "");

	char* send = "Hallo";
	blz_char_write(ch, send, 5);

	blz_char_notify(ch, notify_handler);

	fd = blz_char_write_fd_acquire(ch);
	write(fd, "test", 4);

	while (!terminate) {
		blz_loop(blz);
	}

exit:
	blz_disconnect(blz);
	blz_fini(blz);
	close(fd);
	log_close();

	return EXIT_SUCCESS;
}
