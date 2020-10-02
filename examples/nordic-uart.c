#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <systemd/sd-bus.h>
#include <systemd/sd-event.h>

#include "blzlib.h"
#include "blzlib_log.h"

#define UUID_SERVICE "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define UUID_WRITE	 "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define UUID_READ	 "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

static sd_event* event = NULL;

static void notify_handler(const uint8_t* data, size_t len, blz_char* ch, void* user)
{
	LOG_INF("RX: '%.*s'", (int)len - 1, data);
}

static bool signals_block(void)
{
	sigset_t sigmask_block;
	/* Initialize bocked signals (all which we handle) */
	if (sigemptyset(&sigmask_block) == -1
		|| sigaddset(&sigmask_block, SIGINT) == -1
		|| sigaddset(&sigmask_block, SIGTERM) == -1) {
		LOG_ERR("failed to initialize block signals");
		return false;
	}

	/* block signals and get original mask */
	if (sigprocmask(SIG_BLOCK, &sigmask_block, NULL) == -1) {
		LOG_ERR("failed to block signals:");
		return false;
	}
	return true;
}

static int signal_handler(sd_event_source* s, const struct signalfd_siginfo* si,
						  void* user)
{
	LOG_INF("Received signal, shutting down...");

	/* detach sd-bus from event loop because we still need to use it later
	 * to disconnect and finish */
	sd_bus_detach_event(user);
	sd_event_exit(event, 0);
	return 0;
}

static int stdin_handler(sd_event_source* s, int fd, uint32_t revents,
						 void* user)
{
	int* wfd = user;
	char buffer[21];

	int len = read(STDIN_FILENO, buffer, 20);
	buffer[len] = '\r';

	if (len < 0) {
		LOG_ERR("Read error");
		return -1;
	}

	write(*wfd, buffer, len + 1);
	return 0;
}

static void log_handler(enum loglevel ll, const char* fmt, va_list ap)
{
	printf("blz: ");
	vprintf(fmt, ap);
	printf("\n");
}

int main(int argc, char** argv)
{
	blz_ctx* blz = NULL;  // blz context
	blz_dev* dev = NULL;  // device we connect to
	blz_serv* srv = NULL; // NUS service
	blz_char* wch = NULL; // characteristic to write to
	blz_char* rch = NULL; // characteristic we read from
	sd_bus* sdbus = NULL; // sd-bus object for sd-event

	if (argv[1] == NULL) {
		LOG_ERR("Pass MAC address of device to connect to");
		return EXIT_FAILURE;
	}

	/* blocking signals is necssary for the way we use sd_event */
	signals_block();

	/* Show how to use log handler, this is not necessary */
	blz_set_log_handler(log_handler);

	/* Initialize adapter and context */
	blz = blz_init("hci0");
	if (!blz) {
		goto exit;
	}

	/* Connect to device */
	LOG_INF("Connecting to %s...", argv[1]);
	dev = blz_connect(blz, argv[1], BLZ_ADDR_UNKNOWN);
	if (!dev) {
		goto exit;
	}

	srv = blz_get_serv_from_uuid(dev, UUID_SERVICE);
	if (!srv) {
		goto exit;
	}

	/* Find UUIDs we need */
	wch = blz_get_char_from_uuid(srv, UUID_WRITE);
	rch = blz_get_char_from_uuid(srv, UUID_READ);

	if (!wch || !rch) {
		LOG_ERR("Nordic UART characteristics not found");
		goto exit;
	}

	/* Start to get notifications from read characteristic */
	bool b = blz_char_notify_start(rch, notify_handler, NULL);
	if (!b) {
		goto exit;
	}

	/* Get a file descriptor we can use to write to the write characteristic.
	 * Writing to a fd is more efficient than repeatedly using
	 * blz_char_write(wch, buffer, len); */
	int wfd = blz_char_write_fd_acquire(wch);
	if (wfd < 0) {
		goto exit;
	}

	write(wfd, "---Nordic UART client started---\r\n", 34);

	/* set O_NONBLOCK on STDIN */
	int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
	fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

	/* get default dbus system connection */
	sd_bus_default_system(&sdbus);

	/* Use SD Event loop */
	sd_event_default(&event);
	sd_event_add_signal(event, NULL, SIGTERM, signal_handler, sdbus);
	sd_event_add_signal(event, NULL, SIGINT, signal_handler, sdbus);
	sd_event_add_io(event, NULL, STDIN_FILENO, EPOLLIN, stdin_handler, &wfd);
	sd_bus_attach_event(sdbus, event, SD_EVENT_PRIORITY_NORMAL);

	LOG_INF("Connected! Enter commands:");

	sd_event_loop(event);

exit:
	sd_event_unref(event);
	sd_bus_unref(sdbus);
	blz_char_notify_stop(rch);
	blz_char_free(rch);
	blz_char_free(wch);
	blz_serv_free(srv);
	blz_disconnect(dev);
	blz_fini(blz);
	close(wfd);

	return EXIT_SUCCESS;
}
