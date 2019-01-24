#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>

#include <systemd/sd-bus.h>

#include <uwifi/log.h>

#include "blzlib.h"

#define UUID_WRITE	"6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define UUID_READ	"6e400003-b5a3-f393-e0a9-e50e24dcca9e"

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
	int fd;

	/* register the signal SIGINT handler */
	struct sigaction act;
	act.sa_handler = signal_handler;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	sigaction(SIGINT, &act, NULL);

	log_open("sdgatt");

	blz* blz = blz_init("hci0");

	LOG_INF("Connecting...");
	//uint8_t mac[] = { 0xCF, 0xD6, 0xE8, 0x4B, 0xA0, 0xD2 };
	uint8_t mac[] = { 0xC7, 0x2D, 0x19, 0x62, 0x10, 0xC1 };
	blz_dev* dev = blz_connect(blz, mac);
	if (!dev)
		goto exit;

	blz_char* wch = blz_get_char_from_uuid(dev, UUID_WRITE);
	blz_char* rch = blz_get_char_from_uuid(dev, UUID_READ);

	char* send = "Hallo";
	blz_char_write(wch, send, 5);

	blz_char_notify(rch, notify_handler);

	fd = blz_char_write_fd_acquire(wch);
	write(fd, "test", 4);

	while (!terminate) {
		blz_loop(blz);
	}

exit:
	blz_disconnect(dev);
	blz_fini(blz);
	close(fd);
	log_close();

	return EXIT_SUCCESS;
}
