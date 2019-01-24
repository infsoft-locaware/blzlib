#include <stdlib.h>
#include <unistd.h>
#include <systemd/sd-bus.h>

#include <uwifi/log.h>

#include "blzlib.h"

struct blz_context {
	sd_bus*			bus;
};

struct blz_char {
	struct blz_context*	conn;
	blz_notify_handler_t	notify_cb;
};

blz* blz_init(void)
{
	struct blz_context* blz = calloc(1, sizeof(struct blz_context));
	if (blz == NULL) {
		LOG_ERR("blz_connect: alloc failed");
		return NULL;
	}

	/* Connect to the system bus */
	int r = sd_bus_open_system(&blz->bus);
	if (r < 0) {
		LOG_ERR("Failed to connect to system bus: %s", strerror(-r));
		return NULL;
	}
	return blz;
}

void blz_fini(blz* ctx)
{
	sd_bus_unref(ctx->bus);
	free(ctx);
}

bool blz_connect(blz* ctz, const char* mac)
{
}

void blz_disconnect(blz* conn)
{
	sd_bus_unref(conn->bus);
	free(conn);
}

bool blz_resolve_services(blz* conn)
{
	sd_bus_error error = SD_BUS_ERROR_NULL;
	sd_bus_message* reply = NULL;
	int r = sd_bus_call_method(conn->bus,
			"org.bluez",
			"/",
			"org.freedesktop.DBus.ObjectManager",
			"GetManagedObjects",
			&error, &reply, "");
	if (r < 0) {
		LOG_ERR("Failed to issue method call: %s", error.message);
		goto error;
	}

	LOG_INF("SIG %s", sd_bus_message_get_signature(reply, false));

	/* Parse the response message */
	r = sd_bus_message_enter_container(reply, 'a', "{oa{sa{sv}}}");
        if (r < 0) {
		LOG_ERR("0");
                goto error;
	}

	const char* opath;
	for (;;) {
		r = sd_bus_message_enter_container(reply, 'e', "oa{sa{sv}}");
		if (r < 0) {
			LOG_ERR("1");
			goto error;
		}
		if (r == 0) { /* Reached end of array */
			LOG_ERR("done");
                        break;
		}
		r = sd_bus_message_read_basic(reply, 'o', &opath);
		if (r < 0) {
			LOG_ERR("2");
			goto error;
		}
		LOG_INF("O %s", opath);
		
		r = sd_bus_message_skip(reply, "a{sa{sv}}");
		if (r < 0) {
			LOG_ERR("3");
			goto error;
		}

		r = sd_bus_message_exit_container(reply);
                if (r < 0) {
			LOG_ERR("4");
			goto error;
		}
	}

	r = sd_bus_message_exit_container(reply);
	return true;

error:
	sd_bus_error_free(&error);
	sd_bus_message_unref(reply);
	return false;
}

blz_char* blz_get_char_from_uuid(blz* conn, const char* uuid)
{
	char* uuidr;
	sd_bus_error error = SD_BUS_ERROR_NULL;

	int r = sd_bus_get_property_string(conn->bus,
			"org.bluez",
			"/org/bluez/hci0/dev_CF_D6_E8_4B_A0_D2/service000b/char000c",
			"org.bluez.GattCharacteristic1",
			"UUID", &error, &uuidr);

	LOG_INF("UUID %s", uuidr);

	struct blz_char* ch = calloc(1, sizeof(struct blz_char));
	if (ch == NULL) {
		LOG_ERR("blz_char: alloc failed");
		return NULL;
	}
	ch->conn = conn;
	return ch;
}

bool blz_char_write(blz_char* ch, const char* data, size_t len)
{
	sd_bus_error error = SD_BUS_ERROR_NULL;
	sd_bus_message* reply = NULL;
	sd_bus_message* m;
	int r;

	r = sd_bus_message_new_method_call(ch->conn->bus, &m,
		"org.bluez",
		"/org/bluez/hci0/dev_CF_D6_E8_4B_A0_D2/service000b/char000c",
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

	r = sd_bus_call(ch->conn->bus, m, 0, &error, &reply);
	if (r < 0) {
		LOG_ERR("Failed to issue method call: %s", error.message);
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

	sd_bus_match_signal(ch->conn->bus, &slot, "org.bluez",
		"/org/bluez/hci0/dev_CF_D6_E8_4B_A0_D2/service000b/char000e",
		"org.freedesktop.DBus.Properties",
		"PropertiesChanged",
		blz_signal_cb, ch);

	r = sd_bus_call_method(ch->conn->bus,
		"org.bluez",
		"/org/bluez/hci0/dev_CF_D6_E8_4B_A0_D2/service000b/char000e",
		"org.bluez.GattCharacteristic1",
		"StartNotify",
		&error, &reply, "");

	if (r < 0) {
		LOG_ERR("Failed to issue method call: %s", error.message);
	}
}

int blz_char_write_fd_acquire(blz_char* ch)
{
	sd_bus_error error = SD_BUS_ERROR_NULL;
	sd_bus_message* reply = NULL;
	int r;
	int fd;

	r = sd_bus_call_method(ch->conn->bus,
		"org.bluez",
		"/org/bluez/hci0/dev_CF_D6_E8_4B_A0_D2/service000b/char000c",
		"org.bluez.GattCharacteristic1",
		"AcquireWrite",
		&error, &reply,
		"a{sv}", 0);

	if (r < 0) {
		LOG_ERR("Failed to issue method call: %s", error.message);
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

int blz_char_write_fd_close(blz_char* ch)
{
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
