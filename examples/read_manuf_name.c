#include <stdlib.h>

#include <uwifi/log.h>

#include "blzlib.h"
#include "blzlib_util.h"

int main(int argc, char** argv)
{
	if (argv[1] == NULL) {
		LOG_ERR("Pass MAC address of device to connect to");
		return EXIT_FAILURE;
	}

	char buf[20];

	log_open(NULL);

	blz* blz = blz_init("hci0");

	// F2:86:B3:CC:B7:E4
	uint8_t* mac = blz_string_to_mac_s(argv[1]);
	LOG_INF("Connecting to " MAC_FMT " ...", MAC_PAR(mac));

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
