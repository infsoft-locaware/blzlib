#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <systemd/sd-bus.h>

#include "blzlib.h"
#include "blzlib_internal.h"
#include "blzlib_log.h"

static int msg_parse_characteristic1(sd_bus_message* m, const char* opath, blz_char* ch)
{
	const char* str;
	const char* uuid;
	char** flags;

	/* enter array of dict entries */
	int r = sd_bus_message_enter_container(m, 'a', "{sv}");
	if (r < 0) {
		LOG_ERR("BLZ error parse char 1");
		return r;
	}

	/* enter next dict */
	while ((r = sd_bus_message_enter_container(m, 'e', "sv")) > 0)
	{
		/* property name */
		r = sd_bus_message_read_basic(m, 's', &str);
		if (r < 0) {
			LOG_ERR("BLZ error parse char 2");
			return r;
		}

		//LOG_INF("Name %s", str);

		if (strcmp(str, "UUID") == 0) {
			r = sd_bus_message_enter_container(m, 'v', "s");
			if (r < 0) {
				LOG_ERR("BLZ error parse char 3");
				return r;
			}

			r = sd_bus_message_read_basic(m, 's', &uuid);
			if (r < 0) {
				LOG_ERR("BLZ error parse char 4");
				return r;
			}

			r = sd_bus_message_exit_container(m);
			if (r < 0) {
				LOG_ERR("BLZ error parse char 5");
				return r;
			}
		} else if (strcmp(str, "Flags") == 0) {
			r = sd_bus_message_enter_container(m, 'v', "as");
			if (r < 0) {
				LOG_ERR("BLZ error parse char 6");
				return r;
			}

			r = sd_bus_message_read_strv(m, &flags);
			if (r < 0) {
				LOG_ERR("BLZ error parse char 7");
				return r;
			}

			r = sd_bus_message_exit_container(m);
			if (r < 0) {
				LOG_ERR("BLZ error parse char 8");
				return r;
			}
		} else {
			r = sd_bus_message_skip(m, "v");
			if (r < 0) {
				LOG_ERR("BLZ error parse char 9");
				return r;
			}
		}

		/* exit dict */
		r = sd_bus_message_exit_container(m);
		if (r < 0) {
			LOG_ERR("BLZ error parse char 10");
			return r;
		}
	}

	if (r < 0) {
		LOG_ERR("BLZ error parse char 11");
		return r;
	}

	/* exit array */
	r = sd_bus_message_exit_container(m);
	if (r < 0) {
		LOG_ERR("BLZ error parse char 12");
		return r;
	}

	//LOG_INF("UUID %s", uuid);

	/* if UUID matched or if UUID was empty (match all) */
	if (ch->uuid[0] == '\0' || strcmp(uuid, ch->uuid) == 0) {
		/* save object path and UUID */
		strncpy(ch->path, opath, DBUS_PATH_MAX_LEN);
		if (ch->uuid[0] == '\0') { // TODO better check
			strncpy(ch->uuid, uuid, UUID_STR_LEN);
		}

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

		return RETURN_FOUND;
	} else {
		/* just free flags */
		for (int i = 0; flags != NULL && flags[i] != NULL; i++) {
			free(flags[i]);
		}
	}

	return r;
}

static int msg_parse_device1(sd_bus_message* m, const char* opath, blz_dev* dev)
{
	const char* str;

	/* enter array of dict entries */
	int r = sd_bus_message_enter_container(m, 'a', "{sv}");
	if (r < 0) {
		LOG_ERR("BLZ error parse dev 1");
		return r;
	}

	/* enter next dict */
	while ((r = sd_bus_message_enter_container(m, 'e', "sv")) > 0)
	{
		/* property name */
		r = sd_bus_message_read_basic(m, 's', &str);
		if (r < 0) {
			LOG_ERR("BLZ error parse dev 2");
			return r;
		}

		//LOG_INF("Name %s", str);

		if (strcmp(str, "Name") == 0) {
			r = sd_bus_message_enter_container(m, 'v', "s");
			if (r < 0) {
				LOG_ERR("BLZ error parse dev 3");
				return r;
			}

			r = sd_bus_message_read_basic(m, 's', &str);
			if (r < 0) {
				LOG_ERR("BLZ error parse dev 4");
				return r;
			}

			strncpy(dev->name, str, NAME_STR_LEN);

			r = sd_bus_message_exit_container(m);
			if (r < 0) {
				LOG_ERR("BLZ error parse dev 5");
				return r;
			}
		}
		else if (strcmp(str, "Address") == 0) {
			r = sd_bus_message_enter_container(m, 'v', "s");
			if (r < 0) {
				LOG_ERR("BLZ error parse dev 6");
				return r;
			}

			r = sd_bus_message_read_basic(m, 's', &str);
			if (r < 0) {
				LOG_ERR("BLZ error parse dev 7");
				return r;
			}

			strncpy(dev->mac, str, MAC_STR_LEN);

			r = sd_bus_message_exit_container(m);
			if (r < 0) {
				LOG_ERR("BLZ error parse dev 8");
				return r;
			}
		}
		else if (strcmp(str, "UUIDs") == 0) {
			r = sd_bus_message_enter_container(m, 'v', "as");
			if (r < 0) {
				LOG_ERR("BLZ error parse dev 9");
				return r;
			}

			/* need to free later */
			r = sd_bus_message_read_strv(m, &dev->service_uuids);
			if (r < 0) {
				LOG_ERR("BLZ error parse dev 10");
				return r;
			}

			r = sd_bus_message_exit_container(m);
			if (r < 0) {
				LOG_ERR("BLZ error parse dev 11");
				return r;
			}
		}
		else if (strcmp(str, "ServicesResolved") == 0) {
			r = sd_bus_message_enter_container(m, 'v', "b");
			if (r < 0) {
				LOG_ERR("BLZ error parse dev 12");
				return -2;
			}

			/* note: bool in sd-dbus is expected to be int type */
			int b;
			r = sd_bus_message_read_basic(m, 'b', &b);
			if (r < 0) {
				LOG_ERR("BLZ error parse dev 13");
				return r;
			}

			dev->services_resolved = b;

			r = sd_bus_message_exit_container(m);
			if (r < 0) {
				LOG_ERR("BLZ error parse dev 14");
				return r;
			}
		}
		else {
			r = sd_bus_message_skip(m, "v");
			if (r < 0) {
				LOG_ERR("BLZ error parse dev 15");
				return r;
			}
		}

		/* exit dict */
		r = sd_bus_message_exit_container(m);
		if (r < 0) {
			LOG_ERR("BLZ error parse dev 16");
			return r;
		}
	}

	if (r < 0) {
		LOG_ERR("BLZ error parse dev 17");
		return r;
	}

	/* exit array */
	r = sd_bus_message_exit_container(m);
	if (r < 0) {
		LOG_ERR("BLZ error parse dev 18");
		return r;
	}

	return r;
}

int msg_parse_interface(sd_bus_message* m, enum e_obj eobj, const char* opath, void* user)
{
	char* intf;

	/* interface name */
	int r = sd_bus_message_read_basic(m, 's', &intf);
	if (r < 0) {
		LOG_ERR("BLZ error parse 1intf 1");
		return r;
	}

	if (eobj == OBJ_CHAR && strcmp(intf, "org.bluez.GattCharacteristic1") == 0) {
		r = msg_parse_characteristic1(m, opath, user);
		if (r < 0) {
			LOG_ERR("BLZ error in parse_msg_char_properties");
			return r;
		} else if (r == RETURN_FOUND) {
			return r;
		}
	}
	else if (eobj == OBJ_DEVICE && strcmp(intf, "org.bluez.Device1") == 0) {
		r = msg_parse_device1(m, opath, user);
		if (r < 0) {
			LOG_ERR("BLZ error in parse_msg_device_properties");
			return r;
		}
	}
	else if (eobj == OBJ_DEVICE_SCAN && strcmp(intf, "org.bluez.Device1") == 0) {
		blz_dev dev; // temporary device
		r = msg_parse_device1(m, opath, &dev);
		if (r < 0) {
			LOG_ERR("BLZ error in parse_msg_device_properties");
			return r;
		}

		/* callback */
		blz* ctx = user;
		if (ctx != NULL && ctx->scan_cb != NULL) {
			ctx->scan_cb(dev.mac, dev.name, dev.service_uuids);
		}

		/* free uuids */
		for (int i = 0; dev.service_uuids != NULL && dev.service_uuids[i] != NULL; i++) {
			free(dev.service_uuids[i]);
		}
	}
	else if (eobj == OBJ_CHAR_COUNT && strcmp(intf, "org.bluez.GattCharacteristic1") == 0) {
		int* cnt = user;
		(*cnt)++;
		r = sd_bus_message_skip(m, "a{sv}");
		if (r < 0) {
			LOG_ERR("BLZ error parse 1intf 2");
			return r;
		}
	}
	else if (eobj == OBJ_CHARS_ALL && strcmp(intf, "org.bluez.GattCharacteristic1") == 0) {
		blz_dev* dev = user;
		blz_char ch; // temporary char
		r = msg_parse_characteristic1(m, opath, &ch);
		if (r < 0) {
			LOG_ERR("BLZ error in parse_msg_char_properties");
			return r;
		}
		/* copy UUID */
		dev->char_uuids[dev->chars_idx] = strdup(ch.uuid);
		dev->chars_idx++;
	}
	else {
		/* unknown interface or action */
		r = sd_bus_message_skip(m, "a{sv}");
		if (r < 0) {
			LOG_ERR("BLZ error parse 1intf 3");
			return r;
		}
	}
	return r;
}

static int msg_parse_interfaces(sd_bus_message* m, enum e_obj eobj, const char* opath, void* user)
{
	/* enter array of interface names with array of properties */
	int r = sd_bus_message_enter_container(m, 'a', "{sa{sv}}");
	if (r < 0) {
		LOG_ERR("BLZ error parse intf 1");
		return r;
	}

	/* enter next dict entry */
	while ((r = sd_bus_message_enter_container(m, 'e', "sa{sv}")) > 0)
	{
		r = msg_parse_interface(m, eobj, opath, user);
		if (r < 0) {
			LOG_ERR("BLZ error in parse_msg_one_interface");
			return r;
		} else if (r == RETURN_FOUND) {
			return r;
		}

		/* exit dict */
		r = sd_bus_message_exit_container(m);
		if (r < 0) {
			LOG_ERR("BLZ error parse intf 2");
			return r;
		}
	}

	if (r < 0) {
		LOG_ERR("BLZ error parse intf 3");
		return r;
	}

	/* exit array */
	r = sd_bus_message_exit_container(m);
        if (r < 0) {
		LOG_ERR("BLZ error parse intf 4");
		return r;
	}
	return r;
}

int msg_parse_object(sd_bus_message* m, const char* match_path, enum e_obj eobj, void* user)
{
	const char* opath;

	/* object path */
	int r = sd_bus_message_read_basic(m, 'o', &opath);
	if (r < 0) {
		LOG_ERR("BLZ error parse 1obj 1");
		return r;
	}

	/* check if it is below our own device path */
	if (strncmp(opath, match_path, strlen(match_path)) == 0) {
		/* parse array of interfaces */
		r = msg_parse_interfaces(m, eobj, opath, user);
		if (r < 0) {
			LOG_ERR("BLZ error in parse_msg_interfaces");
			return r;
		} else if (r == RETURN_FOUND) {
			return r;
		}
	} else {
		/* ignore */
		r = sd_bus_message_skip(m, "a{sa{sv}}");
		if (r < 0) {
			LOG_ERR("BLZ error parse 1obj 2");
			return r;
		}
	}

	return r;
}

int msg_parse_objects(sd_bus_message* m, const char* match_path, enum e_obj eobj, void* user)
{
	/* enter array of objects */
	int r = sd_bus_message_enter_container(m, 'a', "{oa{sa{sv}}}");
        if (r < 0) {
		LOG_ERR("BLZ error parse obj 1");
                return r;
	}

	/* enter next dict/object */
	while ((r = sd_bus_message_enter_container(m, 'e', "oa{sa{sv}}")) > 0)
	{
		r = msg_parse_object(m, match_path, eobj, user);
		if (r < 0) {
			LOG_ERR("BLZ error in parse_msg_one_object");
			return r;
		} else if (r == RETURN_FOUND) {
			return r;
		}

		/* exit dict */
		r = sd_bus_message_exit_container(m);
		if (r < 0) {
			LOG_ERR("BLZ error parse obj 2");
			return r;
		}
	}

	if (r < 0) {
		LOG_ERR("BLZ error parse obj 3");
		return r;
	}

	/* exit array */
	r = sd_bus_message_exit_container(m);
	if (r < 0) {
		LOG_ERR("BLZ error parse obj 4");
	}
	return r;
}

int msg_parse_notify(sd_bus_message* m, blz_char* ch, const void** ptr, size_t* len)
{
	int r;
	char* str;

	/* interface name */
	r = sd_bus_message_read_basic(m, 's', &str);
	if (r < 0) {
		LOG_ERR("BLZ error parse notify 1");
		return -2;
	}

	/* ignore all other interfaces */
	if (strcmp(str, "org.bluez.GattCharacteristic1") != 0) {
		LOG_INF("BLZ notify interface %s ignored", str);
		return 0;
	}

	/* enter array of dict entries */
	r = sd_bus_message_enter_container(m, 'a', "{sv}");
	if (r < 0) {
		LOG_ERR("BLZ error parse notify 2");
		return -2;
	}

	/* enter first element */
	r = sd_bus_message_enter_container(m, 'e', "sv");
	if (r < 0) {
		LOG_ERR("BLZ error parse notify 3");
		return -2;
	}

	/* property name */
	r = sd_bus_message_read_basic(m, 's', &str);
	if (r < 0) {
		LOG_ERR("BLZ error parse notify 4");
		return -2;
	}

	/* ignore all except Value */
	if (strcmp(str, "Notifying") == 0) {
		r = sd_bus_message_enter_container(m, 'v', "b");
		if (r < 0) {
			LOG_ERR("BLZ error parse notify 5");
			return -2;
		}

		/* note: bool in sd-dbus is expected to be int type */
		int b;
		r = sd_bus_message_read_basic(m, 'b', &b);
		if (r < 0) {
			LOG_ERR("BLZ error parse notify 6");
			return -2;
		}

		ch->notifying = b;
	} else if (strcmp(str, "Value") == 0) {
		/* enter variant */
		r = sd_bus_message_enter_container(m, 'v', "ay");
		if (r < 0) {
			LOG_ERR("BLZ error parse notify 7");
			return -2;
		}

		/* get byte array */
		r = sd_bus_message_read_array(m, 'y', ptr, len);
		if (r < 0) {
			LOG_ERR("BLZ error parse notify 8");
			return -2;
		}
	} else {
		LOG_INF("BLZ notify property %s ignored", str);
		return 0;
	}

	/* no need to exit containers as we stop parsing */
	return r;
}
