#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <time.h>
#include <syslog.h>
#include <stdbool.h>

#include <libmnl/libmnl.h>
#include <linux/if.h>
#include <linux/if_link.h>
#include <linux/rtnetlink.h>

#include <json-c/json.h>
#include <libubox/list.h>
#include <libubus.h>
#include <libgluonutil.h>
#include <respondd.h>

#ifndef typeof
#define typeof(_type) __typeof__((_type))
#endif

struct nl_ctx {
	struct mnl_socket* nl;
	unsigned int nl_seq;
};

struct l2_iface {
	struct gluonutil_interface* iface;
	char mac_addr[18];
	bool mac_set;

	struct list_head list;
};

struct if_ctx {
	struct list_head *interfaces;
	struct list_head *l2_interfaces;
};

static struct gluonutil_interface *find_interface(struct list_head* interfaces, int ifindex) {
	struct gluonutil_interface *cursor;
	list_for_each_entry(cursor, interfaces, list) {
		if(cursor->ifindex == ifindex) {
			return cursor;
		}
	}
	return NULL;
}

static void free_l2_interfaces(struct list_head *interfaces) {
	struct l2_iface *cursor, *next;
	list_for_each_entry_safe(cursor, next, interfaces, list) {
		list_del(&cursor->list);
		free(cursor);
	}
}

static int link_attr_cb(const struct nlattr *attr, void *data) {
	struct l2_iface *l2_if = data;
	int type = mnl_attr_get_type(attr);

	if(type == IFLA_ADDRESS) {
		uint8_t *hwaddr = mnl_attr_get_payload(attr);
		assert(mnl_attr_get_payload_len(attr) == 6);
		sprintf(l2_if->mac_addr, "%02x:%02x:%02x:%02x:%02x:%02x", hwaddr[0],
			hwaddr[1], hwaddr[2], hwaddr[3], hwaddr[4], hwaddr[5]);
		l2_if->mac_set = true;
	}

	return MNL_CB_OK;
}

static int link_cb(const struct nlmsghdr *nlh, void *data) {
	struct ifinfomsg *ifm = mnl_nlmsg_get_payload(nlh);
	struct if_ctx *interfaces = data;
	struct gluonutil_interface *iface = find_interface(interfaces->interfaces, ifm->ifi_index);
	if(!iface) {
		goto done;
	}

	struct l2_iface *l2_if = malloc(sizeof(struct l2_iface));
	if(!l2_if) {
		syslog(LOG_WARNING, "Failed to allocate layer 2 interface structure\n");
		return -ENOMEM;
	}
	memset(l2_if, 0, sizeof(*l2_if));
	l2_if->iface = iface;

	mnl_attr_parse(nlh, sizeof(*ifm), link_attr_cb, l2_if);

	if(!l2_if->mac_set) {
		syslog(LOG_WARNING, "Failed to get mac address for interface '%s'\n", iface->device);
		free(l2_if);
		goto done;
	}

	list_add(&l2_if->list, interfaces->l2_interfaces);

done:
	return MNL_CB_OK;
}

static int nl_get_mac_addrs(struct nl_ctx *ctx, struct list_head* interfaces, struct list_head* l2_interfaces) {
	int err = 0;
	char buf[MNL_SOCKET_BUFFER_SIZE];
	struct nlmsghdr *nlh;
	struct rtgenmsg *rt;
	
        nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type = RTM_GETLINK;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	nlh->nlmsg_seq = ctx->nl_seq;
	rt = mnl_nlmsg_put_extra_header(nlh, sizeof(struct rtgenmsg));
	rt->rtgen_family = AF_PACKET;

	ssize_t len = 0;
	if((len = mnl_socket_sendto(ctx->nl, nlh, nlh->nlmsg_len)) < 0) {
		err = len;
		syslog(LOG_WARNING, "Failed to send message to rtnl: %d\n", err);
		goto fail;
	}

	struct if_ctx if_ctx;
	if_ctx.interfaces = interfaces;
	if_ctx.l2_interfaces = l2_interfaces;
	int ret = 0;
	while((len = mnl_socket_recvfrom(ctx->nl, buf, sizeof(buf))) > 0) {
		ret = mnl_cb_run(buf, len, ctx->nl_seq, mnl_socket_get_portid(ctx->nl), link_cb, &if_ctx);
		if(ret != MNL_CB_OK) {
			break;
		}
	}

	if(ret < 0) {
		err = -errno;
		syslog(LOG_WARNING, "Callback failed: %s (%d)\n", strerror(errno), err);
		goto fail;		
	}

	if(len < 0) {
		err = len;
		syslog(LOG_WARNING, "Recv from netlink failed: %s (%d)\n", strerror(-err), err);
		goto fail;		
	}

fail:
	return err;
}

static int rtnl_connect(struct nl_ctx *ctx) {
	int err;

	ctx->nl = mnl_socket_open(NETLINK_ROUTE);
	if(!ctx->nl) {
		err = -EINVAL;
		syslog(LOG_WARNING, "Failed to connect to netlink\n");
		goto fail;
	}

	if((err = mnl_socket_bind(ctx->nl, 0, MNL_SOCKET_AUTOPID)) < 0) {
		syslog(LOG_WARNING, "Failed to bind rtnl socket to pid\n");
		goto fail_socket;
	}

	ctx->nl_seq = time(NULL);
	return 0;
fail_socket:
	mnl_socket_close(ctx->nl);
fail:
	return err;
}

static struct json_object *respondd_provider_mesh() {
	struct nl_ctx nl_ctx;
	struct ubus_context *ubus_ctx;
	LIST_HEAD(interfaces);
	LIST_HEAD(l2_interfaces);

	if(rtnl_connect(&nl_ctx)) {
		syslog(LOG_WARNING, "Failed to connect to rt netlink\n");
		return NULL;
	}

	ubus_ctx = ubus_connect(NULL);
	if(!ubus_ctx) {
		syslog(LOG_WARNING, "Failed to connect to ubus\n");
		mnl_socket_close(nl_ctx.nl);
		return NULL;
	}
	ubus_add_uloop(ubus_ctx);

	if(gluonutil_get_mesh_interfaces(ubus_ctx, &interfaces)) {
		syslog(LOG_WARNING, "Failed to get mesh interfaces\n");
		ubus_free(ubus_ctx);
		mnl_socket_close(nl_ctx.nl);
		return NULL;
	}

	ubus_free(ubus_ctx);

	if(nl_get_mac_addrs(&nl_ctx, &interfaces, &l2_interfaces)) {
		syslog(LOG_WARNING, "Failed to get mac adresses\n");
		gluonutil_free_interfaces(&interfaces);
		mnl_socket_close(nl_ctx.nl);
		return NULL;
	}

	mnl_socket_close(nl_ctx.nl);

	struct json_object *json_result = json_object_new_object();
	if (!json_result) {
		syslog(LOG_WARNING, "Failed to create root json object\n");
		gluonutil_free_interfaces(&interfaces);
		return NULL;
	}

	struct json_object *json_network = json_object_new_object();
	if (!json_network) {
		syslog(LOG_WARNING, "Failed to create network json object\n");
		json_object_put(json_result);
		gluonutil_free_interfaces(&interfaces);
		return NULL;
	}

	struct json_object *json_mesh = json_object_new_object();
	if (!json_mesh) {
		syslog(LOG_WARNING, "Failed to create mesh json object\n");
		json_object_put(json_network);
		json_object_put(json_result);
		gluonutil_free_interfaces(&interfaces);
		return NULL;
	}

	struct json_object *json_link = json_object_new_object();
	if (!json_link) {
		syslog(LOG_WARNING, "Failed to create link json object\n");
		json_object_put(json_mesh);
		json_object_put(json_network);
		json_object_put(json_result);
		gluonutil_free_interfaces(&interfaces);
		return NULL;
	}

	struct l2_iface* cursor;
	list_for_each_entry(cursor, &l2_interfaces, list) {
		struct json_object *mac_addr = json_object_new_string(cursor->mac_addr);
		if(!mac_addr) {
			syslog(LOG_WARNING, "Failed to create json MAC string\n");
			continue;
		}
		json_object_object_add(json_link, cursor->iface->device, mac_addr);
	}

	json_object_object_add(json_mesh, "link_macs", json_link);
	json_object_object_add(json_network, "mesh", json_mesh);
	json_object_object_add(json_result, "network", json_network);

	free_l2_interfaces(&l2_interfaces);
	gluonutil_free_interfaces(&interfaces);
	
	return json_result;
}

const struct respondd_provider_info respondd_providers[] = {
	{"nodeinfo", respondd_provider_mesh},
	{0, 0},
};
