#include <stdlib.h>

#include <uwifi/log.h>

#include "blzlib.h"

int main(int argc, char** argv)
{
	char buf[20];

	log_open(NULL);

	blz* blz = blz_init("hci0");

	LOG_INF("Connecting...");
	uint8_t mac[] = { 0xF2, 0x86, 0xB3, 0xCC, 0xB7, 0xE4 };

	blz_dev* dev = blz_connect(blz, mac);
	if (!dev)
		goto exit;

	// Read manufacturer name string
	blz_char* rch = blz_get_char_from_uuid(dev, "00002a29-0000-1000-8000-00805f9b34fb");

	int len = blz_char_read(rch, buf, 20);
	LOG_INF("Manufacturer Name: '%.*s'", len, buf);

exit:
	blz_disconnect(dev);
	blz_fini(blz);
	log_close();

	return EXIT_SUCCESS;
}
