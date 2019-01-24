#ifndef BLZLIB_INTERNAL_H
#define BLZLIB_INTERNAL_H

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

int parse_msg_objects(sd_bus_message* m, blz_char* ch);

#endif
