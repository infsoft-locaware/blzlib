#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>

#include <systemd/sd-event.h>

#include "blzlib.h"
#include "blzlib_util.h"
#include "blzlib_log.h"

#define UUID_WRITE	"6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define UUID_READ	"6e400003-b5a3-f393-e0a9-e50e24dcca9e"

static sd_event *event = NULL;

static void notify_handler(const char* data, size_t len, blz_char* ch)
{
	LOG_INF("RX: '%.*s'", (int)len - 1, data);
}

static bool signals_block(void)
{
	sigset_t sigmask_block;
	/* Initialize bocked signals (all which we handle) */
	if (sigemptyset(&sigmask_block)                       == -1 ||
	    sigaddset(&sigmask_block, SIGINT)                 == -1 ||
	    sigaddset(&sigmask_block, SIGTERM)                == -1) {
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

static int signal_handler(sd_event_source *s,	const struct signalfd_siginfo *si, void *user)
{
	LOG_INF("Received signal, shutting down...");

	/* detach sd-bus from event loop because we still need to use it later
	 * to disconnect and finish */
	sd_bus_detach_event(blz_get_sdbus(user));
	sd_event_exit(event, 0);
}

static int stdin_handler(sd_event_source* s, int fd, uint32_t revents, void* user)
{
	//blz_char* wch = user;
	int* wfd = user;
	char buffer[21];

	int len = read(STDIN_FILENO, buffer, 20);
	buffer[len] = '\r';

	if (len < 0) {
		LOG_ERR("Read error");
		return -1;
	}

	//blz_char_write(wch, buffer, len);
	write(*wfd, buffer, len + 1);
	return 0;
}

int main(int argc, char** argv)
{
	blz* blz = NULL;
	blz_dev* dev = NULL;
	blz_char* wch = NULL;
	blz_char* rch = NULL;

	if (argv[1] == NULL) {
		LOG_ERR("Pass MAC address of device to connect to");
		return EXIT_FAILURE;
	}

	signals_block();

	blz = blz_init("hci0");
	if (!blz)
		goto exit;

	//CF:D6:E8:4B:A0:D2 or C7:2D:19:62:10:C1
	uint8_t* mac = blz_string_to_mac_s(argv[1]);
	LOG_INF("Connecting to " MAC_FMT " ...", MAC_PAR(mac));
	dev = blz_connect(blz, mac);
	if (!dev)
		goto exit;

	wch = blz_get_char_from_uuid(dev, UUID_WRITE);
	rch = blz_get_char_from_uuid(dev, UUID_READ);

	if (!wch || !rch) {
		LOG_ERR("Nordic UART characteristics not found");
		goto exit;
	}

	bool b = blz_char_notify_start(rch, notify_handler);
	if (!b)
		goto exit;

	int wfd = blz_char_write_fd_acquire(wch);
	if (wfd < 0)
		goto exit;

	write(wfd, "---Nordic UART client started---\r\n", 34);

	/* set O_NONBLOCK on STDIN */
	int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
	fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

	/* Use SD Event loop */
	sd_event_default(&event);
        sd_event_add_signal(event, NULL, SIGTERM, signal_handler, blz);
        sd_event_add_signal(event, NULL, SIGINT, signal_handler, blz);
	sd_event_add_io(event, NULL, STDIN_FILENO,  EPOLLIN, stdin_handler, &wfd);
	sd_bus_attach_event(blz_get_sdbus(blz), event, SD_EVENT_PRIORITY_NORMAL);

	sd_event_loop(event);

exit:
	sd_event_unref(event);
	blz_char_notify_stop(rch);
	blz_disconnect(dev);
	blz_fini(blz);
	close(wfd);

	return EXIT_SUCCESS;
}
