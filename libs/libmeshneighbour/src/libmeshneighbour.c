/*
   Copyright (c) 2018, Tobias Schramm <tobleminer@gmail.com>
   
   All rights reserved.
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:
   1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
   SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
   CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
   OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
   */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <stdbool.h>

#include <libmnl/libmnl.h>
#include <linux/if_link.h>
#include <linux/rtnetlink.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <libgluonutil.h>
#include <libubox/list.h>
#include <libubus.h>

#include "libmeshneighbour.h"

#ifndef typeof
#define typeof(_type) __typeof__(_type)
#endif

#define IPV6_MCAST_ALL_NODES (struct in6_addr){ .s6_addr = { 0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 } }


#ifndef container_of
#define container_of(ptr, type, member)					\
	({								\
		const typeof(((type *) NULL)->member) *__mptr = (ptr);	\
		(type *) ((char *) __mptr - offsetof(type, member));	\
	})
#endif

/*
void mesh_free_neighbours(struct list_head *neighbours) {
	struct mesh_neighbour *neigh, *next;
	list_for_each_entry_safe(neigh, next, neighbours, list) {
		list_del(&neigh->list);
		free(neigh);
	}
}

void mesh_free_neighbours_ctx(struct mesh_neighbour_ctx *ctx) {
	mesh_free_neighbours(&ctx->neighbours);
	gluonutil_free_interfaces(&ctx->interfaces);
}

struct mesh_neigh_ptr_ctx {
	struct list_head *interfaces;
	struct list_head *neighbours;
};

static struct gluonutil_interface *find_interface_ifindex(struct list_head* interfaces, unsigned int ifindex) {
	struct gluonutil_interface *iface;
	list_for_each_entry(iface, interfaces, list) {
		if(iface->ifindex == ifindex) {
			return iface;
		}
	}
	return NULL;
}

struct mesh_interface_ctx {
	struct list_head *neighbours;
	struct gluonutil_interface *iface;
};

#define member_size(type, member) sizeof(((type *)0)->member)

static int neigh_attr_cb(const struct nlattr *attr, void *data) {
	int type = mnl_attr_get_type(attr);
	if(mnl_attr_type_valid(attr, NDA_MAX) < 0) {
		goto out;
	}

	if(type == NDA_DST) {
		struct mesh_interface_ctx *ctx = data;
		struct mesh_neighbour *neigh = malloc(sizeof(struct mesh_neighbour));
		if(!neigh) {
			goto out;
		}
		neigh->iface = ctx->iface;
		struct in6_addr *addr = mnl_attr_get_payload(attr);
		memcpy(&neigh->addr, addr, sizeof(*addr));
		list_add(&neigh->list, ctx->neighbours);
		char buf[INET6_ADDRSTRLEN];
		printf("Added neighbour %s ", inet_ntop(AF_INET6, addr, buf, sizeof(buf)));
	}

out:
	return MNL_CB_OK;
}

static int neigh_cb(const struct nlmsghdr *nlh, void *data)
{
	struct ndmsg* ndmsg = mnl_nlmsg_get_payload(nlh);
	switch(ndmsg->ndm_state) {
		case NUD_INCOMPLETE:
		case NUD_FAILED:
			return MNL_CB_OK;
	}

	struct mesh_neigh_ptr_ctx *ctx = data;
	struct gluonutil_interface *iface = find_interface_ifindex(ctx->interfaces, ndmsg->ndm_ifindex);
	if(!iface) {
		return MNL_CB_OK;
	}

	printf("Interface %d, state %x\n", ndmsg->ndm_ifindex, ndmsg->ndm_state);

	struct mesh_interface_ctx iface_ctx;
	iface_ctx.neighbours = ctx->neighbours;
	iface_ctx.iface = iface;
	mnl_attr_parse(nlh, sizeof(*ndmsg), neigh_attr_cb, &iface_ctx);
	return MNL_CB_OK;
}

int mesh_get_neighbours_interfaces(struct list_head *interfaces, struct list_head* neighbours) {
	int err;
	struct mnl_socket *nl;
	char buf[MNL_SOCKET_BUFFER_SIZE];
	struct nlmsghdr *nlh;
	struct ndmsg *ndm;
	unsigned int seq, portid;

	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type = RTM_GETNEIGH;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	nlh->nlmsg_seq = seq = time(NULL);
	ndm = mnl_nlmsg_put_extra_header(nlh, sizeof(struct ndmsg));

	ndm->ndm_family = AF_INET6;

	nl = mnl_socket_open(NETLINK_ROUTE);
	if(!nl) {
		err = -errno;
		goto fail;
	}

	if(mnl_socket_bind(nl, 0, MNL_SOCKET_AUTOPID) < 0) {
		err = -errno;
		goto fail_nl;
	}
	portid = mnl_socket_get_portid(nl);

	if(mnl_socket_sendto(nl, nlh, nlh->nlmsg_len) < 0) {
		err = -errno;
		goto fail_nl;
	}

	struct mesh_neigh_ptr_ctx ctx;
	ctx.interfaces = interfaces;
	ctx.neighbours = neighbours;

	ssize_t len = 0;
	int ret = 0;
	while((len = mnl_socket_recvfrom(nl, buf, sizeof(buf))) > 0) {
		ret = mnl_cb_run(buf, len, seq, portid, neigh_cb, &ctx);
		if(ret != MNL_CB_OK) {
			break;
		}
	}

	// TODO filter local mac addresses

	return 0;

fail_nl:
	mnl_socket_close(nl);
fail:
	return err;
}

int mesh_get_neighbours_ubus(struct ubus_context *ubus_ctx, struct mesh_neighbour_ctx *neigh_ctx) {
	int err = get_neighbours_common(ubus_ctx, neigh_ctx);
	if(err) {
		goto fail;
	}

	err = mesh_get_neighbours_interfaces(&neigh_ctx->interfaces, &neigh_ctx->neighbours);
	if(err) {
		goto fail_interfaces;
	}

	return 0;

fail_interfaces:
	gluonutil_free_interfaces(&neigh_ctx->interfaces);
fail:
	return err;
}

int mesh_get_neighbours(struct mesh_neighbour_ctx *neigh_ctx) {
	struct ubus_context *ubus_ctx = ubus_connect(NULL);
	if(!ubus_ctx) {
		return -ECONNREFUSED;
	}

	int err = mesh_get_neighbours_ubus(ubus_ctx, neigh_ctx);

	ubus_free(ubus_ctx);

	return err;
}
*/

static int get_neighbours_common(struct ubus_context *ubus_ctx, struct mesh_neighbour_ctx *neigh_ctx) {
	neigh_ctx->neighbours = (struct list_head)LIST_HEAD_INIT(neigh_ctx->neighbours);
	neigh_ctx->interfaces = (struct list_head)LIST_HEAD_INIT(neigh_ctx->interfaces);

	return gluonutil_get_mesh_interfaces(ubus_ctx, &neigh_ctx->interfaces);
}

void mesh_free_respondd_neighbours(struct list_head *neighbours) {
	struct mesh_neighbour *neigh, *next;
	list_for_each_entry_safe(neigh, next, neighbours, list) {
		list_del(&neigh->list);
		free(neigh);
	}
}

void mesh_free_respondd_neighbours_ctx(struct mesh_neighbour_ctx *ctx) {
	mesh_free_respondd_neighbours(&ctx->neighbours);
	gluonutil_free_interfaces(&ctx->interfaces);
}

struct mesh_respondd_ctx {
	struct gluonutil_interface *iface;
	struct list_head *neighbours;
	void *cb_priv;
	neighbour_cb cb;
};

static int mesh_respondd_cb(char* json_data, size_t data_len, struct librespondd_pkt_info *pktinfo, void* priv) {
	// pktinfo not set, something is not right
	if(!pktinfo->ifindex) {
		goto out;
	}

	struct mesh_respondd_ctx *ctx = priv;

	struct mesh_neighbour *neighbour = malloc(sizeof(struct mesh_neighbour));
	if(!neighbour) {
		goto out;
	}
	memset(neighbour, 0, sizeof(*neighbour));

	neighbour->iface = ctx->iface;
	neighbour->addr = pktinfo->src_addr;

	if(ctx->cb) {
		if(ctx->cb(json_data, data_len, pktinfo, neighbour, ctx->cb_priv)) {
			goto out_neighbour;
		}
	}

	list_add(&neighbour->list, ctx->neighbours);

	return RESPONDD_CB_OK;

out_neighbour:
	free(neighbour);
out:
	return RESPONDD_CB_OK;
}

int mesh_get_neighbours_respondd_interfaces(struct list_head *interfaces, struct list_head* neighbours, unsigned short respondd_port, neighbour_cb cb, void *priv) {
	struct gluonutil_interface *iface;
	struct sockaddr_in6 sock_addr;
	sock_addr.sin6_family = AF_INET6;
	sock_addr.sin6_port = respondd_port;
	sock_addr.sin6_flowinfo = 0;
	sock_addr.sin6_addr = IPV6_MCAST_ALL_NODES;

	struct timeval timeout = { 3, 0 };

	int err = 0;
	list_for_each_entry(iface, interfaces, list) {
		if(!iface->up) {
			continue;
		}

		sock_addr.sin6_scope_id = iface->ifindex;

		struct mesh_respondd_ctx ctx = {
			.iface = iface,
			.neighbours = neighbours,
			.cb_priv = priv,
			.cb = cb,
		};
		int ret = respondd_request(&sock_addr, "nodeinfo", &timeout, mesh_respondd_cb, &ctx);
		if(ret) {
			err = ret;
		}
	}

	return err;
}

int mesh_get_neighbours_respondd_ubus(struct ubus_context *ubus_ctx, struct mesh_neighbour_ctx *neigh_ctx, unsigned short respondd_port, neighbour_cb cb, void *priv) {
	int err = get_neighbours_common(ubus_ctx, neigh_ctx);
	if(err) {
		goto fail;
	}

	err = mesh_get_neighbours_respondd_interfaces(&neigh_ctx->interfaces, &neigh_ctx->neighbours, respondd_port, cb, priv);
	if(err) {
		goto fail_interfaces;
	}

	return 0;

fail_interfaces:
	gluonutil_free_interfaces(&neigh_ctx->interfaces);
fail:
	return err;
}

int mesh_get_neighbours_respondd(struct mesh_neighbour_ctx *neigh_ctx, unsigned short respondd_port, neighbour_cb cb, void *priv) {
	struct ubus_context *ubus_ctx = ubus_connect(NULL);
	if(!ubus_ctx) {
		return -ECONNREFUSED;
	}

	int err = mesh_get_neighbours_respondd_ubus(ubus_ctx, neigh_ctx, respondd_port, cb, priv);

	ubus_free(ubus_ctx);

	return err;
}