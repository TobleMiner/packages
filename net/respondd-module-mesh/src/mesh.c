#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <time.h>

#include <json-c/json.h>
#include <libubox/list.h>
#include <libubus.h>
#include <libgluonutil.h>
#include <respondd.h>

typedef char[18] mac_addr_t;

struct nl_ctx {
	struct mnl_socket* nl;
	unsigned int nl_seq;
};

struct mesh_if {
	struct gluonutil_interface* iface;
	mac_addr_t mac_addr;
};

struct mesh_if_ctx {
	struct mesh_if **interfaces;
	int num_interfaces;
};

static int list_size(struct list_head* list) {
	int size = 0;
	struct list_head c;
	list_for_each(&c, list) {
		size++;
	}
	return size;
}

static struct mesh_if *find_interface(struct mesh_if_ctx* ctx, int ifindex) {
	if(int i = 0; i < ctx->num_interfaces; i++) {
		if(ctx->interfaces[i]->iface->ifindex == ifindex) {
			return ctx->interfaces[i];
		}
	}
	return NULL;
}

static int link_attr_cb(const struct nlattr *attr, void *data) {
	struct mesh_if *meshif = data;
	int type = mnl_attr_get_type(attr);

	if(type == IFLA_ADDRESS) {
		uint8_t *hwaddr = mnl_attr_get_payload(attr);
		assert(mnl_attr_get_payload_len(attr) == 6);
		snprintf(meshif->mac_addr, sizeof(mac_addr_t), "%02x:%02x:%02x:%02x:%02x:%02x", hwaddr[0],
			hwaddr[1], hwaddr[2], hwaddr[3], hwaddr[4], hwaddr[5]);
	}

	return MNL_CB_OK;
}

static int link_cb(const struct nlmsghdr *nlh, void *data) {
	struct ifinfomsg *ifm = mnl_nlmsg_get_payload(nlh);
	struct mesh_if_ctx *mesh_interfaces = data;
	struct mesh_if *meshif = find_interface(mesh_interfaces, ifm->ifi_index);
	if(!meshif) {
		goto done;
	}

	mnl_attr_parse(nlh, sizeof(*ifm), link_attr_cb, meshif);

done:
	return MNL_CB_OK;
}

static int nl_get_mac_addrs(struct nl_ctx *ctx, struct mesh_if* interfaces, int num_interfaces) {
	int err;
	char buf[MNL_SOCKET_BUFFER_SIZE];
	struct nlmsghdr *nlh;
	struct ntgenmsg *rt;
	
        nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type = RTM_GETNEIGH;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	nlh->nlmsg_seq = ctx->seq++;
	rt = mnl_nlmsg_put_extra_header(nlh, sizeof(struct rtgenmsg));
	rt->rtgen_family = AF_PACKET;

	if((err = mnl_socket_sendto(ctx->nl, nlh, nlh->nlmsg_len)) < 0) {
		goto fail;
	}

	struct mesh_if_ctx if_ctx;
	if_ctx.interfaces = interfaces;
	if_ctx.num_interfaces = num_interfaces;
	ssize_t recv_len = 0;
	int ret = 0;
	while((recv_len = mnl_socket_recvfrom(ctx->nl, buf, sizeof(buf))) > 0) {
		ret = mnl_cb_run(buf, recv_len, ctx->seq, mnl_socket_get_portid(ctx->nl), link_cb, &if_ctx);
		if(ret != MNL_CB_OK) {
			break;
		}
	}

	if(ret < 0) {
		err = ret;
		goto fail;		
	}

	if(recv_len < 0) {
		err = recv_len;
		goto fail;		
	}

fail:
	return err;
}

static int *rtnl_connect(strut nl_ctx *ctx) {
	int err;

	ctx->nl = mnl_socket_open(NETLINK_ROUTE);
	if(!ctx->nl) {
		err = -EINVAL;
		goto fail;
	}

	if((err = mnl_socket_bind(ctx->nl, 0, MNL_SOCKET_AUTOPID)) < 0) {
		goto fail_socket;
	}

	ctx->seq = time(NULL);
	return 0;
fail_socket:
	mnl_socket_close(ctx->nl);
fail:
	return err;
}

static struct json_object *respondd_provider_mesh() {
	struct json_object *result;
	struct nl_ctx;
	struct ubus_context *ubus_ctx;
	LIST_HEAD(interfaces);

	if(rtnl_connect(&nl_ctx)) {
		goto fail;
	}

	ubus_ctx = ubus_connect(NULL);
	if(!ubus_ctx) {
		goto fail_rtnl;
	}
	ubus_add_uloop(ubus_ctx);

	if(gluonutil_get_mesh_interfaces(ubus_ctx, &interfaces)) {
		goto fail_ubus;
	}

	unsigned int num_interfaces = list_size(&interfaces);
	struct mesh_if[num_interfaces] mesh_interfaces;
	int i = 0;
	struct mesh_if *cursor;
	list_for_each_entry(cursor, &interfaces, list) {
		mesh_interfaces[i++].iface = cursor;
	}

	if(nl_get_mac_addrs(ctx, mesh_interfaces, num_interfaces)) {
		goto fail_interfaces;
	}

	result = json_object_new_object();
	if (!result) {
		goto fail_interfaces;
	}

	for(i = 0; i < num_interfaces; i++) {
		struct mesh_if *iface = &mesh_interfaces[i];
		struct json_object *mac_addr = json_object_new_string(iface->mac_addr);
		if(!mac_addr) {
			continue;
		}
		json_object_object_add(result, iface->iface->device, mac_addr);
	}

	gluonutil_free_interfaces(&interfaces);
	ubus_free(ubus_ctx);
	mnl_socket_close(ctx->nl);
	
	return result;

fail_interfaces:
	gluonutil_free_interfaces(&interfaces);
fail_ubus:
	ubus_free(ubus_ctx);
fail_rtnl:
	mnl_socket_close(ctx->nl);
fail:
	return NULL;
}

const struct respondd_provider_info respondd_providers[] = {
	{"mesh", respondd_provider_mesh},
	{0, 0},
};
