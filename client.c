#include <getopt.h>

#include "common.h"

char *src_uid, *dst_uid;
struct fid_fabric *fabric;
struct fid_domain *domain;
struct fid_ep *ep;
struct fid_eq *eq;
struct fid_cq *send_cq, *recv_cq;
int post_depth = 16;

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
            	// printf("Usage:\n");
                // printf("rdma_server: [-a <server_addr>] [-p <server_port>]\n");
                exit(1);
        }
    }

    // set the hints
    // hints->ep_attr->type = FI_EP_RDM;
    // getinfo with given provider
    ret = fi_getinfo(FI_VERSION(1,17), NULL, NULL, 0, hints, &fi);
    if (ret == -FI_ENODATA) {
        error("Could not find any optimal provider\n");
        return -FI_ENODATA;
    } else if (ret) {
        error("Failed to get info with errno: %d (%s)\n", -ret, fi_strerror(-ret));
        return -ret;
    }
    info("Found info:\n");
    debug("%s\n", fi_tostr(fi, FI_TYPE_INFO));

    ret = fi_fabric(fi->fabric_attr, &fabric, NULL);
    if (ret) {
        error("Failed to open fabric with errno: %d (%s)\n", -ret, fi_strerror(-ret));
        return -ret;
    }
    debug("Fabric opened\n");

    ret = fi_domain(fabric, fi, &domain, NULL);
    if (ret) {
        error("Failed to open domain with errno: %d (%s)\n", -ret, fi_strerror(-ret));
        return -ret;
    }
    debug("Domain opened\n");

    
    struct fi_eq_attr eq_attr = {
        .wait_obj = FI_WAIT_UNSPEC
    };
    ret = fi_eq_open(fabric, &eq_attr, &eq, NULL);
    if (ret) {
        error("Failed to open event queue with errno: %d (%s)\n", -ret, fi_strerror(-ret));
        return -ret;
    }
    debug("Event queue opened\n");

    // ???啥玩意？
    struct fi_info *provider = fi_allocinfo();
    provider->ep_attr->tx_ctx_cnt = (size_t)(post_depth + 1);
    provider->ep_attr->rx_ctx_cnt = (size_t)(post_depth + 1);
    provider->tx_attr->iov_limit = 1;
    provider->rx_attr->iov_limit = 1;
    provider->tx_attr->inject_size = 0;

    ret = fi_endpoint(domain, fi, &ep, NULL);  //???
    if (ret) {
        error("Failed to open (active) endpoint on client with errno: %d (%s)\n",
             -ret, fi_strerror(-ret));
        return -ret;
    }
    debug("Endpoint opened\n");

    struct fi_cq_attr cq_attr;
    cq_attr.size = 0;
    cq_attr.flags = 0;
    cq_attr.format = FI_CQ_FORMAT_MSG;
    cq_attr.wait_obj = 0;
    cq_attr.signaling_vector = 0;
    cq_attr.wait_cond = FI_CQ_COND_NONE;
    cq_attr.wait_set = 0;
    ret = fi_cq_open(domain, &cq_attr, &send_cq, NULL);
    if (ret) {
        error("Failed to open completion queue with errno: %d (%s)\n", -ret, fi_strerror(-ret));
        return -ret;
    }
    debug("Completion queue opened\n");

    ret = fi_ep_bind(ep, &eq->fid, 0);
    if (ret) {
        error("Failed to bind event queue to endpoint with errno: %d (%s)\n", -ret, fi_strerror(-ret));
        return -ret;
    }
    debug("Event queue binded to endpoint\n");
    ret = fi_ep_bind(ep, &send_cq->fid, 0);
    if (ret) {
        error("Failed to bind event queue to endpoint with errno: %d (%s)\n", -ret, fi_strerror(-ret));
        return -ret;
    }
    debug("Event queue binded to endpoint\n");

    ret = fi_enable(ep);
    if (ret) {
        error("Failed to enable endpoint with errno: %d (%s)\n", -ret, fi_strerror(-ret));
        return -ret;
    }
    debug("Endpoint enabled\n");

    // ???
    char *param = "Private Data";
    struct sockaddr_in addr = {0};
    // addr.sin_family = AF_INET;
    // addr.sin_port = htons(DEFAULT_RDMA_PORT); // ??
    // addr.sin_addr = htonl("172.31.12.210");   // ??
    ret = get_addr("172.31.12.210", (struct sockaddr *)&addr);
    if (ret) {
        error("Failed to get address with errno: %d (%s)\n", -ret, fi_strerror(-ret));
        return -ret;
    }
    ret = fi_connect(ep, (void *)&addr, (void *)param, sizeof(*param));
    if (ret) {
        error("Failed to connect to server with errno: %d (%s)\n", -ret, fi_strerror(-ret));
        return -ret;
    }
    debug("Connected to server\n");

    //wait for accept
    uint32_t event_id;
    struct fi_eq_cm_entry event_entry;
    ret = fi_eq_sread(eq, &event_id, (void *)&event_entry, sizeof(event_entry),
        TIMEOUT, 0);
    if (ret < 0) {
        error("Failed to read event queue with errno: %d (%s)\n", ret, fi_strerror(-ret));
        return ret;
    }
    if (ret < sizeof(event_entry)) {
        error("Event queue read less than expected\n");
        return -FI_EOTHER;
    }
    if (event_id != FI_CONNECTED) {
        error("Unexpected event id: %d, expecting %d\n", event_id, FI_CONNECTED);
        return -FI_EOTHER;
    }
    // ???
    if (event_entry.fid->context != NULL) {
        error("event_entry.fid->context %lx != %lx\n",
            (unsigned long)entry.fid->context, (usigned long)NULL);
        return -FI_EOTHER;
    }
    debug("Client connected\n");
    debug("Client private data (len %d): '%s'\n", (ret - sizeof(event_entry), event_entry.data));

    char *buffer = calloc(1, BUFFER_SIZE);
    if (!buffer) {
        error("Failed to allocate buffer\n");
        return -FI_ENOMEM;
    }
    ret = fi_mr_reg(domain, buffer, BUFFER_SIZE, 0, 0, 0, 0, &mr, NULL);
    if (ret < 0) {
        error("Failed to register memory region with errno: %d (%s)\n", -ret, fi_strerror(-ret));
        return -ret;
    }
    debug("Memory region registered\n");

    ret = fi_send(ep, buffer, BUFFER_SIZE, fi_mr_desc(mr), 0, NULL);
    if (ret < 0) {
        error("Failed to send message with errno: %d (%s)\n", -ret, fi_strerror(-ret));
        return -ret;
    }

    struct fi_cq_msg_entry completion_event;
    ret = fi_cq_sread(send_cq, &completion_event, 1);
    // TODO: assert this is a completion event
    if (ret < 0) {
        error("Failed to read completion queue with errno: %d (%s)\n", -ret, fi_strerror(-ret));
        return -ret;
    }
    //

    ret = fi_recv(ep, buffer, BUFFER_SIZE, fi_mr_desc(mr), FI_ADDR_UNSPEC, (void *)buffer);
    if (ret < 0) {
        error("Failed to receive message with errno: %d (%s)\n", -ret, fi_strerror(-ret));
        return -ret;
    }

    free(src_uid);
    free(dst_uid);
    if (fi) {
        fi_freeinfo(fi);
        fi = NULL;
    }
    if (provider) {
        fi_freeinfo(provider);
        provider = NULL;
    }
    if (hints) {
        freehints(hints);
        hints = NULL;
    }
    
    return ret;
}