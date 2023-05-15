#include <stdio.h>
#include <stdlib.h>

#include <rdma/fabric.h>
#include <rdma/fi_errno.h>
#include <rdma/fi_rma.h>
#include <rdma/fi_domain.h>

#include "common.h"

void freehints(struct fi_info *hints)
{
	if (!hints)
		return;

	if (hints->domain_attr->name) {
		free(hints->domain_attr->name);
		hints->domain_attr->name = NULL;
	}
	if (hints->fabric_attr->name) {
		free(hints->fabric_attr->name);
		hints->fabric_attr->name = NULL;
	}
	if (hints->fabric_attr->prov_name) {
		free(hints->fabric_attr->prov_name);
		hints->fabric_attr->prov_name = NULL;
	}
	if (hints->src_addr) {
		free(hints->src_addr);
		hints->src_addr = NULL;
		hints->src_addrlen = 0;
	}
	if (hints->dest_addr) {
		free(hints->dest_addr);
		hints->dest_addr = NULL;
		hints->dest_addrlen = 0;
	}

	fi_freeinfo(hints);
}


/* Code acknowledgment: rping.c from librdmacm/examples */
int get_addr(char *dst, struct sockaddr *addr)
{
	struct addrinfo *res;
	int ret = -1;
	ret = getaddrinfo(dst, NULL, NULL, &res);
	if (ret) {
		error("getaddrinfo failed - invalid hostname or IP address\n");
		return ret;
	}
	memcpy(addr, res->ai_addr, sizeof(struct sockaddr_in));
	freeaddrinfo(res);
	return ret;
}