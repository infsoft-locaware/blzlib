#include <stdlib.h>
#include <signal.h>

#include <uwifi/log.h>

#include "blzlib.h"

static bool terminate = false;

void scan_cb(const char* name, const uint8_t* mac)
{
	LOG_INF("scan");
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

	log_open(NULL);

	blz* blz = blz_init("hci0");

	blz_scan_start(blz, scan_cb);

	while (!terminate) {
		blz_loop(blz);
	}

	blz_scan_stop(blz);

	blz_fini(blz);

	log_close();
	return EXIT_SUCCESS;
}
