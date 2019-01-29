#include <stdlib.h>

#include "blzlib.h"
#include "blzlib_util.h"
#include "blzlib_log.h"

int main(int argc, char** argv)
{
	blz* blz = NULL;
	blz_dev* dev = NULL;
	blz_char* rch = NULL;

	if (argv[1] == NULL) {
		LOG_ERR("Pass MAC address of device to connect to");
		return EXIT_FAILURE;
	}

	char buf[20];

	blz = blz_init("hci0");
	if (!blz)
		goto exit;

	// F2:86:B3:CC:B7:E4
	uint8_t* mac = blz_string_to_mac_s(argv[1]);
	LOG_INF("Connecting to " MAC_FMT " ...", MAC_PAR(mac));

	dev = blz_connect(blz, mac);
	if (!dev)
		goto exit;

	// Read manufacturer name string
	rch = blz_get_char_from_uuid(dev, "00002a29-0000-1000-8000-00805f9b34fb");
	if (!rch)
		goto exit;

	int len = blz_char_read(rch, buf, 20);
	LOG_INF("Manufacturer Name: '%.*s'", len, buf);

exit:
	blz_disconnect(dev);
	blz_fini(blz);

	return EXIT_SUCCESS;
}
