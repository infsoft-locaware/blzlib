/*
 * blzlib - Copyright (C) 2019 Bruno Randolf (br1@einfach.org)
 *
 * This source code is licensed under the GNU Lesser General Public License,
 * Version 3. See the file COPYING for more details.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <systemd/sd-bus.h>
#include <time.h>
#include <unistd.h>

#include "blzlib.h"
#include "blzlib_internal.h"
#include "blzlib_log.h"
#include "blzlib_util.h"

blz_ctx* blz_init(const char* dev)
{
	int r;
	struct blz_context* ctx;
	sd_bus_error error = SD_BUS_ERROR_NULL;

	ctx = calloc(1, sizeof(struct blz_context));
	if (ctx == NULL) {
		LOG_ERR("BLZ: blz_context alloc failed");
		return NULL;
	}

	r = snprintf(ctx->path, DBUS_PATH_MAX_LEN, "/org/bluez/%s", dev);
	if (r < 0 || r >= DBUS_PATH_MAX_LEN) {
		LOG_ERR("BLZ: Failed to construct path");
		free(ctx);
		return NULL;
	}

	/* Connect to the system bus */
	r = sd_bus_default_system(&ctx->bus);
	if (r < 0) {
		LOG_ERR("BLZ: Failed to connect to system bus: %s", strerror(-r));
		free(ctx);
		return NULL;
	}

	/* power on if necessary */
	r = sd_bus_set_property(ctx->bus, "org.bluez", ctx->path,
							"org.bluez.Adapter1", "Powered", &error, "b", 1);

	if (r < 0) {
		if (sd_bus_error_has_name(&error, SD_BUS_ERROR_UNKNOWN_OBJECT)) {
			LOG_ERR("BLZ: Adapter %s not known", dev);
		} else {
			LOG_ERR("BLZ: Failed to power on: %s", error.message);
		}
		sd_bus_error_free(&error);
		sd_bus_unref(ctx->bus);
		free(ctx);
		return NULL;
	}

	sd_bus_error_free(&error);
	return ctx;
}

void blz_fini(blz_ctx* ctx)
{
	if (ctx == NULL) {
		return;
	}
	sd_bus_unref(ctx->bus);
	ctx->bus = NULL;
	free(ctx);
}

bool blz_known_devices(blz_ctx* ctx, blz_scan_handler_t cb, void* user)
{
	sd_bus_error error = SD_BUS_ERROR_NULL;
	sd_bus_message* reply = NULL;
	int r;

	ctx->scan_cb = cb;
	ctx->scan_user = user;

	r = sd_bus_call_method(ctx->bus, "org.bluez", "/",
						   "org.freedesktop.DBus.ObjectManager",
						   "GetManagedObjects", &error, &reply, "");

	if (r < 0) {
		LOG_ERR("BLZ: Failed to get managed objects: %s", error.message);
		goto exit;
	}

	r = msg_parse_objects(reply, ctx->path, MSG_DEVICE_SCAN, ctx);
	/* error logging done in function */

exit:
	sd_bus_error_free(&error);
	sd_bus_message_unref(reply);
	return r >= 0;
}

static int blz_intf_cb(sd_bus_message* m, void* user, sd_bus_error* err)
{
	blz_ctx* ctx = user;

	if (ctx == NULL || ctx->scan_cb == NULL) {
		LOG_ERR("BLZ: Scan no callback");
		return -1;
	}

	/* error logging done in function */
	return msg_parse_object(m, ctx->path, MSG_DEVICE_SCAN, ctx);
}

bool blz_scan_start(blz_ctx* ctx, blz_scan_handler_t cb, void* user)
{
	sd_bus_error error = SD_BUS_ERROR_NULL;
	int r;

	ctx->scan_cb = cb;
	ctx->scan_user = user;

	r = sd_bus_match_signal(ctx->bus, &ctx->scan_slot, "org.bluez", "/",
							"org.freedesktop.DBus.ObjectManager",
							"InterfacesAdded", blz_intf_cb, ctx);

	if (r < 0) {
		LOG_ERR("BLZ: Failed to notify");
		goto exit;
	}

	r = sd_bus_call_method(ctx->bus, "org.bluez", ctx->path,
						   "org.bluez.Adapter1", "StartDiscovery", &error, NULL,
						   "");

	if (r < 0) {
		LOG_ERR("BLZ: Failed to scan: %s", error.message);
	}

exit:
	sd_bus_error_free(&error);
	return r >= 0;
}

bool blz_scan_stop(blz_ctx* ctx)
{
	sd_bus_error error = SD_BUS_ERROR_NULL;
	int r;

	r = sd_bus_call_method(ctx->bus, "org.bluez", ctx->path,
						   "org.bluez.Adapter1", "StopDiscovery", &error, NULL,
						   "");

	if (r < 0) {
		LOG_ERR("BLZ: Failed to stop scan: %s", error.message);
	}

	ctx->scan_slot = sd_bus_slot_unref(ctx->scan_slot);
	ctx->scan_cb = NULL;
	ctx->scan_user = NULL;

	sd_bus_error_free(&error);
	return r >= 0;
}

static int blz_connect_cb(sd_bus_message* m, void* user, sd_bus_error* err)
{
	struct blz_dev* dev = user;

	if (dev == NULL) {
		LOG_ERR("BLZ: Connect cb no dev");
		return -1;
	}

	/* error logging done in function */
	msg_parse_interface(m, MSG_DEVICE, NULL, dev);
	return 0;
}

static int connect_known_cb(sd_bus_message* reply, void* userdata,
							sd_bus_error* error)
{
	int r = 0;
	blz_dev* dev = (blz_dev*)userdata;

	if (dev == NULL) {
		LOG_ERR("BLZ: Connect cb no dev");
		return -1;
	}

	const sd_bus_error* err = sd_bus_message_get_error(reply);
	if (err != NULL) {
		r = -sd_bus_message_get_errno(reply);
		LOG_INF("BLZ: Connect error: %s '%s' (%d)", err->name, err->message, r);
	}

	dev->connect_async_result = r;
	dev->connect_async_done = true;
	return r;
}

static int blz_connect_known(blz_dev* dev, const char* macstr)
{
	int r;
	sd_bus_message* call = NULL;

	r = sd_bus_message_new_method_call(dev->ctx->bus, &call, "org.bluez",
									   dev->path, "org.bluez.Device1",
									   "Connect");

	if (r < 0) {
		LOG_ERR("BLZ: Connect failed to create message: %d", r);
		goto exit;
	}

	dev->connect_async_done = false;

	/* call it async because it can take longer than the normal sd_bus
	 * timeout and we want to wait until it is finished or failed */
	r = sd_bus_call_async(dev->ctx->bus, NULL, call, connect_known_cb, dev,
						  CONNECT_TIMEOUT * 1000000);

	if (r < 0) {
		LOG_ERR("BLZ: Connect failed (%d)", r);
		goto exit;
	}

	/* wait for callback */
	r = blz_loop_wait(dev->ctx, &dev->connect_async_done,
					  CONNECT_TIMEOUT * 1000);
	if (r < 0) {
		LOG_ERR("BLZ: Connect timeout");
	} else {
		r = dev->connect_async_result;
	}

exit:
	sd_bus_message_unref(call);
	return r;
}

static int connect_new_cb(sd_bus_message* reply, void* userdata,
						  sd_bus_error* error)
{
	int r;
	char* opath;
	blz_dev* dev = (blz_dev*)userdata;

	if (dev == NULL) {
		LOG_ERR("BLZ: Connect new cb no dev");
		return -1;
	}

	const sd_bus_error* err = sd_bus_message_get_error(reply);
	if (err != NULL) {
		if (sd_bus_error_has_name(err, SD_BUS_ERROR_UNKNOWN_METHOD)) {
			LOG_NOTI("BLZ: Connect new failed: Bluez < 5.49 (with -E"
					 " flag) doesn't support ConnectDevice");
			r = -2;
		} else {
			r = -sd_bus_message_get_errno(reply);
			LOG_INF("BLZ: Connect new error: %s '%s' (%d)", err->name,
					err->message, r);
		}
		goto exit;
	}

	r = sd_bus_message_read_basic(reply, 'o', &opath);
	if (r < 0) {
		LOG_ERR("BLZ: Connect new invalid reply");
		goto exit;
	}

	if (strcmp(opath, dev->path) != 0) {
		LOG_ERR("BLZ: Connect new device paths don't match (%s %s)", opath,
				dev->path);
		r = -1;
		goto exit;
	}

exit:
	dev->connect_async_result = r;
	dev->connect_async_done = true;
	return r;
}

static int blz_connect_new(blz_dev* dev, const char* macstr, bool addr_public)
{
	int r;
	sd_bus_message* call = NULL;

	LOG_INF("BLZ: Connect new to %s (%s)", macstr,
			addr_public ? "public" : "random");

	r = sd_bus_message_new_method_call(dev->ctx->bus, &call, "org.bluez",
									   dev->ctx->path, "org.bluez.Adapter1",
									   "ConnectDevice");

	if (r < 0) {
		LOG_ERR("BLZ: Connect new failed to create message");
		goto exit;
	}

	/* open array */
	r = sd_bus_message_open_container(call, 'a', "{sv}");
	if (r < 0) {
		LOG_ERR("BLZ: Connect new failed to create message");
		goto exit;
	}

	r = msg_append_property(call, "Address", 's', macstr);
	if (r < 0) {
		goto exit;
	}

	/* AddressType must either be public or random for BLE, otherwise a
	 * Bluetooth classic connection (BR/EDR) is attempted */
	r = msg_append_property(call, "AddressType", 's',
							addr_public ? "public" : "random");
	if (r < 0) {
		goto exit;
	}

	/* close array */
	r = sd_bus_message_close_container(call);
	if (r < 0) {
		LOG_ERR("BLZ: Connect new failed to create message");
		goto exit;
	}

	dev->connect_async_done = false;

	/* call ConnectDevice, it is only supported from Bluez 5.49 on.
	 * call it async because it can take longer than the normal sd_bus
	 * timeout and we want to wait until it is finished or failed */
	r = sd_bus_call_async(dev->ctx->bus, NULL, call, connect_new_cb, dev,
						  CONNECT_TIMEOUT * 1000000);
	if (r < 0) {
		LOG_ERR("BLZ: Connect new failed: %d", r);
		goto exit;
	}

	/* wait for callback */
	r = blz_loop_wait(dev->ctx, &dev->connect_async_done,
					  CONNECT_TIMEOUT * 1000);
	if (r < 0) {
		LOG_ERR("BLZ: Connect new timeout");
	} else {
		r = dev->connect_async_result;
	}

exit:
	sd_bus_message_unref(call);
	return r;
}

blz_dev* blz_connect(blz_ctx* ctx, const char* macstr, enum blz_addr_type atype)
{
	int r;
	uint8_t mac[6];
	sd_bus_error error = SD_BUS_ERROR_NULL;
	int conn_status = -2; // invalid
	bool need_disconnect = false;

	struct blz_dev* dev = calloc(1, sizeof(struct blz_dev));
	if (dev == NULL) {
		LOG_ERR("BLZ: Connect blz_dev alloc failed");
		return NULL;
	}

	dev->ctx = ctx;
	dev->connected = false;
	dev->services_resolved = false;

	/* create device path based on MAC address */
	blz_string_to_mac(macstr, mac);
	r = snprintf(dev->path, DBUS_PATH_MAX_LEN,
				 "%s/dev_%02X_%02X_%02X_%02X_%02X_%02X", ctx->path, mac[5],
				 mac[4], mac[3], mac[2], mac[1], mac[0]);

	if (r < 0 || r >= DBUS_PATH_MAX_LEN) {
		LOG_ERR("BLZ: Connect failed to construct device path");
		free(dev);
		return NULL;
	}

	/* check if it already is connected. this also serves as a mean to check
	 * wether the object path is known in DBus */
	r = sd_bus_get_property_trivial(dev->ctx->bus, "org.bluez", dev->path,
									"org.bluez.Device1", "Connected", &error,
									'b', &conn_status);

	if (r < 0) {
		if (sd_bus_error_has_name(&error, SD_BUS_ERROR_UNKNOWN_OBJECT)) {
			/* device is unknown, mark for ConnectDevice API below */
			conn_status = -1;
		} else {
			LOG_ERR("BLZ: Failed to get connected: %s", error.message);
			goto exit;
		}
	}

	if (conn_status == 1) {
		LOG_NOTI("BLZ: Device %s already was connected", macstr);
		/* get ServicesResolved status */
		int sr;
		r = sd_bus_get_property_trivial(dev->ctx->bus, "org.bluez", dev->path,
										"org.bluez.Device1", "ServicesResolved",
										&error, 'b', &sr);
		if (r < 0) {
			LOG_ERR("BLZ: Failed to get ServicesResolved: %s", error.message);
			need_disconnect = true;
			goto exit;
		}
		dev->services_resolved = sr;
	} else if (conn_status != 0 && conn_status != -1) {
		/* invalid status */
		goto exit;
	}

	/* connect signal for device properties changed */
	r = sd_bus_match_signal(ctx->bus, &dev->connect_slot, "org.bluez",
							dev->path, "org.freedesktop.DBus.Properties",
							"PropertiesChanged", blz_connect_cb, dev);

	if (r < 0) {
		LOG_ERR("BLZ: Failed to add connect signal");
		goto exit;
	}

	/* if the device is already known in the DBus object hierarchy, connect
	 * by the normal Connect API, if not try using the new (Bluez 5.49)
	 * ConnectDevice API for unknown (not yet discovered) devices */
	if (conn_status == 0) {
		r = blz_connect_known(dev, macstr);
	} else if (conn_status == -1) {
		r = blz_connect_new(dev, macstr,
							atype == BLZ_ADDR_PUBLIC ? true : false);
		/* when addr type is unknown and connect failed, try the other type */
		if (r < 0 && atype == BLZ_ADDR_UNKNOWN) {
			r = blz_connect_new(dev, macstr,
								atype == BLZ_ADDR_PUBLIC ? false : true);
		}
	}

	/* connect calls may have failed with timeout, in this situation bluez
	 * is still trying to open the connection. Calling Disconnect cancels
	 * the connection attempt */
	if (r < 0) {
		need_disconnect = true;
		goto exit;
	}

	/* wait until ServicesResolved property changed to true for this device.
	 * we usually receive connected = true before that, but at that time we
	 * are not ready yet to look up service and characteristic UUIDs */
	r = blz_loop_wait(ctx, &dev->services_resolved, SERV_RESOLV_TIMEOUT * 1000);
	if (r < 0) {
		LOG_ERR("BLZ: Timeout waiting for ServicesResolved");
		need_disconnect = true;
	} else {
		dev->connected = true;
	}

exit:
	sd_bus_error_free(&error);
	if (r < 0) {
		if (need_disconnect) {
			blz_disconnect(dev); // frees
		} else {
			free(dev);
		}
		return NULL;
	}
	return dev;
}

void blz_set_disconnect_handler(blz_dev* dev, blz_disconn_handler_t cb,
								void* disconn_user)
{
	dev->disconnect_cb = cb;
	dev->disconn_user = disconn_user;
}

static bool find_serv_by_uuid(blz_serv* srv)
{
	sd_bus_error error = SD_BUS_ERROR_NULL;
	sd_bus_message* reply = NULL;
	int r;

	r = sd_bus_call_method(srv->ctx->bus, "org.bluez", "/",
						   "org.freedesktop.DBus.ObjectManager",
						   "GetManagedObjects", &error, &reply, "");

	if (r < 0) {
		LOG_ERR("BLZ: Failed to get managed objects: %s", error.message);
		goto exit;
	}

	r = msg_parse_objects(reply, srv->dev->path, MSG_SERV_FIND, srv);
	/* error logging done in function */

exit:
	sd_bus_error_free(&error);
	sd_bus_message_unref(reply);
	return r == RETURN_FOUND;
}

blz_serv* blz_get_serv_from_uuid(blz_dev* dev, const char* uuid)
{
	/* alloc serv structure for use later */
	struct blz_serv* srv = calloc(1, sizeof(struct blz_serv));
	if (srv == NULL) {
		LOG_ERR("BLZ: blz_srv alloc failed");
		return NULL;
	}

	srv->ctx = dev->ctx;
	srv->dev = dev;
	strncpy(srv->uuid, uuid, UUID_STR_LEN);

	/* this will try to find the uuid in char, fill required info */
	bool b = find_serv_by_uuid(srv);
	if (!b) {
		LOG_ERR("BLZ: Couldn't find service with UUID %s", uuid);
		free(srv);
		return NULL;
	}

	// LOG_INF("Found service with UUID %s", uuid);
	return srv;
}

char** blz_list_service_uuids(blz_dev* dev)
{
	sd_bus_error error = SD_BUS_ERROR_NULL;

	int r = sd_bus_get_property_strv(dev->ctx->bus, "org.bluez", dev->path,
									 "org.bluez.Device1", "UUIDs", &error,
									 &dev->service_uuids);

	if (r < 0) {
		LOG_ERR("BLZ: Couldn't get services: %s", error.message);
	}

	sd_bus_error_free(&error);
	return dev->service_uuids;
}

static bool find_char_by_uuid(blz_char* ch)
{
	sd_bus_error error = SD_BUS_ERROR_NULL;
	sd_bus_message* reply = NULL;
	int r;

	r = sd_bus_call_method(ch->ctx->bus, "org.bluez", "/",
						   "org.freedesktop.DBus.ObjectManager",
						   "GetManagedObjects", &error, &reply, "");

	if (r < 0) {
		LOG_ERR("BLZ: Failed to get managed objects: %s", error.message);
		goto exit;
	}

	r = msg_parse_objects(reply, ch->dev->path, MSG_CHAR_FIND, ch);
	/* error logging done in function */

exit:
	sd_bus_error_free(&error);
	sd_bus_message_unref(reply);
	return r == RETURN_FOUND;
}

char** blz_list_char_uuids(blz_serv* srv)
{
	sd_bus_error error = SD_BUS_ERROR_NULL;
	sd_bus_message* reply = NULL;
	int r;

	r = sd_bus_call_method(srv->ctx->bus, "org.bluez", "/",
						   "org.freedesktop.DBus.ObjectManager",
						   "GetManagedObjects", &error, &reply, "");

	if (r < 0) {
		LOG_ERR("BLZ: Failed to get managed objects: %s", error.message);
		goto exit;
	}

	/* first count how many characteristics there are and alloc space */
	int cnt = 0;
	r = msg_parse_objects(reply, srv->path, MSG_CHAR_COUNT, &cnt);
	if (r < 0) {
		goto exit;
	}

	/* alloc space for them */
	srv->char_uuids = calloc(cnt + 1, sizeof(char*));
	srv->char_uuids[cnt] = NULL;
	if (srv->char_uuids == NULL) {
		LOG_ERR("BLZ: Alloc of chars failed");
		goto exit;
	}

	/* now parse all characteristics data */
	sd_bus_message_rewind(reply, true);
	r = msg_parse_objects(reply, srv->path, MSG_CHARS_ALL, srv);
	/* error logging done in function */

exit:
	sd_bus_error_free(&error);
	sd_bus_message_unref(reply);
	if (r < 0) {
		return NULL;
	}
	return srv->char_uuids;
}

blz_char* blz_get_char_from_uuid(blz_serv* srv, const char* uuid)
{
	/* alloc char structure for use later */
	struct blz_char* ch = calloc(1, sizeof(struct blz_char));
	if (ch == NULL) {
		LOG_ERR("BLZ: blz_char alloc failed");
		return NULL;
	}

	ch->ctx = srv->dev->ctx;
	ch->dev = srv->dev;
	strncpy(ch->uuid, uuid, UUID_STR_LEN);

	/* this will try to find the uuid in char, fill required info */
	bool b = find_char_by_uuid(ch);
	if (!b) {
		LOG_ERR("BLZ: Couldn't find characteristic with UUID %s", uuid);
		free(ch);
		return NULL;
	}

	LOG_INF("BLZ: Found characteristic with UUID %s", uuid);
	return ch;
}

bool blz_char_write(blz_char* ch, const uint8_t* data, size_t len)
{
	sd_bus_error error = SD_BUS_ERROR_NULL;
	sd_bus_message* call = NULL;
	sd_bus_message* reply = NULL;
	int r;

	if (!(ch->flags & (BLZ_CHAR_WRITE | BLZ_CHAR_WRITE_WITHOUT_RESPONSE))) {
		LOG_ERR("BLZ: Characteristic does not support write");
		return false;
	}

	r = sd_bus_message_new_method_call(
		ch->ctx->bus, &call, "org.bluez", ch->path,
		"org.bluez.GattCharacteristic1", "WriteValue");

	if (r < 0) {
		LOG_ERR("BLZ: Frite failed to create message");
		goto exit;
	}

	r = sd_bus_message_append_array(call, 'y', data, len);
	if (r < 0) {
		LOG_ERR("BLZ: Write failed to create message");
		goto exit;
	}

	r = sd_bus_message_open_container(call, 'a', "{sv}");
	if (r < 0) {
		LOG_ERR("BLZ: Write failed to create message");
		goto exit;
	}

	r = sd_bus_message_close_container(call);
	if (r < 0) {
		LOG_ERR("BLZ: Write failed to create message");
		goto exit;
	}

	r = sd_bus_call(ch->ctx->bus, call, 0, &error, &reply);
	if (r < 0) {
		LOG_ERR("BLZ: Failed to write: %s", error.message);
		goto exit;
	}

exit:
	sd_bus_error_free(&error);
	sd_bus_message_unref(call);
	sd_bus_message_unref(reply);
	return r >= 0;
}

bool blz_char_write_cmd(blz_char* ch, const uint8_t* data, size_t len)
{
	return blz_char_write(ch, data, len);
}

int blz_char_read(blz_char* ch, uint8_t* data, size_t len)
{
	sd_bus_error error = SD_BUS_ERROR_NULL;
	sd_bus_message* reply = NULL;
	const void* ptr;
	size_t rlen = -1;
	int r;

	if (!(ch->flags & BLZ_CHAR_READ)) {
		LOG_ERR("BLZ: Characteristic does not support read");
		return false;
	}

	r = sd_bus_call_method(ch->ctx->bus, "org.bluez", ch->path,
						   "org.bluez.GattCharacteristic1", "ReadValue", &error,
						   &reply, "a{sv}", 0);

	if (r < 0) {
		LOG_ERR("BLZ: Failed to read: %s", error.message);
		goto exit;
	}

	r = sd_bus_message_read_array(reply, 'y', &ptr, &rlen);
	if (r < 0) {
		LOG_ERR("BLZ: Failed to read result: %s", error.message);
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
		LOG_ERR("BLZ: Signal no callback");
		return -1;
	}

	r = msg_parse_notify(m, ch, &ptr, &len);

	if (r > 0 && ptr != NULL) {
		ch->notify_cb(ptr, len, ch, ch->notify_user);
	}

	return 0;
}

bool blz_char_notify_start(blz_char* ch, blz_notify_handler_t cb, void* user)
{
	sd_bus_error error = SD_BUS_ERROR_NULL;
	sd_bus_message* reply = NULL;
	int r;

	if (!(ch->flags & (BLZ_CHAR_NOTIFY | BLZ_CHAR_INDICATE))) {
		LOG_ERR("BLZ: Characteristic does not support notify");
		return false;
	}

	ch->notify_cb = cb;
	ch->notify_user = user;

	r = sd_bus_match_signal(ch->ctx->bus, &ch->notify_slot, "org.bluez",
							ch->path, "org.freedesktop.DBus.Properties",
							"PropertiesChanged", blz_notify_cb, ch);

	if (r < 0) {
		LOG_ERR("BLZ: Failed to notify");
		goto exit;
	}

	r = sd_bus_call_method(ch->ctx->bus, "org.bluez", ch->path,
						   "org.bluez.GattCharacteristic1", "StartNotify",
						   &error, &reply, "");

	if (r < 0) {
		LOG_ERR("BLZ: Failed to start notify: %s", error.message);
	}

	/* wait until Notifying property changed to true */
	r = blz_loop_wait(ch->ctx, &ch->notifying, 5000);
	if (r < 0) {
		LOG_ERR("BLZ: Timeout waiting for Notifying");
	}

exit:
	sd_bus_error_free(&error);
	sd_bus_message_unref(reply);
	return r >= 0;
}

bool blz_char_indicate_start(blz_char* ch, blz_notify_handler_t cb, void* user)
{
	return blz_char_notify_start(ch, cb, user);
}

bool blz_char_notify_stop(blz_char* ch)
{
	sd_bus_error error = SD_BUS_ERROR_NULL;
	sd_bus_message* reply = NULL;
	int r;

	if (ch == NULL || ch->notify_slot == NULL) {
		return false;
	}

	r = sd_bus_call_method(ch->ctx->bus, "org.bluez", ch->path,
						   "org.bluez.GattCharacteristic1", "StopNotify",
						   &error, &reply, "");

	if (r < 0) {
		LOG_ERR("BLZ: Failed to stop notify: %s", error.message);
	}

	ch->notify_slot = sd_bus_slot_unref(ch->notify_slot);
	ch->notify_cb = NULL;
	ch->notify_user = NULL;

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
		LOG_WARN("BLZ: Characteristic does not support write-without-response");
		return -1;
	}

	r = sd_bus_call_method(ch->ctx->bus, "org.bluez", ch->path,
						   "org.bluez.GattCharacteristic1", "AcquireWrite",
						   &error, &reply, "a{sv}", 0);

	if (r < 0) {
		LOG_ERR("BLZ: Failed acquire write: %s", error.message);
		goto exit;
	}

	r = sd_bus_message_read(reply, "h", &fd);
	if (r < 0) {
		LOG_ERR("BLZ: Failed to get write fd");
	} else {
		r = dup(fd);
	}

exit:
	sd_bus_error_free(&error);
	sd_bus_message_unref(reply);
	return r;
}

/** frees dev */
void blz_disconnect(blz_dev* dev)
{
	if (!dev || !dev->ctx || !dev->ctx->bus) {
		return;
	}

	if (dev->connect_slot) {
		dev->connect_slot = sd_bus_slot_unref(dev->connect_slot);
	}

	if (dev->connected) {
		sd_bus_error error = SD_BUS_ERROR_NULL;
		int r;

		r = sd_bus_call_method(dev->ctx->bus, "org.bluez", dev->path,
							   "org.bluez.Device1", "Disconnect", &error, NULL,
							   "");

		if (r < 0) {
			LOG_ERR("BLZ: Failed to disconnect: %s", error.message);
		}

		sd_bus_error_free(&error);
	}

	/* free */
	for (int i = 0; dev->service_uuids != NULL && dev->service_uuids[i] != NULL;
		 i++) {
		free(dev->service_uuids[i]);
	}
	free(dev->service_uuids);

	free(dev);
}

void blz_serv_free(blz_serv* sv)
{
	if (!sv) {
		return;
	}
	for (int i = 0; sv->char_uuids != NULL && sv->char_uuids[i] != NULL; i++) {
		free(sv->char_uuids[i]);
	}
	free(sv->char_uuids);
	free(sv);
}

void blz_char_free(blz_char* ch)
{
	free(ch);
}

bool blz_loop_one(blz_ctx* ctx, uint32_t timeout_ms)
{
	if (!ctx || !ctx->bus) {
		return false;
	}

	int r = sd_bus_process(ctx->bus, NULL);
	if (r < 0) {
		LOG_ERR("BLZ: Loop process error: %s", strerror(-r));
		return false;
	}

	/* sd_bus_wait() should be called only if sd_bus_process() returned 0 */
	if (r > 0) {
		return true;
	}

	r = sd_bus_wait(ctx->bus, timeout_ms * 1000);
	if (r < 0 && -r != EINTR) {
		LOG_ERR("BLZ: Loop wait error: %s", strerror(-r));
	}
	return r >= 0;
}

/** return -1 on timeout, -2 on error */
int blz_loop_wait(blz_ctx* ctx, bool* check, uint32_t timeout_ms)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	uint32_t current_ms = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
	uint32_t end_ms = current_ms + timeout_ms;

	while (!*check && current_ms < end_ms) {
		if (!blz_loop_one(ctx, end_ms - current_ms)) {
			return -2;
		}
		clock_gettime(CLOCK_MONOTONIC, &ts);
		current_ms = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
	}

	return *check ? 0 : -1;
}

int blz_get_fd(blz_ctx* ctx)
{
	return sd_bus_get_fd(ctx->bus);
}

void blz_handle_read(blz_ctx* ctx)
{
	int r = sd_bus_process(ctx->bus, NULL);
	if (r < 0) {
		LOG_ERR("BLZ: Handle read process error: %s", strerror(-r));
		return;
	}
}
