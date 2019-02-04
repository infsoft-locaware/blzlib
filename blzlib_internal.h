#ifndef BLZLIB_INTERNAL_H
#define BLZLIB_INTERNAL_H

#define DBUS_PATH_MAX_LEN	255
#define UUID_STR_LEN		37
#define MAC_STR_LEN		18
#define NAME_STR_LEN		20

struct blz_context {
	sd_bus*			bus;
	char			path[DBUS_PATH_MAX_LEN];
	blz_scan_handler_t	scan_cb;
	sd_bus_slot*		scan_slot;
};

struct blz_dev {
	struct blz_context*	ctx;
	char			path[DBUS_PATH_MAX_LEN];
	char			mac[MAC_STR_LEN];
	char			name[NAME_STR_LEN];
	sd_bus_slot*		connect_slot;
	bool			connected;
	char**			service_uuids;
	char**			char_uuids;
	size_t			chars_idx;
};

/* Characteristic Flags (Characteristic Properties bit field) */
#define BLZ_CHAR_BROADCAST		0x01
#define BLZ_CHAR_READ			0x02
#define BLZ_CHAR_WRITE_WITHOUT_RESPONSE	0x04
#define BLZ_CHAR_WRITE			0x08
#define BLZ_CHAR_NOTIFY			0x10
#define BLZ_CHAR_INDICATE		0x20

struct blz_char {
	struct blz_context*	ctx;
	struct blz_dev*		dev;
	char			path[DBUS_PATH_MAX_LEN];
	char			uuid[UUID_STR_LEN];
	uint32_t		flags;
	blz_notify_handler_t	notify_cb;
	sd_bus_slot*		notify_slot;
};

enum e_obj { OBJ_CHAR, OBJ_DEVICE, OBJ_DEVICE_SCAN, OBJ_CHAR_COUNT, OBJ_CHARS_ALL };

int parse_msg_objects(sd_bus_message* m, const char* match_path, enum e_obj eobj, void* user);
int parse_msg_one_object(sd_bus_message* m, const char* match_path, enum e_obj eobj, void* user);
int parse_msg_device_properties(sd_bus_message* m, const char* opath, blz_dev* dev);
int parse_msg_notify(sd_bus_message* m, const void** ptr, size_t* len);
int parse_msg_one_interface(sd_bus_message* m, enum e_obj eobj, const char* opath, void* user);

#endif
