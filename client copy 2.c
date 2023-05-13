// devived from https://gist.github.com/joerns/7f6a97a6504214db04c68df6dac5b253
#include <stdio.h>
#include <stdlib.h>

#include <sys/epoll.h>
#include <rdma/fi_errno.h>
#include <rdma/fabric.h>
#include <rdma/fi_eq.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_cm.h>
#include <rdma/fi_errno.h>

#include "common.h"

struct send_socket
{
  struct fi_info* fi;
  struct fid_fabric *fabric;

  struct fid_eq *eq;
  struct fid_domain *domain;
  struct fid_ep *ep;
  struct fid_cq* cq;
  int fd;

};

int main() {
    int ret;

    int efd = epoll_create(64); 
    struct send_socket *ssocket = malloc(sizeof(struct send_socket));
    struct fi_info *hints;
    struct fi_eq_attr eq_attr;
    eq_attr.wait_obj = FI_WAIT_UNSPEC;

    // prepare hints
    hints = fi_allocinfo();
    hints->ep_attr->type = FI_EP_MSG;
    hints->caps = FI_MSG;
    hints->mode = FI_LOCAL_MR;
    // use local memory region for both send and receive?
    // transmit abilities
    hints->tx_attr->mode = FI_LOCAL_MR;
    // receive abilities
    hints->rx_attr->mode = FI_LOCAL_MR;

    ret = fi_getinfo(FI_VERSION(1, 0), "172.31.12.210", NULL, 0, hints, &(ssocket->fi));
    if (ret) {
        error("fi_getinfo failed: %d '%s'\n", ret, fi_strerror(-ret));
    }
    debug("fi_getinfo succeeded\n");

    printf("info: %s\n", fi_tostr(ssocket->fi, FI_TYPE_INFO));

    ret = fi_fabric(ssocket->fi->fabric_attr, &(ssocket->fabric), NULL);
    if (ret) {
        error("fi_fabric failed: %d\n", ret);
    }
    debug("open fi_fabric succeeded\n");
   
    ret = fi_eq_open(ssocket->fabric, &eq_attr, &(ssocket->eq), NULL);
    if (ret) {
        error("fi_eq_open failed: %d\n", ret);
    }
    debug("open fi event queue succeeded\n");

    // open domain?
    ret = fi_domain(ssocket->fabric, ssocket->fi, &(ssocket->domain), NULL);
    if (ret) {
        error("fi_domain failed: %d: '%s'\n", ret, fi_strerror(-ret));
    }
    debug("open fi domain succeeded\n");

    // open endpoint
    ret = fi_endpoint(ssocket->domain, ssocket->fi, &(ssocket->ep), NULL);
    if (ret) {
        error("fi_endpoint failed: %d\n", ret);
    }
    debug("create fi (active?) endpoint succeeded\n");

    // bind event queue
    ret = fi_ep_bind(ssocket->ep, &(ssocket->eq->fid), 0);
    if (ret) {
        error("fi_ep_bind failed: %d\n", ret);
    }
    debug("bind fi event queue to endpoint succeeded\n");

    // 两次bind有什么区别？？
    // completion queue
    struct fi_cq_attr cq_attr;
    cq_attr.size = 64;
    cq_attr.flags = 0;
    cq_attr.format = FI_CQ_FORMAT_CONTEXT;
    cq_attr.wait_obj = FI_WAIT_FD; //???
    cq_attr.signaling_vector = 0;  //???
    cq_attr.wait_cond = FI_WAIT_NONE; //???
    cq_attr.wait_set = NULL;  //???

    ret = fi_cq_open(ssocket->domain, &cq_attr, &(ssocket->cq), NULL);
    if (ret) {
        error("fi_cq_open failed: %d\n", ret);
    }
    debug("open fi completion queue succeeded\n");

    // bind completion queue
    ret = fi_ep_bind(ssocket->ep, &(ssocket->cq->fid), FI_TRANSMIT | FI_RECV);
    if (ret) {
        error("fi_ep_bind failed: %d\n", ret);
    }
    debug("bind fi completion queue to endpoint succeeded\n");

    // connect
    ret = fi_connect(ssocket->ep, ssocket->fi->src_addr, NULL, 0);
    if (ret) {
        error("fi_connect failed: %d\n", ret);
    }

    ssize_t rd;
    uint32_t event;
    struct fi_eq_cm_entry entry;  // event entry
    rd = fi_eq_sread(ssocket->eq, &event, &entry, sizeof(entry), -1, 0);
    if (rd == -FI_EAVAIL) {
        // ???
        struct fi_eq_err_entry err_entry;
        ret = fi_eq_readerr(ssocket->eq, &err_entry, 0);
        if (ret < 0) {
            error("fi_eq_readerr failed: %d\n", ret);
        }

        char buf[1024];
        fprintf(stderr, "error event: %s\n", fi_eq_strerror(ssocket->eq, err_entry.prov_errno, 
            err_entry.err_data, buf, 1024));
        // exit(EXIT_FAILURE);
    } else if (rd < 0) {
        error("fi_eq_sread failed: %d '%s]\n", rd, fi_strerror(-rd));
    } else if (rd != sizeof(entry)) {
        error("fi_eq_sread returned %ld bytes, expected %ld\n", rd, sizeof(entry));
    }
    debug("Received event\n");

    if (event != FI_CONNECTED || entry.fid != &(ssocket->ep->fid)) {
        error("Unexpected CM event %d fid %p (ep %p)", event, entry.fid, ssocket->ep);
    }
    
    debug("Connection established\n");
    // cannot

    return 0;
}