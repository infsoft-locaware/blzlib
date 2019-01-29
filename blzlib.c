#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <systemd/sd-bus.h>

#include "blzlib.h"
#include "blzlib_internal.h"
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
	r = sd_bus_open_system(&ctx->bus);
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
		LOG_ERR("BLZ failed to power on: %s", error.message);
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

bool blz_scan_start(blz* ctx, blz_scan_handler_t cb)
{
	sd_bus_error error = SD_BUS_ERROR_NULL;
	int r;

	ctx->scan_cb = cb;

	//TODO list all already known devices
	//TODO connect signal

	r = sd_bus_call_method(ctx->bus,
		"org.bluez", ctx->path,
		"org.bluez.Adapter1",
		"StartDiscovery",
		&error, NULL, "");

	if (r < 0) {
		LOG_ERR("BLZ failed to scan: %s", error.message);
	}

	sd_bus_error_free(&error);
	return r >= 0;
}

bool blz_scan_stop(blz* ctx)
{
	sd_bus_error error = SD_BUS_ERROR_NULL;
	int r;

	ctx->scan_cb = NULL;

	//TODO disconnect signal

	r = sd_bus_call_method(ctx->bus,
		"org.bluez", ctx->path,
		"org.bluez.Adapter1",
		"StopDiscovery",
		&error, NULL, "");

	if (r < 0) {
		LOG_ERR("BLZ failed to stop scan: %s", error.message);
	}

	sd_bus_error_free(&error);
	return r >= 0;
}

blz_dev* blz_connect(blz* ctx, const uint8_t* mac)
{
	int r;
	sd_bus_error error = SD_BUS_ERROR_NULL;

	struct blz_dev* dev = calloc(1, sizeof(struct blz_dev));
	if (dev == NULL) {
		LOG_ERR("blz_dev: alloc failed");
		return NULL;
	}

	dev->ctx = ctx;

	/* create device path based on MAC address */
	r = snprintf(dev->path, DBUS_PATH_MAX_LEN,
			"%s/dev_%02X_%02X_%02X_%02X_%02X_%02X",
			ctx->path, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	if (r < 0 || r >= DBUS_PATH_MAX_LEN) {
		LOG_ERR("BLZ connect failed to construct device path");
		free(dev);
		return NULL;
	}

	r = sd_bus_call_method(ctx->bus,
		"org.bluez", dev->path,
		"org.bluez.Device1",
		"Connect",
		&error, NULL, "");

	if (r < 0) {
		LOG_ERR("BLZ failed to connect: %s", error.message);
		sd_bus_error_free(&error);
		free(dev);
		return NULL;
	}

	sd_bus_error_free(&error);
	return dev;
}

#if 0
// unused: get services on device
void blz_get_services(blz_dev* dev)
{
	char** uuids;
	sd_bus_error error = SD_BUS_ERROR_NULL;

	int r = sd_bus_get_property_strv(dev->ctx->bus,
			"org.bluez",
			dev->path,
			"org.bluez.Device1",
			"UUIDs",
			&error, &uuids);

	if (r < 0)
		LOG_ERR("couldnt get services: %s", error.message);

	for (int i = 0; uuids[i] != NULL; i++) {
		LOG_INF("UUID %s", uuids[i]);
		free(uuids[i]);
	}

	sd_bus_error_free(&error);
}
#endif

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

	r = parse_msg_objects(reply, ch);

exit:
	sd_bus_error_free(&error);
	sd_bus_message_unref(reply);
	return r == 1000 ? true : false;
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

static int blz_signal_cb(sd_bus_message* m, void* user, sd_bus_error* err)
{
	int r;
	char* str;
	const void* ptr;
	size_t len;
	struct blz_char* ch = user;

	if (ch == NULL || ch->notify_cb == NULL) {
		LOG_ERR("BLZ signal no callback");
		return -1;
	}

	/* interface */
	r = sd_bus_message_read_basic(m, 's', &str);
	if (r < 0) {
		LOG_ERR("BLZ signal msg read error");
		return -2;
	}

	/* ignore all other interfaces */
	if (strcmp(str, "org.bluez.GattCharacteristic1") != 0) {
		LOG_INF("BLZ signal interface %s ignored", str);
		return 0;
	}

	/* array of dict */
	r = sd_bus_message_enter_container(m, 'a', "{sv}");
	if (r < 0) {
		LOG_ERR("BLZ signal msg read error");
		return -2;
	}

	/* enter first element */
	r = sd_bus_message_enter_container(m, 'e', "sv");
	if (r < 0) {
		LOG_ERR("BLZ signal msg read error");
		return -2;
	}

	/* property name */
	r = sd_bus_message_read_basic(m, 's', &str);
	if (r < 0) {
		LOG_ERR("BLZ signal msg read error");
		return -2;
	}

	/* ignore all except Value */
	if (strcmp(str, "Value") != 0) {
		LOG_INF("BLZ signal property %s ignored", str);
		return 0;
	}

	/* enter variant */
	r = sd_bus_message_enter_container(m, 'v', "ay");
	if (r < 0) {
		LOG_ERR("BLZ signal msg read error");
		return -2;
	}

	/* get byte array */
	r = sd_bus_message_read_array(m, 'y', &ptr, &len);
	if (r < 0) {
		LOG_ERR("BLZ signal msg read error");
		return -2;
	}


	ch->notify_cb(ptr, len, ch);
	return 0;
}

bool blz_char_notify_start(blz_char* ch, blz_notify_handler_t cb)
{
	sd_bus_error error = SD_BUS_ERROR_NULL;
	sd_bus_message* reply = NULL;
	int r;

	ch->notify_cb = cb;

	r = sd_bus_match_signal(ch->ctx->bus, &ch->notify_slot,
		"org.bluez", ch->path,
		"org.freedesktop.DBus.Properties",
		"PropertiesChanged",
		blz_signal_cb, ch);

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

	if (ch == NULL)
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

void blz_loop(blz* conn)
{
	sd_bus_message *m = NULL;
	int r = sd_bus_process(conn->bus, &m);
	if (r < 0) {
		LOG_ERR("BLZ loop process error: %s", strerror(-r));
		return;
	}

	/* sd_bus_wait() should be called only if sd_bus_process() returned 0 */
	if (r > 0) {
		return;
	}

	r = sd_bus_wait(conn->bus, (uint64_t)-1);
	if (r < 0 && -r != EINTR) {
		LOG_ERR("BLZ loop wait error: %s", strerror(-r));
	}
}

int blz_get_fd(blz* ctx)
{
	return sd_bus_get_fd(ctx->bus);
}

int blz_get_events(blz* ctx)
{
	return sd_bus_get_events(ctx->bus);
}

uint64_t blz_get_timeout(blz* ctx)
{
	uint64_t usec;
	int r = sd_bus_get_timeout(ctx->bus, &usec);
	if (r < 0)
		LOG_ERR("ERR %d", r);
	else
		LOG_INF("to %ld", usec);
	return usec;
}

void blz_process(blz* ctx)
{
	sd_bus_message *m = NULL;
	int r = sd_bus_process(ctx->bus, &m);
	if (r < 0) {
		LOG_ERR("BLZ loop process error: %s", strerror(-r));
		return;
	}
}

sd_bus* blz_get_sdbus(blz* ctx)
{
	// new in systemd 240: sd_bus_set_close_on_exit(ctx->bus, 0);
	return ctx->bus;
}
