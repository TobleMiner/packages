#include <stdbool.h>

#include <libubox/list.h>

typedef struct {
	char *if_name;
	bool up;
	char *proto;
} gluon_interface;
