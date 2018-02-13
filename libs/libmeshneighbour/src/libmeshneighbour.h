#ifndef _LIBMESHNEIGHBOUR_H_
#define _LIBMESHNEIGHBOUR_H_

#include <libgluonutil.h>
#include <libubox/list.h>

struct mesh_neighbour {
	unsigned char hwaddr[6];
	struct gluonutil_interface *iface;

	struct list_head list;
};

struct mesh_neighbour_ctx {
	struct list_head neighbours;
	struct list_head interfaces;
};

int mesh_get_neighbours(struct mesh_neighbour_ctx *neigh_ctx);
int mesh_get_neighbours_interfaces(struct list_head *interfaces, struct list_head* neighbours);
int mesh_get_neighbours_ubus(struct ubus_context *ubus_ctx, struct mesh_neighbour_ctx *neigh_ctx);

void mesh_free_neighbours(struct list_head *neighbours);
void mesh_free_neighbours_ctx(struct mesh_neighbour_ctx *ctx);

#endif
