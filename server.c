#include <getopt.h>

#include "common.h"

char *src_uid, *dst_uid;
struct fi_info *fi, *fi_pep;
struct fid_fabric *fabric;
struct fid_domain *domain;
struct fid_pep *pep;
struct fid_ep *ep;
struct fid_eq *eq;
struct fid_cq *send_cq, *recv_cq;
struct fid_mr *mr;
int post_depth = 16;

int main(int argc, char ** argv) {
    int ret = -1, option = 0;

    struct fi_info *hints;
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
            	// printf("Usage:\n");
                // printf("rdma_server: [-a <server_addr>] [-p <server_port>]\n");
                exit(1);
        }
    }

    // set the hints
    // hints->ep_attr->type = FI_EP_RDM;
    // getinfo with given provider
    ret = fi_getinfo(FI_VERSION(1,17), NULL, NULL, 0, hints, &fi_pep);
    if (ret == -FI_ENODATA) {
        error("Could not find any optimal provider\n");
        return -FI_ENODATA;
    } else if (ret) {
        error("Failed to get info with errno: %d (%s)\n", -ret, fi_strerror(-ret));
        return -ret;
    }
    info("Found info:\n");
    debug("%s\n", fi_tostr(fi_pep, FI_TYPE_INFO));

    ret = fi_fabric(fi_pep->fabric_attr, &fabric, NULL);
    if (ret) {
        error("Failed to open fabric with errno: %d (%s)\n", -ret, fi_strerror(-ret));
        return -ret;
    }
    debug("Fabric opened\n");

    // ret = fi_domain(fabric, fi, &domain, NULL);
    // if (ret) {
    //     error("Failed to open domain with errno: %d (%s)\n", -ret, fi_strerror(-ret));
    //     return -ret;
    // }
    // debug("Domain opened\n");

    struct fi_eq_attr eq_attr = {
        .wait_obj = FI_WAIT_UNSPEC
    };
    ret = fi_eq_open(fabric, &eq_attr, &eq, NULL);
    if (ret) {
        error("Failed to open event queue with errno: %d (%s)\n", -ret, fi_strerror(-ret));
        return -ret;
    }
    debug("Event queue opened\n");

    // create endpoint
    // debug("%s\n", fi_tostr(fi->fabric_attr, FI_TYPE_FABRIC_ATTR));
    ret = fi_passive_ep(fabric, fi_pep, &pep, NULL);
    if (ret) {
        error("Create passive endpoint on server failed with errno: %d\n", ret);
        return ret;
    }
    debug("Passive endpoint created\n");

    // bind the endpoint to the event queue
    ret = fi_pep_bind(pep, &eq->fid, 0);
    if (ret) {
        error("Bind endpoint to event queue failed with errno %d\n", -ret);
        return -ret;
    }
    debug("Endpoint binded to event queue\n");

    // // listen???
    // ret = fi_listen(pep);
    // if (ret) {
    //     error("Listen failed with errno %d\n", -ret);
    //     return -ret;
    // }
    // debug("Listen\n");

    // // waiting for connection
    // uint32_t event_id = 0;
    // struct fi_eq_cm_entry event_entry;
    // ret = fi_eq_sread(eq, &event_id, (void *)&event_entry, sizeof(event_entry), 
    //     20000, 0);
    // if (ret < 0) {
    //     error("Event sread failed with errno %d\n", -ret);
    //     return -ret;
    // }
    // if (event_id != FI_CONNREQ) {
    //     error("Unexpected event id %d\n", event_id);
    //     return -FI_EOTHER;
    // }
    // struct fi_info *prov = event_entry.info;
    // debug("Receive connection request with info:\n");
    // debug("%s\n", fi_tostr(prov, FI_TYPE_INFO));

    // // create new domain, endpoint with the given info
    // // 是在原来domain的基础上update？还是新建一个domain？
    // ret = fi_domain(fabric, prov, &domain, NULL);
    // if (ret) {
    //     error("Failed to open domain again with errno: %d (%s)\n", -ret, fi_strerror(-ret));
    //     // TODO: reject the connection
    //     return -ret;
    // }
    // prov->ep_attr->tx_ctx_cnt = (size_t)post_depth;
    // prov->ep_attr->rx_ctx_cnt = (size_t)post_depth;
    // prov->tx_attr->iov_limit = 1;  // ScatterGatter max depth
    // prov->rx_attr->iov_limit = 1; 
    // prov->tx_attr->inject_size = 0;  // no INLINE support
    // ret = fi_endpoint(domain, prov, &ep, NULL);
    // if (ret) {
    //     error("Failed to open endpoint again with errno: %d (%s)\n", -ret, fi_strerror(-ret));
    //     // TODO: reject the connection
    //     return -ret;
    // }

    // // create new comp queue for send and recv
    // struct fi_cq_attr cq_attr;
    // cq_attr.format = FI_CQ_FORMAT_MSG;
    // cq_attr.wait_obj = FI_WAIT_NONE;
    // cq_attr.wait_cond = FI_CQ_COND_NONE;
    // cq_attr.flags = FI_SEND;
    // ret = fi_cq_open(domain, &cq_attr, &send_cq, NULL);
    // if (ret) {
    //     error("Create send completion queue failed with errno: %d (%s)\n", -ret, fi_strerror(-ret));
    //     return -ret;
    // }

    // cq_attr.flags = FI_RECV;
    // ret = fi_cq_open(domain, &cq_attr, &recv_cq, NULL);
    // if (ret) {
    //     error("Create recv completion queue failed with errno: %d (%s)\n", -ret, fi_strerror(-ret));
    //     return -ret;
    // }

    // // ???
    // ret = fi_bind(&ep->fid, &send_cq->fid, FI_SEND);
    // ret = fi_bind(&ep->fid, &recv_cq->fid, FI_RECV);
    // if (ret) {
    //     error("Bind completion queue to endpoint failed with errno: %d (%s)\n", -ret, fi_strerror(-ret));
    //     return -ret;
    // }

    // // transit the endpoint to active state
    // ret = fi_enable(ep);
    // if (ret) {
    //     error("Enable endpoint failed with errno: %d (%s)\n", -ret, fi_strerror(-ret));
    //     return -ret;
    // }

    // // register memory region
    // char *buffer = calloc(1, BUFFER_SIZE);
    // if (!buffer) {
    //     error("Allocate buffer failed\n");
    //     return -FI_ENOMEM;
    // }

    // ret = fi_mr_reg(domain, buffer, BUFFER_SIZE, 0, 0, 0, 0, &mr, NULL);
    // if (ret) {
    //     error("Register memory region failed with errno: %d (%s)\n", -ret, fi_strerror(-ret));
    //     return -ret;
    // }

    // // post recv request
    // char *msg_buffer = calloc(1, BUFFER_SIZE);
    // ret = fi_recv(ep, msg_buffer, BUFFER_SIZE, fi_mr_desc(mr), FI_ADDR_UNSPEC, NULL);
    // if (ret) {
    //     error("Post recv request failed with errno: %d (%s)\n", -ret, fi_strerror(-ret));
    //     return -ret;
    // }

    // ret = fi_accept(ep,NULL, 0);
    // if (ret) {
    //     error("Accept failed with errno: %d (%s)\n", -ret, fi_strerror(-ret));
    //     return -ret;
    // }

    // // waiting for client to complete the connection
    // int n = fi_eq_sread(eq, &event_id, (void *)&event_entry, sizeof(event_entry), 
    //     TIMEOUT, 0);
    // if (n < 0) {
    //     error("Event sread failed with errno %d (%s)\n", n, fi_strerror(n));
    //     return n;
    // }
    // if (n != sizeof(event_entry)) {
    //     error("Unexpected event size %d, expected %d\n", n, sizeof(event_entry));
    //     return -FI_EOTHER;
    // }
    // if (event_id != FI_CONNECTED) {
    //     error("Unexpected event id %d, expected FI_CONNECTED %d\n", event_id, FI_CONNECTED);
    //     return -FI_EOTHER;
    // }
    // if (event_entry.fid != &ep->fid) {
    //     error("Unexpected event fid %p, expected %p\n", event_entry.fid, &ep->fid);
    //     return -FI_EOTHER;
    // }

    // // sread more?
    // n = fi_eq_sread(eq, &event_id, (void *)&event_entry, sizeof(event_entry), 
    //     TIMEOUT, 0);

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