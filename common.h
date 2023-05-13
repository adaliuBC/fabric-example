#ifndef _COMMON_H
#define _COMMON_H

#include <stdio.h>
#include <stdlib.h>

#include <rdma/fabric.h>
#include <rdma/fi_errno.h>
#include <rdma/fi_rma.h>
#include <rdma/fi_domain.h>

/* error Macro*/
#define error(msg, args...) do {\
	fprintf(stderr, "\033[1;31;40m%s : %d : error : "msg"\033[0m", __FILE__, __LINE__, ## args);\
}while(0);

#define debug(msg, args...) do {\
    printf("\033[1;37;40mDEBUG: "msg"\033[0m", ## args);\
}while(0);

#define info(msg, args...) do {\
    printf("\033[1;33;40mINFO: "msg"\033[0m", ## args);\
}while(0);


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

#endif