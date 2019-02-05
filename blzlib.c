#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <systemd/sd-bus.h>

#include "blzlib.h"
#include "blzlib_internal.h"
#include "blzlib_util.h"
#include "blzlib_log.h"

blz* blz_init(const char* dev)
{
	int r;
	struct blz_context* ctx;
	sd_bus_error error = SD_BUS_ERROR_NULL;

	ctx = calloc(1, sizeof(struct blz_context));
	if (ctx == NULL) {
		LOG_ERR("blz_context: alloc failed");
		return NULL;
	}

	r = snprintf(ctx->path, DBUS_PATH_MAX_LEN, "/org/bluez/%s", dev);
	if (r < 0 || r >= DBUS_PATH_MAX_LEN) {
		LOG_ERR("BLZ init failed to construct path");
		free(ctx);
		return NULL;
	}

	/* Connect to the system bus */
	r = sd_bus_default_system(&ctx->bus);
	if (r < 0) {
		LOG_ERR("Failed to connect to system bus: %s", strerror(-r));
		free(ctx);
		return NULL;
	}

	/* power on if necessary */
	r = sd_bus_set_property(ctx->bus,
		"org.bluez", ctx->path,
		"org.bluez.Adapter1",
		"Powered",
		 &error, "b", 1);

	if (r < 0) {
		if (sd_bus_error_has_name(&error, SD_BUS_ERROR_UNKNOWN_OBJECT)) {
			LOG_ERR("Adapter %s not known", dev);
		} else {
			LOG_ERR("BLZ failed to power on: %s", error.message);
		}
		sd_bus_error_free(&error);
		sd_bus_unref(ctx->bus);
		free(ctx);
		return NULL;
	}

	sd_bus_error_free(&error);
	return ctx;
}

void blz_fini(blz* ctx)
{
	if (ctx == NULL)
		return;

	sd_bus_unref(ctx->bus);
	free(ctx);
}

bool blz_known_devices(blz* ctx, blz_scan_handler_t cb)
{
	sd_bus_error error = SD_BUS_ERROR_NULL;
	sd_bus_message* reply = NULL;
	int r;

	ctx->scan_cb = cb;

	r = sd_bus_call_method(ctx->bus,
		"org.bluez", "/",
		"org.freedesktop.DBus.ObjectManager",
		"GetManagedObjects",
		&error, &reply, "");

	if (r < 0) {
		LOG_ERR("Failed to get managed objects: %s", error.message);
		goto exit;
	}

	r = msg_parse_objects(reply, ctx->path, OBJ_DEVICE_SCAN, ctx);
	/* error logging done in function */

exit:
	sd_bus_error_free(&error);
	sd_bus_message_unref(reply);
	return r >= 0;
}

static int blz_intf_cb(sd_bus_message* m, void* user, sd_bus_error* err)
{
	blz* ctx = user;

	if (ctx == NULL || ctx->scan_cb == NULL) {
		LOG_ERR("BLZ scan no callback");
		return -1;
	}

	/* error logging done in function */
	return msg_parse_object(m, ctx->path, OBJ_DEVICE_SCAN, ctx);
}

bool blz_scan_start(blz* ctx, blz_scan_handler_t cb)
{
	sd_bus_error error = SD_BUS_ERROR_NULL;
	int r;

	ctx->scan_cb = cb;

	r = sd_bus_match_signal(ctx->bus, &ctx->scan_slot,
		"org.bluez", "/",
		"org.freedesktop.DBus.ObjectManager",
		"InterfacesAdded",
		blz_intf_cb, ctx);

	if (r < 0) {
		LOG_ERR("BLZ Failed to notify");
		goto exit;
	}

	r = sd_bus_call_method(ctx->bus,
		"org.bluez", ctx->path,
		"org.bluez.Adapter1",
		"StartDiscovery",
		&error, NULL, "");

	if (r < 0) {
		LOG_ERR("BLZ failed to scan: %s", error.message);
	}

exit:
	sd_bus_error_free(&error);
	return r >= 0;
}

bool blz_scan_stop(blz* ctx)
{
	sd_bus_error error = SD_BUS_ERROR_NULL;
	int r;

	r = sd_bus_call_method(ctx->bus,
		"org.bluez", ctx->path,
		"org.bluez.Adapter1",
		"StopDiscovery",
		&error, NULL, "");

	if (r < 0) {
		LOG_ERR("BLZ failed to stop scan: %s", error.message);
	}

	ctx->scan_slot = sd_bus_slot_unref(ctx->scan_slot);
	ctx->scan_cb = NULL;

	sd_bus_error_free(&error);
	return r >= 0;
}

static int blz_connect_cb(sd_bus_message* m, void* user, sd_bus_error* err)
{
	struct blz_dev* dev = user;

	if (dev == NULL) {
		LOG_ERR("BLZ conn no dev");
		return -1;
	}

	/* error logging done in function */
	msg_parse_interface(m, OBJ_DEVICE, NULL, dev);
	return 0;
}

blz_dev* blz_connect(blz* ctx, const char* macstr)
{
	int r;
	uint8_t mac[6];
	sd_bus_error error = SD_BUS_ERROR_NULL;
	int alread_connected; /* note: bool in sd-dbus is expected to be int type */

	struct blz_dev* dev = calloc(1, sizeof(struct blz_dev));
	if (dev == NULL) {
		LOG_ERR("blz_dev: alloc failed");
		return NULL;
	}

	dev->ctx = ctx;
	dev->services_resolved = false;

	/* create device path based on MAC address */
	blz_string_to_mac(macstr, mac);
	r = snprintf(dev->path, DBUS_PATH_MAX_LEN,
			"%s/dev_%02X_%02X_%02X_%02X_%02X_%02X",
			ctx->path, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	if (r < 0 || r >= DBUS_PATH_MAX_LEN) {
		LOG_ERR("BLZ connect failed to construct device path");
		free(dev);
		return NULL;
	}

	/* check if it already is connected. this also serves as a mean to check
	 * wether the object path is known in DBus */
	r = sd_bus_get_property_trivial(dev->ctx->bus,
			"org.bluez",
			dev->path,
			"org.bluez.Device1",
			"Connected",
			&error, 'b', &alread_connected);

	if (r < 0) {
		if (sd_bus_error_has_name(&error, SD_BUS_ERROR_UNKNOWN_OBJECT)) {
			LOG_ERR("Device %s not known", macstr);
		} else {
			LOG_ERR("BLZ failed to get connected: %s", error.message);
		}
		goto exit;
	}

	if (alread_connected) {
		LOG_NOTI("Device %s already was connected", macstr);
		goto exit;
	}

	/* connect signal for ServicesResolved */
	r = sd_bus_match_signal(ctx->bus, &dev->connect_slot,
		"org.bluez", dev->path,
		"org.freedesktop.DBus.Properties",
		"PropertiesChanged",
		blz_connect_cb, dev);

	if (r < 0) {
		LOG_ERR("BLZ Failed to add connect signal");
		goto exit;
	}

	r = sd_bus_call_method(ctx->bus,
		"org.bluez", dev->path,
		"org.bluez.Device1",
		"Connect",
		&error, NULL, "");

	if (r < 0) {
		LOG_ERR("BLZ failed to connect: %s", error.message);
		goto exit;
	}

	/* wait until ServicesResolved property changed to true for this device.
	 * we usually receive connected = true before that, but at that time we
	 * are not ready yet to look up service and characteristic UUIDs */
	r = blz_loop_timeout(ctx, &dev->services_resolved, 5000);
	if (r < 0) {
		LOG_ERR("BLZ timeout waiting for ServicesResolved");
	}

exit:
	sd_bus_error_free(&error);
	dev->connect_slot = sd_bus_slot_unref(dev->connect_slot);
	if (r < 0) {
		free(dev);
		return NULL;
	}
	return dev;
}

char** blz_list_service_uuids(blz_dev* dev)
{
	sd_bus_error error = SD_BUS_ERROR_NULL;

	int r = sd_bus_get_property_strv(dev->ctx->bus,
			"org.bluez",
			dev->path,
			"org.bluez.Device1",
			"UUIDs",
			&error, &dev->service_uuids);

	if (r < 0) {
		LOG_ERR("couldnt get services: %s", error.message);
	}

	sd_bus_error_free(&error);
	return dev->service_uuids;
}

void blz_disconnect(blz_dev* dev)
{
	if (!dev)
		return;

	sd_bus_error error = SD_BUS_ERROR_NULL;
	int r;

	r = sd_bus_call_method(dev->ctx->bus,
		"org.bluez", dev->path,
		"org.bluez.Device1",
		"Disconnect",
		&error, NULL, "");

	if (r < 0) {
		LOG_ERR("BLZ failed to disconnect: %s", error.message);
	}

	sd_bus_error_free(&error);

	/* free */
	for (int i = 0; dev->service_uuids != NULL && dev->service_uuids[i] != NULL; i++) {
		free(dev->service_uuids[i]);
	}
	for (int i = 0; dev->char_uuids != NULL && dev->char_uuids[i] != NULL; i++) {
		free(dev->char_uuids[i]);
	}

	free(dev);
}

static bool find_char_by_uuid(blz_char* ch)
{
	sd_bus_error error = SD_BUS_ERROR_NULL;
	sd_bus_message* reply = NULL;
	int r;

	r = sd_bus_call_method(ch->ctx->bus,
		"org.bluez", "/",
		"org.freedesktop.DBus.ObjectManager",
		"GetManagedObjects",
		&error, &reply, "");

	if (r < 0) {
		LOG_ERR("Failed to get managed objects: %s", error.message);
		goto exit;
	}

	r = msg_parse_objects(reply, ch->dev->path, OBJ_CHAR, ch);
	/* error logging done in function */

exit:
	sd_bus_error_free(&error);
	sd_bus_message_unref(reply);
	return r == RETURN_FOUND;
}

char** blz_list_char_uuids(blz_dev* dev)
{
	sd_bus_error error = SD_BUS_ERROR_NULL;
	sd_bus_message* reply = NULL;
	int r;

	r = sd_bus_call_method(dev->ctx->bus,
		"org.bluez", "/",
		"org.freedesktop.DBus.ObjectManager",
		"GetManagedObjects",
		&error, &reply, "");

	if (r < 0) {
		LOG_ERR("Failed to get managed objects: %s", error.message);
		goto exit;
	}

	/* first count how many characteristics there are and alloc space */
	int cnt = 0;
	r = msg_parse_objects(reply, dev->path, OBJ_CHAR_COUNT, &cnt);
	if (r < 0) {
		goto exit;
	}

	/* alloc space for them */
	dev->char_uuids = calloc(cnt+1, sizeof(char*));
	dev->char_uuids[cnt] = NULL;
	if (dev->char_uuids == NULL) {
		LOG_ERR("BLZ alloc of chars failed");
		goto exit;
	}

	/* now parse all characteristics data */
	sd_bus_message_rewind(reply, true);
	r = msg_parse_objects(reply, dev->path, OBJ_CHARS_ALL, dev);
	/* error logging done in function */

exit:
	sd_bus_error_free(&error);
	sd_bus_message_unref(reply);
	if (r < 0) {
		return NULL;
	}
	return dev->char_uuids;
}

blz_char* blz_get_char_from_uuid(blz_dev* dev, const char* uuid)
{
	/* alloc char structure for use later */
	struct blz_char* ch = calloc(1, sizeof(struct blz_char));
	if (ch == NULL) {
		LOG_ERR("blz_char: alloc failed");
		return NULL;
	}

	ch->ctx = dev->ctx;
	ch->dev = dev;
	strncpy(ch->uuid, uuid, UUID_STR_LEN);

	/* this will try to find the uuid in char, fill required info */
	bool b = find_char_by_uuid(ch);
	if (!b) {
		LOG_ERR("Couldn't find characteristic with UUID %s", uuid);
		free(ch);
		return NULL;
	}

	LOG_INF("Found characteristic with UUID %s", uuid);
	return ch;
}

bool blz_char_write(blz_char* ch, const char* data, size_t len)
{
	sd_bus_error error = SD_BUS_ERROR_NULL;
	sd_bus_message* call = NULL;
	sd_bus_message* reply = NULL;
	int r;

	if (!(ch->flags & (BLZ_CHAR_WRITE | BLZ_CHAR_WRITE_WITHOUT_RESPONSE))) {
		LOG_ERR("BLZ characteristic does not support write");
		return false;
	}

	r = sd_bus_message_new_method_call(ch->ctx->bus, &call,
		"org.bluez", ch->path,
		"org.bluez.GattCharacteristic1",
		"WriteValue");

	if (r < 0) {
		LOG_ERR("BLZ write failed to create message");
		goto exit;
	}

	r = sd_bus_message_append_array(call, 'y', data, len);
	if (r < 0) {
		LOG_ERR("BLZ write failed to create message");
		goto exit;
	}

	r = sd_bus_message_open_container(call, 'a', "{sv}");
	if (r < 0) {
		LOG_ERR("BLZ write failed to create message");
		goto exit;
	}

	r = sd_bus_message_close_container(call);
	if (r < 0) {
		LOG_ERR("BLZ write failed to create message");
		goto exit;
	}

	r = sd_bus_call(ch->ctx->bus, call, 0, &error, &reply);
	if (r < 0) {
		LOG_ERR("BLZ failed to write: %s", error.message);
		goto exit;
	}

exit:
	sd_bus_error_free(&error);
	sd_bus_message_unref(call);
	sd_bus_message_unref(reply);
	return r >= 0;
}

int blz_char_read(blz_char* ch, char* data, size_t len)
{
	sd_bus_error error = SD_BUS_ERROR_NULL;
	sd_bus_message* reply = NULL;
	const void* ptr;
	size_t rlen = -1;
	int r;

	if (!(ch->flags & BLZ_CHAR_READ)) {
		LOG_ERR("BLZ characteristic does not support read");
		return false;
	}

	r = sd_bus_call_method(ch->ctx->bus,
		"org.bluez", ch->path,
		"org.bluez.GattCharacteristic1",
		"ReadValue",
		&error, &reply, "a{sv}", 0);

	if (r < 0) {
		LOG_ERR("BLZ failed to read: %s", error.message);
		goto exit;
	}

	r = sd_bus_message_read_array(reply, 'y', &ptr, &rlen);
	if (r < 0) {
		LOG_ERR("BLZ failed to read result: %s", error.message);
		goto exit;
	}

	if (rlen > 0) {
		memcpy(data, ptr, rlen < len ? rlen : len);
	}

exit:
	sd_bus_error_free(&error);
	sd_bus_message_unref(reply);
	return rlen;
}

static int blz_notify_cb(sd_bus_message* m, void* user, sd_bus_error* err)
{
	int r;
	const void* ptr = NULL;
	size_t len;
	struct blz_char* ch = user;

	if (ch == NULL || ch->notify_cb == NULL) {
		LOG_ERR("BLZ signal no callback");
		return -1;
	}

	r = msg_parse_notify(m, ch, &ptr, &len);

	if (r > 0 && ptr != NULL) {
		ch->notify_cb(ptr, len, ch);
	}

	return 0;
}

bool blz_char_notify_start(blz_char* ch, blz_notify_handler_t cb)
{
	sd_bus_error error = SD_BUS_ERROR_NULL;
	sd_bus_message* reply = NULL;
	int r;

	if (!(ch->flags & (BLZ_CHAR_NOTIFY | BLZ_CHAR_INDICATE))) {
		LOG_ERR("BLZ characteristic does not support notify");
		return false;
	}

	ch->notify_cb = cb;

	r = sd_bus_match_signal(ch->ctx->bus, &ch->notify_slot,
		"org.bluez", ch->path,
		"org.freedesktop.DBus.Properties",
		"PropertiesChanged",
		blz_notify_cb, ch);

	if (r < 0) {
		LOG_ERR("BLZ Failed to notify");
		goto exit;
	}

	r = sd_bus_call_method(ch->ctx->bus,
		"org.bluez", ch->path,
		"org.bluez.GattCharacteristic1",
		"StartNotify",
		&error, &reply, "");

	if (r < 0) {
		LOG_ERR("BLZ Failed to start notify: %s", error.message);
	}

	/* wait until Notifying property changed to true */
	r = blz_loop_timeout(ch->ctx, &ch->notifying, 5000);
	if (r < 0) {
		LOG_ERR("BLZ timeout waiting for Notifying");
	}

exit:
	sd_bus_error_free(&error);
	sd_bus_message_unref(reply);
	return r >= 0;
}

bool blz_char_notify_stop(blz_char* ch)
{
	sd_bus_error error = SD_BUS_ERROR_NULL;
	sd_bus_message* reply = NULL;
	int r;

	if (ch == NULL || ch->notify_slot == NULL)
		return false;

	r = sd_bus_call_method(ch->ctx->bus,
		"org.bluez", ch->path,
		"org.bluez.GattCharacteristic1",
		"StopNotify",
		&error, &reply, "");

	if (r < 0) {
		LOG_ERR("BLZ Failed to stop notify: %s", error.message);
	}

	ch->notify_slot = sd_bus_slot_unref(ch->notify_slot);
	ch->notify_cb = NULL;

	sd_bus_error_free(&error);
	sd_bus_message_unref(reply);
	return r >= 0;
}

int blz_char_write_fd_acquire(blz_char* ch)
{
	sd_bus_error error = SD_BUS_ERROR_NULL;
	sd_bus_message* reply = NULL;
	int fd = -1;
	int r;

	if (!(ch->flags & BLZ_CHAR_WRITE_WITHOUT_RESPONSE)) {
		LOG_ERR("BLZ characteristic does not support write-without-response");
		return -1;
	}

	r = sd_bus_call_method(ch->ctx->bus,
		"org.bluez", ch->path,
		"org.bluez.GattCharacteristic1",
		"AcquireWrite",
		&error, &reply,
		"a{sv}", 0);

	if (r < 0) {
		LOG_ERR("BLZ Failed acquire write: %s", error.message);
		goto exit;
	}

	r = sd_bus_message_read(reply, "h", &fd);
	if (r < 0) {
		LOG_ERR("BLZ Failed to get write fd");
	} else {
		r = dup(fd);
	}

exit:
	sd_bus_error_free(&error);
	sd_bus_message_unref(reply);
	return r;
}

void blz_loop(blz* ctx, uint64_t timeout_us)
{
	int r = sd_bus_process(ctx->bus, NULL);
	if (r < 0) {
		LOG_ERR("BLZ loop process error: %s", strerror(-r));
		return;
	}

	/* sd_bus_wait() should be called only if sd_bus_process() returned 0 */
	if (r > 0) {
		return;
	}

	r = sd_bus_wait(ctx->bus, timeout_us);
	if (r < 0 && -r != EINTR) {
		LOG_ERR("BLZ loop wait error: %s", strerror(-r));
	}
}

/** return -1 on timeout */
int blz_loop_timeout(blz* ctx, bool* check, uint32_t timeout_ms)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	uint32_t current_ms = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
	uint32_t end_ms = current_ms + timeout_ms;

	while (!*check && current_ms < end_ms) {
		blz_loop(ctx, (end_ms - current_ms) * 1000);
		clock_gettime(CLOCK_MONOTONIC, &ts);
		current_ms = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
	}

	return *check ? 0 : -1;
}
