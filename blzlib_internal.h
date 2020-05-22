/*
 * blzlib - Copyright (C) 2019 Bruno Randolf (br1@einfach.org)
 *
 * This source code is licensed under the GNU Lesser General Public License,
 * Version 3. See the file COPYING for more details.
 */

#ifndef BLZLIB_INTERNAL_H
#define BLZLIB_INTERNAL_H

#define DBUS_PATH_MAX_LEN	255
#define UUID_STR_LEN		37
#define MAC_STR_LEN			18
#define NAME_STR_LEN		20
#define CONNECT_TIMEOUT		60 /* sec */
#define SERV_RESOLV_TIMEOUT 60 /* sec */

/* this return value is used to indicate that we found what was searched */
#define RETURN_FOUND 1000

/* clang-format off */
struct blz_context {
	sd_bus*			   bus;
	char			   path[DBUS_PATH_MAX_LEN];
	blz_scan_handler_t scan_cb;
	sd_bus_slot*	   scan_slot;
	void*              scan_user;
};

struct blz_dev {
	struct blz_context*	  ctx;
	char				  path[DBUS_PATH_MAX_LEN];
	uint8_t				  mac[6];
	char				  name[NAME_STR_LEN];
	sd_bus_slot*		  connect_slot;
	bool				  connect_async_done;
	int					  connect_async_result;
	bool				  connected;
	bool				  services_resolved;
	int16_t				  rssi;
	char**				  service_uuids;
	blz_disconn_handler_t disconnect_cb;
	void*                 disconn_user;
};

struct blz_serv {
	struct blz_context* ctx;
	struct blz_dev*		dev;
	char				path[DBUS_PATH_MAX_LEN];
	char				uuid[UUID_STR_LEN];
	char**				char_uuids;
	size_t				chars_idx;
};

/* Characteristic Flags (Characteristic Properties bit field) */
#define BLZ_CHAR_BROADCAST				0x01
#define BLZ_CHAR_READ					0x02
#define BLZ_CHAR_WRITE_WITHOUT_RESPONSE 0x04
#define BLZ_CHAR_WRITE					0x08
#define BLZ_CHAR_NOTIFY					0x10
#define BLZ_CHAR_INDICATE				0x20
#define BLZ_CHAR_SIGNED_WRITE			0x40
#define BLZ_CHAR_EXTENDED				0x80

struct blz_char {
	struct blz_context*	 ctx;
	struct blz_dev*		 dev;
	char				 path[DBUS_PATH_MAX_LEN];
	char				 uuid[UUID_STR_LEN];
	uint32_t			 flags;
	blz_notify_handler_t notify_cb;
	sd_bus_slot*		 notify_slot;
	bool				 notifying;
	void*                notify_user;
};
/* clang-format on */

/* actions that can be done on message parsing for objects and interfaces */
enum msg_act {
	MSG_CHAR_FIND,
	MSG_DEVICE,
	MSG_DEVICE_SCAN,
	MSG_CHAR_COUNT,
	MSG_CHARS_ALL,
	MSG_SERV_FIND
};

int msg_parse_objects(sd_bus_message* m, const char* match_path,
					  enum msg_act act, void* user);
int msg_parse_object(sd_bus_message* m, const char* match_path,
					 enum msg_act act, void* user);
int msg_parse_interface(sd_bus_message* m, enum msg_act act, const char* opath,
						void* user);
int msg_parse_notify(sd_bus_message* m, blz_char* ch, const void** ptr,
					 size_t* len);
int msg_append_property(sd_bus_message* m, const char* name, char type,
						const void* value);
int msg_read_variant(sd_bus_message* m, char* type, void* dest);
int msg_read_variant_strv(sd_bus_message* m, char*** dest);

#endif
