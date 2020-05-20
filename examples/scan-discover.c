#include <signal.h>
#include <stdlib.h>
#include <string.h>

#include "blzlib.h"
#include "blzlib_log.h"
#include "blzlib_util.h"

#define MAX_SCAN 10

static bool terminate = false;
static uint8_t scanned_macs[6][MAX_SCAN];
static int scan_idx = 0;

static void discover(blz* blz, const char* mac)
{
	LOG_INF("Connecting to %s...", mac);
	blz_dev* dev = blz_connect(blz, mac, BLZ_ADDR_UNKNOWN, NULL);
	if (!dev) {
		return;
	}

	char** uuids = blz_list_service_uuids(dev);

	for (int i = 0; uuids != NULL && uuids[i] != NULL; i++) {
		LOG_INF("\tService %s", uuids[i]);
		blz_serv* srv = blz_get_serv_from_uuid(dev, uuids[i]);
		if (srv) {
			char** chuuids = blz_list_char_uuids(srv);
			for (int i = 0; chuuids != NULL && chuuids[i] != NULL; i++) {
				LOG_INF("\t\tCharacteristic %s", chuuids[i]);
			}

			blz_serv_free(srv);
		}
	}

	blz_disconnect(dev);
}

static void scan_cb(const uint8_t* mac, int8_t rssi, const uint8_t* data,
					size_t len)
{
	/* Note: you can't connect in the scan callback! */

	LOG_INF("SCAN " MAC_FMT " %d", MAC_PARR(mac), rssi);

	for (int i = 0; i < MAX_SCAN && scanned_macs[i] != NULL; i++) {
		if (memcmp(scanned_macs[i], mac, 6) == 0) {
			return; // already in list
		}
	}

	memcpy(scanned_macs[scan_idx++], mac, 6);

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
	if (!blz) {
		return EXIT_FAILURE;
	}

	if (argc > 1) {
		for (int i = 1; i < argc; i++) {
			discover(blz, argv[i]);
		}
	} else {
		LOG_INF("Cached devices...");
		blz_known_devices(blz, scan_cb);

		LOG_INF("Scanning for 10 seconds... press Ctrl-C to cancel...");
		blz_scan_start(blz, scan_cb);

		blz_loop_timeout(blz, &terminate, 10000);

		blz_scan_stop(blz);

		/* connect to device to discover services and characteristics */
		for (int i = 0; i < MAX_SCAN && MAC_NOT_EMPTY(scanned_macs[i]); i++) {
			discover(blz, blz_mac_to_string_s(scanned_macs[i]));
		}
	}

	blz_fini(blz);

	return EXIT_SUCCESS;
}
