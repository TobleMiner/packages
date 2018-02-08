#pragma once

#include <stdbool.h>

#include <libubus.h>
#include <libubox/list.h>

typedef struct {
	char *if_name;
	bool up;
	char *proto;
} gluon_interface;

int gluon_get_mesh_interfaces(struct ubus_context* ubus_ctx, struct list_head *interfaces);
