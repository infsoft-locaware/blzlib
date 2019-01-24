#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <systemd/sd-bus.h>

#include <uwifi/log.h>

#include "blzlib.h"

#define DBUS_PATH_MAX_LEN	255
#define UUID_STR_LEN		37

struct blz_context {
	sd_bus*			bus;
	char			path[DBUS_PATH_MAX_LEN];
};

struct blz_dev {
	struct blz_context*	ctx;
	char			path[DBUS_PATH_MAX_LEN];
};

struct blz_char {
	struct blz_context*	ctx;
	struct blz_dev*		dev;
	char			path[DBUS_PATH_MAX_LEN];
	char			uuid[UUID_STR_LEN];
	blz_notify_handler_t	notify_cb;
};

blz* blz_init(const char* dev)
{
	struct blz_context* ctx = calloc(1, sizeof(struct blz_context));
	if (ctx == NULL) {
		LOG_ERR("blz_connect: alloc failed");
		return NULL;
	}

	/* Connect to the system bus */
	int r = sd_bus_open_system(&ctx->bus);
	if (r < 0) {
		LOG_ERR("Failed to connect to system bus: %s", strerror(-r));
		free(ctx);
		return NULL;
	}

	snprintf(ctx->path, DBUS_PATH_MAX_LEN, "/org/bluez/%s", dev);

	sd_bus_error error = SD_BUS_ERROR_NULL;
	r = sd_bus_set_property(ctx->bus,
			"org.bluez", ctx->path,
			"org.bluez.Adapter1",
			"Powered",
			 &error, "b", 1);
	if (r < 0) {
		LOG_ERR("BLZ failed to power on: %s", strerror(-r));
		sd_bus_unref(ctx->bus);
		free(ctx);
		return NULL;
	}

	return ctx;
}

void blz_fini(blz* ctx)
{
	sd_bus_unref(ctx->bus);
	free(ctx);
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

	r = snprintf(dev->path, DBUS_PATH_MAX_LEN,
			 "%s/dev_%02X_%02X_%02X_%02X_%02X_%02X",
			ctx->path, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	if (r < 0) {
		free(dev);
		return NULL;
	}

	LOG_INF("dev %s", dev->path);

	r = sd_bus_call_method(ctx->bus,
			"org.bluez", dev->path,
			"org.bluez.Device1",
			"Connect",
			&error, NULL, "");

	if (r < 0)
		LOG_ERR("couldnt connect: %s", error.message);

	sd_bus_error_free(&error);

	if (r < 0) {
		free(dev);
		return NULL;
	}

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

	int r = sd_bus_call_method(dev->ctx->bus,
			"org.bluez", dev->path,
			"org.bluez.Device1",
			"Disconnect",
			&error, NULL, "");

	if (r < 0)
		LOG_ERR("couldnt disconnect: %s", error.message);

	sd_bus_error_free(&error);
}

static int parse_msg_char_properties(sd_bus_message* m, const char* opath, blz_char* ch)
{
	int r;
	const char* str;

	/* array of dict entries */
	r = sd_bus_message_enter_container(m, 'a', "{sv}");
	if (r < 0) {
		LOG_ERR("parse msg 1");
		return r;
	}

	/* next dict */
	while (r = sd_bus_message_enter_container(m, 'e', "sv") > 0)
	{
		/* property name */
		r = sd_bus_message_read_basic(m, 's', &str);
		if (r < 0) {
			LOG_ERR("parse msg 2");
			return r;
		}
		//LOG_INF("Name %s", opath);

		if (strcmp(str, "UUID") == 0) {
			r = sd_bus_message_enter_container(m, 'v', "s");
			if (r < 0) {
				LOG_ERR("parse msg 3");
				return r;
			}
			r = sd_bus_message_read_basic(m, 's', &str);
			if (r < 0) {
				LOG_ERR("parse msg 4");
				return r;
			}

			//LOG_INF("UUID %s", str);
			if (strcmp(str, ch->uuid) == 0) {
				strncpy(ch->path, opath, DBUS_PATH_MAX_LEN);
				return 1000;
			}

			r = sd_bus_message_exit_container(m);
			if (r < 0) {
				LOG_ERR("parse msg 5");
				return r;
			}
		} else {
			r = sd_bus_message_skip(m, "v");
			if (r < 0) {
				LOG_ERR("parse msg 6");
				return r;
			}
		}

		r = sd_bus_message_exit_container(m);
		if (r < 0) {
			LOG_ERR("parse msg 7");
			return r;
		}
	}

	if (r < 0) {
		LOG_ERR("parse msg 8");
		return r;
	}

	r = sd_bus_message_exit_container(m);
	if (r < 0) {
		LOG_ERR("parse msg 9");
		return r;
	}
}


int parse_msg_interfaces(sd_bus_message* m, const char* opath, blz_char* ch)
{
	int r;
	const char* str;

	/* array of interface names with array of properties */
	r = sd_bus_message_enter_container(m, 'a', "{sa{sv}}");
	if (r < 0) {
		LOG_ERR("parse intf 1");
		return r;
	}

	/* next entry */
	while (r = sd_bus_message_enter_container(m, 'e', "sa{sv}") > 0)
	{
		r = sd_bus_message_read_basic(m, 's', &str);
		if (r < 0) {
			LOG_ERR("parse intf 2");
			return r;
		}

		if (strcmp(str, "org.bluez.GattCharacteristic1") == 0) {
			r = parse_msg_char_properties(m, opath, ch);
			if (r == 1000) // found
				return r;
		}
		else {
			r = sd_bus_message_skip(m, "a{sv}");
			if (r < 0) {
				LOG_ERR("parse intf 3");
				return r;
			}
		}

		r = sd_bus_message_exit_container(m);
		if (r < 0) {
			LOG_ERR("parse intf 4");
			return r;
		}
	}

	if (r < 0) {
		LOG_ERR("parse intf 5");
		return r;
	}

	r = sd_bus_message_exit_container(m);
        if (r < 0) {
		LOG_ERR("parse intf 6");
		return r;
	}
}

static bool find_char(blz_char* ch)
{
	const char* opath;
	sd_bus_error error = SD_BUS_ERROR_NULL;
	sd_bus_message* reply = NULL;

	int r = sd_bus_call_method(ch->ctx->bus,
			"org.bluez", "/",
			"org.freedesktop.DBus.ObjectManager",
			"GetManagedObjects",
			&error, &reply, "");
	if (r < 0) {
		LOG_ERR("Failed to get managed objects: %s", error.message);
		goto error;
	}

	/* array of objects */
	r = sd_bus_message_enter_container(reply, 'a', "{oa{sa{sv}}}");
        if (r < 0) {
		LOG_ERR("parse obj 1");
                goto error;
	}

	while (r = sd_bus_message_enter_container(reply, 'e', "oa{sa{sv}}") > 0)
	{
		r = sd_bus_message_read_basic(reply, 'o', &opath);
		if (r < 0) {
			LOG_ERR("parse obj 2");
			goto error;
		}

		LOG_INF("O %s", opath);

		/* check if it is below our own device path, otherwise skip */
		if (strncmp(opath, ch->dev->path, strlen(ch->dev->path)) == 0) {
			r = parse_msg_interfaces(reply, opath, ch);
			if (r == 1000)
				goto error; // exit
		} else {
			r = sd_bus_message_skip(reply, "a{sa{sv}}");
			if (r < 0) {
				LOG_ERR("parse obj 4");
				return r;
			}
		}

		r = sd_bus_message_exit_container(reply);
                if (r < 0) {
			LOG_ERR("parse obj 3");
			goto error;
		}
	}

	if (r < 0) {
		LOG_ERR("parse obj 4");
		goto error;
	}

	r = sd_bus_message_exit_container(reply);
	if (r < 0) {
		LOG_ERR("parse obj 5");
		goto error;
	}

error:
	sd_bus_error_free(&error);
	sd_bus_message_unref(reply);
	return r == 1000 ? true : false;
}

blz_char* blz_get_char_from_uuid(blz_dev* dev, const char* uuid)
{
	char* uuidr;
	sd_bus_error error = SD_BUS_ERROR_NULL;

	struct blz_char* ch = calloc(1, sizeof(struct blz_char));
	if (ch == NULL) {
		LOG_ERR("blz_char: alloc failed");
		return NULL;
	}
	ch->ctx = dev->ctx;
	ch->dev = dev;
	strncpy(ch->uuid, uuid, UUID_STR_LEN);

	bool b = find_char(ch);
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
	sd_bus_message* reply = NULL;
	sd_bus_message* m;
	int r;

	r = sd_bus_message_new_method_call(ch->ctx->bus, &m,
		"org.bluez", ch->path,
		"org.bluez.GattCharacteristic1",
		"WriteValue");
	if (r < 0) {
		LOG_ERR("a");
		goto error;
	}

	r = sd_bus_message_append_array(m, 'y', data, len);
	if (r < 0) {
		LOG_ERR("a");
		goto error;
	}
	r = sd_bus_message_open_container(m, 'a', "{sv}");
	if (r < 0) {
		LOG_ERR("a");
		goto error;
	}
	
	r = sd_bus_message_close_container(m);
	if (r < 0) {
		LOG_ERR("a");
		goto error;
	}

	r = sd_bus_call(ch->ctx->bus, m, 0, &error, &reply);
	if (r < 0) {
		LOG_ERR("BLZ failed to write: %s", error.message);
		goto error;
	}

	return true;

error:
	sd_bus_error_free(&error);
	sd_bus_message_unref(reply);
	return false;
}

bool blz_char_read(blz_char* ch, const char* data, size_t len)
{
}

static int blz_signal_cb(sd_bus_message* m, void* user, sd_bus_error* err)
{
	int r;
	char* str;
	const void* v;
	size_t len;
	struct blz_char* ch = user;

	if (ch->notify_cb == NULL) {
		LOG_ERR("no cb");
		return 0;
	}

	r = sd_bus_message_read_basic(m, 's', &str);
	if (r < 0)
		LOG_INF("err1");
	
	if (strcmp(str, "org.bluez.GattCharacteristic1") != 0) {
		LOG_INF("SIGNAL intf %s ignored", str);
		return 0;
	}

	r = sd_bus_message_enter_container(m, 'a', "{sv}");
	if (r < 0)
		LOG_INF("err2");

	r = sd_bus_message_enter_container(m, 'e', "sv");
	if (r < 0)
		LOG_INF("err3");

	r = sd_bus_message_read_basic(m, 's', &str);
	if (r < 0)
		LOG_INF("err4");
	
	if (strcmp(str, "Value") != 0) {
		LOG_INF("SIGNAL Property %s ignored", str);
		return 0;
	}

	r = sd_bus_message_enter_container(m, 'v', "ay");
	if (r < 0)
		LOG_INF("err41");

	r = sd_bus_message_read_array(m, 'y', &v, &len);
	if (r < 0)
		LOG_INF("err5");

	const char* buf = v;
	LOG_INF("SIGNAL '%.*s'", (int)len-1, buf);
	ch->notify_cb(v, len, ch);
}

bool blz_char_notify(blz_char* ch, blz_notify_handler_t cb)
{
	sd_bus_error error = SD_BUS_ERROR_NULL;
	sd_bus_message* reply = NULL;
	sd_bus_slot *slot;
	int r;
	
	ch->notify_cb = cb;

	sd_bus_match_signal(ch->ctx->bus, &slot,
		"org.bluez", ch->path,
		"org.freedesktop.DBus.Properties",
		"PropertiesChanged",
		blz_signal_cb, ch);

	r = sd_bus_call_method(ch->ctx->bus,
		"org.bluez", ch->path,
		"org.bluez.GattCharacteristic1",
		"StartNotify",
		&error, &reply, "");

	if (r < 0) {
		LOG_ERR("BLZ Failed to start notify: %s", error.message);
	}
}

int blz_char_write_fd_acquire(blz_char* ch)
{
	sd_bus_error error = SD_BUS_ERROR_NULL;
	sd_bus_message* reply = NULL;
	int r;
	int fd;

	r = sd_bus_call_method(ch->ctx->bus,
		"org.bluez", ch->path,
		"org.bluez.GattCharacteristic1",
		"AcquireWrite",
		&error, &reply,
		"a{sv}", 0);

	if (r < 0) {
		LOG_ERR("BLZ Failed acquire write: %s", error.message);
		goto error;
	}

	r = sd_bus_message_read(reply, "h", &fd);
	if (r < 0)
		LOG_ERR("fd");

	return dup(fd);
error:
	sd_bus_error_free(&error);
	sd_bus_message_unref(reply);
	return -1;
}

void blz_loop(blz* conn)
{
	sd_bus_message *m = NULL;
	int r = sd_bus_process(conn->bus, &m);
	if (r < 0) {
		//error handling
	}

	r = sd_bus_wait(conn->bus, (uint64_t)-1);
	if (r < 0) {
		//error handling
	}
}
