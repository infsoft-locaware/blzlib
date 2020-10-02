/*
 * blzlib - Copyright (C) 2019 Bruno Randolf (br1@einfach.org)
 *
 * This source code is licensed under the GNU Lesser General Public License,
 * Version 3. See the file COPYING for more details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <systemd/sd-bus.h>

#include "blzlib.h"
#include "blzlib_internal.h"
#include "blzlib_log.h"
#include "blzlib_util.h"

static int msg_parse_characteristic1(sd_bus_message* m, const char* opath,
									 blz_char* ch)
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
	while ((r = sd_bus_message_enter_container(m, 'e', "sv")) > 0) {
		/* property name */
		r = sd_bus_message_read_basic(m, 's', &str);
		if (r < 0) {
			LOG_ERR("BLZ error parse char 2");
			return r;
		}

		// LOG_INF("Name %s", str);

		if (strcmp(str, "UUID") == 0) {
			r = msg_read_variant(m, "s", &uuid);
			if (r < 0) {
				return r;
			}
		} else if (strcmp(str, "Flags") == 0) {
			r = msg_read_variant_strv(m, &flags);
			if (r < 0) {
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

	// LOG_INF("UUID %s", uuid);

	/* if UUID matched or if UUID was empty (match all) */
	if (ch->uuid[0] == '\0' || strcasecmp(uuid, ch->uuid) == 0) {
		/* save object path and UUID */
		strncpy(ch->path, opath, DBUS_PATH_MAX_LEN);
		if (ch->uuid[0] == '\0') {
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
		free(flags);

		return RETURN_FOUND;
	} else {
		/* just free flags */
		for (int i = 0; flags != NULL && flags[i] != NULL; i++) {
			free(flags[i]);
		}
		free(flags);
	}

	return r;
}

static int msg_parse_service1(sd_bus_message* m, const char* opath,
							  blz_serv* srv)
{
	const char* str;
	const char* uuid;

	/* enter array of dict entries */
	int r = sd_bus_message_enter_container(m, 'a', "{sv}");
	if (r < 0) {
		LOG_ERR("BLZ error parse serv 1");
		return r;
	}

	/* enter next dict */
	while ((r = sd_bus_message_enter_container(m, 'e', "sv")) > 0) {
		/* property name */
		r = sd_bus_message_read_basic(m, 's', &str);
		if (r < 0) {
			LOG_ERR("BLZ error parse serv 2");
			return r;
		}

		// LOG_INF("serv Name %s", str);

		if (strcmp(str, "UUID") == 0) {
			r = msg_read_variant(m, "s", &uuid);
			if (r < 0) {
				LOG_ERR("BLZ error parse serv 3");
				return r;
			}
		} else {
			r = sd_bus_message_skip(m, "v");
			if (r < 0) {
				LOG_ERR("BLZ error parse serv 4");
				return r;
			}
		}

		/* exit dict */
		r = sd_bus_message_exit_container(m);
		if (r < 0) {
			LOG_ERR("BLZ error parse serv 5");
			return r;
		}
	}

	if (r < 0) {
		LOG_ERR("BLZ error parse serv 6");
		return r;
	}

	/* exit array */
	r = sd_bus_message_exit_container(m);
	if (r < 0) {
		LOG_ERR("BLZ error parse serv 7");
		return r;
	}

	// LOG_INF("Serv UUID %s path %s", uuid, opath);

	/* if UUID matched or if UUID was empty (match all) */
	if (srv->uuid[0] == '\0' || strcasecmp(uuid, srv->uuid) == 0) {
		/* save object path and UUID */
		strncpy(srv->path, opath, DBUS_PATH_MAX_LEN);
		if (srv->uuid[0] == '\0') {
			strncpy(srv->uuid, uuid, UUID_STR_LEN);
		}

		return RETURN_FOUND;
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
	while ((r = sd_bus_message_enter_container(m, 'e', "sv")) > 0) {
		/* property name */
		r = sd_bus_message_read_basic(m, 's', &str);
		if (r < 0) {
			LOG_ERR("BLZ error parse dev 2");
			return r;
		}

		// LOG_INF("Name %s", str);

		if (strcmp(str, "Name") == 0) {
			r = msg_read_variant(m, "s", &str);
			if (r < 0) {
				return r;
			}
			strncpy(dev->name, str, NAME_STR_LEN);
		} else if (strcmp(str, "Address") == 0) {
			r = msg_read_variant(m, "s", &str);
			if (r < 0) {
				return r;
			}
			blz_string_to_mac(str, dev->mac);
		} else if (strcmp(str, "UUIDs") == 0) {
			r = msg_read_variant_strv(m, &dev->service_uuids);
			if (r < 0) {
				return r;
			}
		} else if (strcmp(str, "ServicesResolved") == 0) {
			/* note: bool in sd-dbus is expected to be int type */
			int b;
			r = msg_read_variant(m, "b", &b);
			if (r < 0) {
				return r;
			}
			dev->services_resolved = b;
		} else if (strcmp(str, "Connected") == 0) {
			/* note: bool in sd-dbus is expected to be int type */
			int b;
			r = msg_read_variant(m, "b", &b);
			if (r < 0) {
				return r;
			}
			dev->connected = b;
			if (dev->disconnect_cb && !b) {
				dev->disconnect_cb(dev->disconn_user);
			}
		} else if (strcmp(str, "RSSI") == 0) {
			r = msg_read_variant(m, "n", &dev->rssi);
			if (r < 0) {
				return r;
			}
		} else {
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

int msg_parse_interface(sd_bus_message* m, enum msg_act act, const char* opath,
						void* user)
{
	const char* intf;

	/* interface name */
	int r = sd_bus_message_read_basic(m, 's', &intf);
	if (r < 0) {
		LOG_ERR("BLZ error parse 1intf 1");
		return r;
	}

	if (act == MSG_SERV_FIND && strcmp(intf, "org.bluez.GattService1") == 0) {
		/* find service by UUID, returns RETURN_FOUND if found, user
		 * points to a blz_serv where the UUID to look for is filled */
		r = msg_parse_service1(m, opath, user);
	} else if (act == MSG_CHAR_FIND
			   && strcmp(intf, "org.bluez.GattCharacteristic1") == 0) {
		/* find char by UUID, returns RETURN_FOUND if found, user
		 * points to a blz_char where the UUID to look for is filled */
		r = msg_parse_characteristic1(m, opath, user);
	} else if (act == MSG_CHAR_COUNT
			   && strcmp(intf, "org.bluez.GattCharacteristic1") == 0) {
		/* just count the times the GattCharacteristic1 interface was
		 * found, user just is an int pointer */
		int* cnt = user;
		(*cnt)++;
		r = sd_bus_message_skip(m, "a{sv}");
		if (r < 0) {
			LOG_ERR("BLZ error parse 1intf 2");
		}
	} else if (act == MSG_CHARS_ALL
			   && strcmp(intf, "org.bluez.GattCharacteristic1") == 0) {
		/* get UUIDs from all characteristics. user points to the service
		 * where enough space for them has already been allocated */
		blz_serv* srv = user;
		blz_char ch = {0}; // temporary char
		r = msg_parse_characteristic1(m, opath, &ch);
		if (r < 0) {
			return r;
		}
		/* copy UUID */
		srv->char_uuids[srv->chars_idx] = strdup(ch.uuid);
		srv->chars_idx++;
		return 0; // override RETURN_FOUND this would stop the loop
	} else if (act == MSG_DEVICE && strcmp(intf, "org.bluez.Device1") == 0) {
		/* parse device properties, user points to device */
		r = msg_parse_device1(m, opath, user);
	} else if (act == MSG_DEVICE_SCAN
			   && strcmp(intf, "org.bluez.Device1") == 0) {
		/* used in scan callback. user points to a blz* where the scan_cb
		 * can be found. create a temporary device, parse all info into
		 * it and then call callback */
		blz_dev dev = {0};
		r = msg_parse_device1(m, opath, &dev);
		if (r < 0) {
			return r;
		}

		/* callback */
		blz_ctx* ctx = user;
		if (ctx != NULL && ctx->scan_cb != NULL) {
			ctx->scan_cb(dev.mac, BLZ_ADDR_UNKNOWN, dev.rssi, NULL, 0, ctx->scan_user);
		}

		/* free uuids of temporary device */
		for (int i = 0;
			 dev.service_uuids != NULL && dev.service_uuids[i] != NULL; i++) {
			free(dev.service_uuids[i]);
		}
		free(dev.service_uuids);
	} else {
		/* unknown interface or action */
		r = sd_bus_message_skip(m, "a{sv}");
		if (r < 0) {
			LOG_ERR("BLZ error parse 1intf 3");
		}
	}
	return r;
}

static int msg_parse_interfaces(sd_bus_message* m, enum msg_act act,
								const char* opath, void* user)
{
	/* enter array of interface names with array of properties */
	int r = sd_bus_message_enter_container(m, 'a', "{sa{sv}}");
	if (r < 0) {
		LOG_ERR("BLZ error parse intf 1");
		return r;
	}

	/* enter next dict entry */
	while ((r = sd_bus_message_enter_container(m, 'e', "sa{sv}")) > 0) {
		r = msg_parse_interface(m, act, opath, user);
		if (r < 0 || r == RETURN_FOUND) {
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

int msg_parse_object(sd_bus_message* m, const char* match_path,
					 enum msg_act act, void* user)
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
		r = msg_parse_interfaces(m, act, opath, user);
	} else {
		/* ignore */
		r = sd_bus_message_skip(m, "a{sa{sv}}");
		if (r < 0) {
			LOG_ERR("BLZ error parse 1obj 2");
		}
	}

	return r;
}

int msg_parse_objects(sd_bus_message* m, const char* match_path,
					  enum msg_act act, void* user)
{
	/* enter array of objects */
	int r = sd_bus_message_enter_container(m, 'a', "{oa{sa{sv}}}");
	if (r < 0) {
		LOG_ERR("BLZ error parse obj 1");
		return r;
	}

	/* enter next dict/object */
	while ((r = sd_bus_message_enter_container(m, 'e', "oa{sa{sv}}")) > 0) {
		r = msg_parse_object(m, match_path, act, user);
		if (r < 0 || r == RETURN_FOUND) {
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

int msg_parse_notify(sd_bus_message* m, blz_char* ch, const void** ptr,
					 size_t* len)
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
		/* note: bool in sd-dbus is expected to be int type */
		int b;
		r = msg_read_variant(m, "b", &b);
		if (r < 0) {
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

int msg_append_property(sd_bus_message* m, const char* name, char type,
						const void* value)
{
	/* open dict */
	int r = sd_bus_message_open_container(m, 'e', "sv");
	if (r < 0) {
		LOG_ERR("BLZ failed to create property");
		return r;
	}

	r = sd_bus_message_append_basic(m, 's', name);
	if (r < 0) {
		LOG_ERR("BLZ failed to create property");
		return r;
	}

	/* open variant */
	r = sd_bus_message_open_container(m, 'v', "s");
	if (r < 0) {
		LOG_ERR("BLZ failed to create property");
		return r;
	}

	r = sd_bus_message_append_basic(m, type, value);
	if (r < 0) {
		LOG_ERR("BLZ failed to create property");
		return r;
	}

	/* close variant */
	r = sd_bus_message_close_container(m);
	if (r < 0) {
		LOG_ERR("BLZ failed to create property");
		return r;
	}

	/* close dict */
	r = sd_bus_message_close_container(m);
	if (r < 0) {
		LOG_ERR("BLZ failed to create property");
		return r;
	}

	return r;
}

/** type can only be basic type, but as string */
int msg_read_variant(sd_bus_message* m, char* type, void* dest)
{
	int r = sd_bus_message_enter_container(m, 'v', type);
	if (r < 0) {
		LOG_ERR("BLZ error parse variant 1");
		return r;
	}

	r = sd_bus_message_read_basic(m, type[0], dest);
	if (r < 0) {
		LOG_ERR("BLZ error parse variant 2");
		return r;
	}

	r = sd_bus_message_exit_container(m);
	if (r < 0) {
		LOG_ERR("BLZ error parse variant 3");
		return r;
	}

	return r;
}

/** type can only be basic type, but as string */
int msg_read_variant_strv(sd_bus_message* m, char*** dest)
{
	int r = sd_bus_message_enter_container(m, 'v', "as");
	if (r < 0) {
		LOG_ERR("BLZ error parse strv variant 1");
		return r;
	}

	r = sd_bus_message_read_strv(m, dest);
	if (r < 0) {
		LOG_ERR("BLZ error parse strv variant 2");
		return r;
	}

	r = sd_bus_message_exit_container(m);
	if (r < 0) {
		LOG_ERR("BLZ error parse strv variant 3");
		return r;
	}

	return r;
}
