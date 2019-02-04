#include <stdlib.h>
#include <signal.h>
#include <string.h>

#include "blzlib.h"
#include "blzlib_log.h"

#define MAX_SCAN	10

static bool terminate = false;
static char* scanned_macs[MAX_SCAN];
static int scan_idx = 0;

static void discover(blz* blz, const char* mac)
{
	LOG_INF("Connecting to %s...", mac);
	blz_dev* dev = blz_connect(blz, mac);
	if (!dev)
		return;

	char** uuids = blz_list_service_uuids(dev);

	for (int i=0; uuids != NULL && uuids[i] != NULL; i++) {
		LOG_INF("\t[service %s]", uuids[i]);
	}

	uuids = blz_list_char_uuids(dev);

	for (int i=0; uuids != NULL && uuids[i] != NULL; i++) {
		LOG_INF("\t[char %s]", uuids[i]);
	}

	blz_disconnect(dev);
}

static void scan_cb(const char* mac, const char* name, char** uuids)
{
	LOG_INF("%s: %s", mac, name);

	for (int i = 0; uuids != NULL && uuids[i] != NULL; i++) {
		LOG_INF("\t[cached service %s]", uuids[i]);
	}

	scanned_macs[scan_idx++] = strdup(mac);

	if (scan_idx >= MAX_SCAN) {
		terminate = true;
	}
}

static void signal_handler(__attribute__((unused)) int signo)
{
	terminate = true;
}

int main(int argc, char** argv)
{
	/* register the signal SIGINT handler */
	struct sigaction act;
	act.sa_handler = signal_handler;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	sigaction(SIGINT, &act, NULL);

	blz* blz = blz_init("hci0");

	LOG_INF("Cached devices...");
	blz_known_devices(blz, scan_cb);

	LOG_INF("Scanning... press Ctrl-C to continue...");
	blz_scan_start(blz, scan_cb);

	while (!terminate) {
		blz_loop(blz, -1);
	}

	blz_scan_stop(blz);

	/* connect to device to discover services and characteristics */
	for (int i=0; i < MAX_SCAN && scanned_macs[i] != NULL; i++) {
		discover(blz, scanned_macs[i]);
		free(scanned_macs[i]);
	}

	blz_fini(blz);

	return EXIT_SUCCESS;
}
