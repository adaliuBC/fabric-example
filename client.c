#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include <rdma/fabric.h>
#include <rdma/fi_errno.h>
#include <rdma/fi_rma.h>
#include <rdma/fi_domain.h>

#include "common.h"

const char *src_uid, *dst_uid;

int main(int argc, char ** argv) {
    int ret = -1, option = 0;

    struct fi_info *hints, *fi;
    hints = fi_allocinfo();
    if (!hints) {
        error("Failed to alloc info for hint\n");
        return EXIT_FAILURE;
    }

    // deal with the options
    while ((option = getopt(argc, argv, "s:d:p:P:w:")) != -1) {
        switch (option) {
            // set the src UID
            case 's':
                src_uid = calloc(strlen(optarg) + 1, 1);
                strncpy(src_uid, optarg, strlen(optarg));
                break;
            // set the dst UID
            case 'd':
                dst_uid = calloc(strlen(optarg) + 1, 1);
                strncpy(dst_uid, optarg, strlen(optarg));
                break;
            // set the provider
            case 'p':
                if (!hints->fabric_attr) {
                    hints->fabric_attr = malloc(sizeof *(hints->fabric_attr));
                    if (!hints->fabric_attr) {
                        perror("malloc");
                        exit(EXIT_FAILURE);
                    }
                }
                hints->fabric_attr->prov_name = strdup(optarg);
                break;
            case 'P':

                break; // ??
            case 'w':
                // window_size = str2size(optarg);
                // if (!window_size) {
                //     window_size = DEFAULT_WINDOW_SIZE;
                // }
                break;
            // wrong usage
            default:
            	printf("Usage:\n");
                printf("rdma_server: [-a <server_addr>] [-p <server_port>]\n");
                printf("(default port is %d)\n", DEFAULT_RDMA_PORT);
                exit(1);
                break;
        }
    }

    // set the hints
    // hints->ep_attr->type = FI_EP_RDM;
    ret = fi_getinfo(FI_VERSION(1,18), NULL, NULL, 0, hints, &fi);
    if (ret == -FI_ENODATA) {
        error("Could not find any optimal provider");
        return -FI_ENODATA;
    } else if (ret) {
        error("Failed to get info with errno: %d\n", -ret);
        return -ret;
    }
    info("Found info:\n");
    debug("%s\n", fi_tostr(fi, FI_TYPE_INFO));

    free(src_uid);
    free(dst_uid);
    if (fi) {
        fi_freeinfo(fi);
        fi = NULL;
    }
    if (hints) {
        freehints(hints);
        hints = NULL;
    }
    
    return ret;
}