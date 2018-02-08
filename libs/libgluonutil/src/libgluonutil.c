#include <stdlib.h>
#include <stdio.h>

#include <libubus.h>
#include <libubox/blobmsg.h>
#include <libubox/list.h>

#define GLUON_UBUS_INTERFACES "network.interfaces"
#define GLUON_UBUS_DUMP "dump"
#define GLUON_UBUS_TIMEOUT 3000

typedef struct {
	struct blob_attr* attr;
} attr_ctx;

static int gluon_parse_blob_interfaces(struct list_head *interfaces, struct blob_attr *attr, int len) {
	struct blob_attr *cursor;
	int remain = len;
	__blob_for_each_attr(cursor, attr, remain) {
		if(!blobmsg_check_attr(cursor, false)) {
			printf("Skipping invalid attribute\n");
			continue;
		}
		printf("Got attribute '%s'\n", blobmsg_name(attr));
	}
	return 0;
}

static int gluon_parse_blob_interface() {
	return 0;
}

static void gluon_get_mesh_interfaces_cb(struct ubus_request *req, int type, struct blob_attr *msg) {
	attr_ctx *ctx = req->priv;
	ctx->attr = msg;
}

int gluon_get_mesh_interfaces(struct ubus_context* ubus_ctx, struct list_head *interfaces) {
	int id, err = 0;
	if((err = ubus_lookup_id(ubus_ctx, GLUON_UBUS_INTERFACES, &id))) {
		goto fail;
	}

	struct blob_buf buf;
	blob_buf_init(&buf, 0);
	attr_ctx result;
	if((err = ubus_invoke(ubus_ctx, id, GLUON_UBUS_DUMP, buf.head,
		gluon_get_mesh_interfaces_cb, &result, GLUON_UBUS_TIMEOUT))) {
		goto fail;
	}

	if(!ctx->attr) {
		err = -EINVAL;
		goto fail;
	}

	gluon_parse_blob_interfaces(interfaces, blob_data(ctx->attr), blob_len(ctx->attr));

fail:
	return err;
}
