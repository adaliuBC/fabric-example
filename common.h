#ifndef _COMMON_H
#define _COMMON_H

#include <stdio.h>
#include <stdlib.h>

#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <rdma/fabric.h>
#include <rdma/fi_errno.h>
#include <rdma/fi_rma.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_cm.h>
#include <rdma/fi_eq.h>

#define DEFAULT_RDMA_PORT (20886)
#define TIMEOUT (20000) /* in ms */
#define BUFFER_SIZE (1024)

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


void freehints(struct fi_info *hints);
int get_addr(char *dst, struct sockaddr *addr);

#endif