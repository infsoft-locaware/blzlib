#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <systemd/sd-bus.h>

#include "blzlib.h"
#include "blzlib_internal.h"
#include "blzlib_log.h"

static int parse_msg_char_properties(sd_bus_message* m, const char* opath, blz_char* ch)
{
	int r;
	const char* str;
	const char* uuid;
	char** flags;

	/* array of dict entries */
	r = sd_bus_message_enter_container(m, 'a', "{sv}");
	if (r < 0) {
		LOG_ERR("parse msg 1");
		return r;
	}

	/* next dict */
	while ((r = sd_bus_message_enter_container(m, 'e', "sv")) > 0)
	{
		/* property name */
		r = sd_bus_message_read_basic(m, 's', &str);
		if (r < 0) {
			LOG_ERR("parse msg 2");
			return r;
		}

		//LOG_INF("Name %s", str);

		if (strcmp(str, "UUID") == 0) {
			r = sd_bus_message_enter_container(m, 'v', "s");
			if (r < 0) {
				LOG_ERR("parse msg 3");
				return r;
			}
			r = sd_bus_message_read_basic(m, 's', &uuid);
			if (r < 0) {
				LOG_ERR("parse msg 4");
				return r;
			}

			r = sd_bus_message_exit_container(m);
			if (r < 0) {
				LOG_ERR("parse msg 5");
				return r;
			}
		} else if (strcmp(str, "Flags") == 0) {
			r = sd_bus_message_enter_container(m, 'v', "as");
			if (r < 0) {
				LOG_ERR("parse msg 3");
				return r;
			}
			r = sd_bus_message_read_strv(m, &flags);
			if (r < 0) {
				LOG_ERR("parse msg 4");
				return r;
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

	//LOG_INF("UUID %s", uuid);
	if (ch->uuid[0] == '\0' || strcmp(uuid, ch->uuid) == 0) {
		strncpy(ch->path, opath, DBUS_PATH_MAX_LEN);
		if (ch->uuid[0] == '\0')
			strncpy(ch->uuid, uuid, UUID_STR_LEN);

		/* convert flags */
		for (int i = 0; flags != NULL && flags[i] != NULL; i++) {
			if (strcmp(flags[i], "broadcast") == 0) {
				ch->flags |= BLZ_CHAR_BROADCAST;
			} else if (strcmp(flags[i], "read") == 0) {
				ch->flags |= BLZ_CHAR_READ;
			} else if (strcmp(flags[i], "write-without-response") == 0) {
				ch->flags |= BLZ_CHAR_WRITE_WITHOUT_RESPONSE;
			} else if (strcmp(flags[i], "write") == 0) {
				ch->flags |= BLZ_CHAR_WRITE;
			} else if (strcmp(flags[i], "notify") == 0) {
				ch->flags |= BLZ_CHAR_NOTIFY;
			} else if (strcmp(flags[i], "indicate") == 0) {
				ch->flags |= BLZ_CHAR_INDICATE;
			}
			free(flags[i]);
		}

		return 1000;
	} else {
		/* just free flags */
		for (int i = 0; flags != NULL && flags[i] != NULL; i++) {
			free(flags[i]);
		}
	}

	return r;
}

int parse_msg_device_properties(sd_bus_message* m, const char* opath, blz* ctx)
{
	int r;
	const char* str;
	const char* mac;
	const char* name;
	char** uuids;

	/* array of dict entries */
	r = sd_bus_message_enter_container(m, 'a', "{sv}");
	if (r < 0) {
		LOG_ERR("parse msg 1");
		return r;
	}

	/* next dict */
	while ((r = sd_bus_message_enter_container(m, 'e', "sv")) > 0)
	{
		/* property name */
		r = sd_bus_message_read_basic(m, 's', &str);
		if (r < 0) {
			LOG_ERR("parse msg 2");
			return r;
		}

		//LOG_INF("Name %s", str);

		if (strcmp(str, "Name") == 0) {
			r = sd_bus_message_enter_container(m, 'v', "s");
			if (r < 0) {
				LOG_ERR("parse msg 3");
				return r;
			}
			r = sd_bus_message_read_basic(m, 's', &name);
			if (r < 0) {
				LOG_ERR("parse msg 4");
				return r;
			}

			r = sd_bus_message_exit_container(m);
			if (r < 0) {
				LOG_ERR("parse msg 5");
				return r;
			}
		}
		else if (strcmp(str, "Address") == 0) {
			r = sd_bus_message_enter_container(m, 'v', "s");
			if (r < 0) {
				LOG_ERR("parse msg 3");
				return r;
			}
			r = sd_bus_message_read_basic(m, 's', &mac);
			if (r < 0) {
				LOG_ERR("parse msg 4");
				return r;
			}

			r = sd_bus_message_exit_container(m);
			if (r < 0) {
				LOG_ERR("parse msg 5");
				return r;
			}
		}
		else if (strcmp(str, "UUIDs") == 0) {
			r = sd_bus_message_enter_container(m, 'v', "as");
			if (r < 0) {
				LOG_ERR("parse msg 3");
				return r;
			}

			/* need to free */
			r = sd_bus_message_read_strv(m, &uuids);
			if (r < 0) {
				LOG_ERR("parse msg 4");
				return r;
			}

			r = sd_bus_message_exit_container(m);
			if (r < 0) {
				LOG_ERR("parse msg 5");
				return r;
			}
		}
		else {
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

	/* callback */
	ctx->scan_cb(mac, name, uuids);

	/* free uuids */
	for (int i = 0; uuids != NULL && uuids[i] != NULL; i++) {
		free(uuids[i]);
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
	return r;
}

static int parse_msg_interfaces(sd_bus_message* m, enum e_obj eobj, const char* opath, void* user)
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
	while ((r = sd_bus_message_enter_container(m, 'e', "sa{sv}")) > 0)
	{
		r = sd_bus_message_read_basic(m, 's', &str);
		if (r < 0) {
			LOG_ERR("parse intf 2");
			return r;
		}

		if (eobj == OBJ_CHAR && strcmp(str, "org.bluez.GattCharacteristic1") == 0) {
			r = parse_msg_char_properties(m, opath, user);
			if (r == 1000) // found
				return r;
		}
		else if ( eobj == OBJ_DEVICE && strcmp(str, "org.bluez.Device1") == 0) {
			r = parse_msg_device_properties(m, opath, user);
		}
		else if (eobj == OBJ_CHAR_COUNT && strcmp(str, "org.bluez.GattCharacteristic1") == 0) {
			int* cnt = user;
			(*cnt)++;
			r = sd_bus_message_skip(m, "a{sv}");
			if (r < 0) {
				LOG_ERR("parse intf 3");
				return r;
			}
		}
		else if (eobj == OBJ_CHARS_ALL && strcmp(str, "org.bluez.GattCharacteristic1") == 0) {
			blz_dev* dev = user;
			blz_char ch;
			parse_msg_char_properties(m, opath, &ch);
			dev->char_uuids[dev->chars_idx] = strdup(ch.uuid);
			dev->chars_idx++;
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
	return r;
}

int parse_msg_one_object(sd_bus_message* m, const char* match_path, enum e_obj eobj, void* user)
{
	const char* opath;

	int r = sd_bus_message_read_basic(m, 'o', &opath);
	if (r < 0) {
		LOG_ERR("parse obj 2");
		return r;
	}

	//LOG_INF("O %s", opath);

	/* check if it is below our own device path, otherwise skip */
	if (strncmp(opath, match_path, strlen(match_path)) == 0) {
		r = parse_msg_interfaces(m, eobj, opath, user);
		if (r == 1000)
			return r;
	} else {
		r = sd_bus_message_skip(m, "a{sa{sv}}");
		if (r < 0) {
			LOG_ERR("parse obj 4");
			return r;
		}
	}
	return r;
}

int parse_msg_objects(sd_bus_message* m, const char* match_path, enum e_obj eobj, void* user)
{
	/* array of objects */
	int r = sd_bus_message_enter_container(m, 'a', "{oa{sa{sv}}}");
        if (r < 0) {
		LOG_ERR("parse obj 1");
                return r;
	}

	while ((r = sd_bus_message_enter_container(m, 'e', "oa{sa{sv}}")) > 0)
	{
		r = parse_msg_one_object(m, match_path, eobj, user);
		if (r < 0) {
			LOG_ERR("parse obj 3");
			return r;
		}
		if (r == 1000)
			return r;

		r = sd_bus_message_exit_container(m);
		if (r < 0) {
			LOG_ERR("parse obj 3");
			return r;
		}
	}

	if (r < 0) {
		LOG_ERR("parse obj 4");
		return r;
	}

	r = sd_bus_message_exit_container(m);
	if (r < 0) {
		LOG_ERR("parse obj 5");
	}
	return r;
}

int parse_msg_notify(sd_bus_message* m, const void** ptr, size_t* len)
{
	int r;
	char* str;

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
	r = sd_bus_message_read_array(m, 'y', ptr, len);
	if (r < 0) {
		LOG_ERR("BLZ signal msg read error");
		return -2;
	}
	return r;
}

int parse_msg_connect(sd_bus_message* m, blz_dev* dev)
{
	int r;
	char* str;

	/* interface */
	r = sd_bus_message_read_basic(m, 's', &str);
	if (r < 0) {
		LOG_ERR("BLZ signal msg read error");
		return -2;
	}

	/* ignore all other interfaces */
	if (strcmp(str, "org.bluez.Device1") != 0) {
		LOG_INF("BLZ signal interface %s ignored", str);
		return 0;
	}

	/* array of dict */
	r = sd_bus_message_enter_container(m, 'a', "{sv}");
	if (r < 0) {
		LOG_ERR("BLZ signal msg read error");
		return -2;
	}

	while ((r = sd_bus_message_enter_container(m, 'e', "sv")) > 0) {
		/* property name */
		r = sd_bus_message_read_basic(m, 's', &str);
		if (r < 0) {
			LOG_ERR("BLZ signal msg read error");
			return -2;
		}

		if (strcmp(str, "ServicesResolved") == 0) {
			r = sd_bus_message_enter_container(m, 'v', "b");
			if (r < 0) {
				LOG_ERR("BLZ msg read error");
				return -2;
			}

			/* note: bool in sd-dbus is expected to be int type */
			int b;
			r = sd_bus_message_read_basic(m, 'b', &b);
			if (r < 0) {
				LOG_ERR("parse read basic 1");
				return r;
			}

			LOG_INF("ServicesResolved %d", b);
			dev->connected = b;

			r = sd_bus_message_exit_container(m);
			if (r < 0) {
				LOG_ERR("parse obj ra3");
				return r;
			}
		}
		else {
			LOG_INF("BLZ conn property %s ignored", str);
			r = sd_bus_message_skip(m, "v");
			if (r < 0) {
				LOG_ERR("parse obj 4");
				return r;
			}
		}

		r = sd_bus_message_exit_container(m);
		if (r < 0) {
			LOG_ERR("parse obj 3");
			return r;
		}
	}

	if (r < 0) {
		LOG_ERR("BLZ signal msg read error");
		return -2;
	}

	return r;
}
