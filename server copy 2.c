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

struct listen_socket
{
  struct fi_info* fi;
  struct fid_fabric *fabric;
  struct fid_eq *eq;
  struct fid_pep *pep;
  int fd;
};

struct data_socket {
    struct fid_domain *domain;
    struct fid_ep *ep;
    struct fid_cq* cq;
    int fd;
    struct fid_mr *mr;

    char* buf;
    size_t len;
};

int main() {
    int ret;
    struct listen_socket lsocket;
    // struct fi_info *fi;  // fabric info
    // struct fid_fabric *fabric;  // fabric domain
    // struct fid_eq *eq;  // event queue
    // struct fid_pep *pep;  // passive endpoint
    // int fd; // file descriptor for event queue
    int efd = epoll_create(64); // epoll is for I/O multiplexing

    struct epoll_event ev;
    struct fi_info *hints;
    struct fi_eq_attr eq_attr;
    eq_attr.wait_obj = FI_WAIT_FD;

    // prepare hints. The hints are used to specify a service to be used
    hints = fi_allocinfo();
    hints->ep_attr->type = FI_EP_MSG;
    hints->caps = FI_MSG;
    hints->mode = FI_LOCAL_MR;

    ret = fi_getinfo(FI_VERSION(1, 0), "127.0.0.1", NULL, FI_SOURCE, hints, &(lsocket.fi));
    if (ret) {
        error("fi_getinfo failed: %d '%s'\n", ret, fi_strerror(-ret));
    }
    debug("fi_getinfo succeeded\n");

    printf("info: %s\n", fi_tostr(lsocket.fi, FI_TYPE_INFO));

    // open fabric network
    ret = fi_fabric(lsocket.fi->fabric_attr, &(lsocket.fabric), NULL);
    if (ret) {
        error("fi_fabric failed: %d\n", ret);
    }
    debug("open fi_fabric succeeded\n");
   
    // open event queue
    ret = fi_eq_open(lsocket.fabric, &eq_attr, &(lsocket.eq), NULL);
    if (ret) {
        error("fi_eq_open failed: %d\n", ret);
    }
    debug("open fi event queue succeeded\n");

    // open passive endpoint
    ret = fi_passive_ep(lsocket.fabric, lsocket.fi, &(lsocket.pep), NULL);
    if (ret) {
        error("fi_passive_ep failed: %d\n", ret);
    }
    debug("open fi domain succeeded\n");

    // listen?
    ret = fi_listen(lsocket.pep);
    if (ret) {
        error("fi_listen failed: %d '%s'\n", ret, fi_strerror(-ret));
    }
    debug("listening...\n");

    // wait for connection?
    ret = fi_control(&(lsocket.eq)->fid, FI_GETWAIT, &(lsocket.fd));
    if (ret) {
        error("fi_control failed: %d '%s'\n", ret, fi_strerror(-ret));
    }
    debug("fi get a wait succeeded\n");

    ev.events = EPOLLIN;
    ev.data.ptr = &lsocket;
    // add fd to the interest list of epoll efd
    ret = epoll_ctl(efd, EPOLL_CTL_ADD, lsocket.fd, &ev);
    if (ret) {
        error("epoll_ctl failed: %d\n", ret);
    }

    struct epoll_event events[1024];  // events array?
    int i, n;
    while (1) {
        // wait for events on the queue
        n = epoll_wait(efd, events, 1024, -1);
        if (n < 0) {
            error("epoll_wait failed: %d\n", n);
        }
        for (i = 0; i < n; i++) {
            // handle each event
            if (events[i].data.ptr == &lsocket) {
                //connection event
                struct fi_eq_cm_entry entry; // ?
                ssize_t rd;  //?
                uint32_t event;  //?

                rd = fi_eq_sread(lsocket.eq, &event, &entry, sizeof(entry), -1, 0);
                if (rd < 0) {
                    error("fi_eq_sread failed: %d\n", rd);
                } else if (rd != sizeof(entry)) {
                    error("fi_eq_sread failed: read %ld bytes, expected %ld\n", rd, sizeof(entry));
                }
                // info("Receive connection event: %s\n", fi_tostr(&event, FI_CM_EN));

                switch (event) {
                    case FI_CONNREQ: ;
                        // connection request
                        // handle_connection_request(efd, entry.info, &lsocket);
                        struct data_socket *rsocket = malloc(sizeof(struct data_socket));
                        ret = fi_domain(lsocket.fabric, entry.info, &(rsocket->domain), NULL);
                        if (ret) {
                            error("fi_domain failed: %d\n", ret);
                        }
                        debug("open fi domain succeeded\n");

                        ret = fi_endpoint(rsocket->domain, entry.info, &(rsocket->ep), NULL);
                        if (ret) {
                            error("fi_endpoint failed: %d\n", ret);
                        }
                        debug("open fi endpoint succeeded\n");

                        ret = fi_ep_bind(rsocket->ep, &(lsocket.eq)->fid, 0);
                        if (ret) {
                            error("fi_ep_bind failed: %d\n", ret);
                        }
                        debug("fi_ep_bind succeeded\n");

                        // completion queue
                        struct fi_cq_attr cq_attr;
                        cq_attr.size = 64;
                        cq_attr.flags = 0;
                        cq_attr.format = FI_CQ_FORMAT_CONTEXT;
                        cq_attr.wait_obj = FI_WAIT_FD; //???
                        cq_attr.signaling_vector = 0;  //???
                        cq_attr.wait_cond = FI_WAIT_NONE; //???
                        cq_attr.wait_set = NULL;  //???

                        ret = fi_cq_open(rsocket->domain, &cq_attr, &(rsocket->cq), NULL);
                        if (ret) {
                            error("fi_cq_open failed: %d\n", ret);
                        }

                        ret = fi_ep_bind(rsocket->ep, &(rsocket->cq)->fid, FI_TRANSMIT | FI_RECV);
                        if (ret) {
                            error("fi_ep_bind failed: %d\n", ret);
                        }

                        ret = fi_accept(rsocket->ep, NULL, 0);
                        if (ret) {
                            error("fi_accept failed: %d '%s'\n", ret, fi_strerror(-ret));
                        }

                        // wait for the connection to be established  ??                        
                        ssize_t rd;
                        uint32_t event;
                        struct fi_eq_cm_entry entry;
                        rd = fi_eq_sread(lsocket.eq, &event, &entry, sizeof(entry), -1, 0);
                        if (rd < 0) {
                            error("fi_eq_sread failed: %d\n", rd);
                        } else if (rd != sizeof(entry)) {
                            error("fi_eq_sread failed: read %ld bytes, expected %ld\n", rd, sizeof(entry));
                        }

                        if (event != FI_CONNECTED || entry.fid != &rsocket->ep->fid) {
                            error("Unexpected CM event %d fid %p (ep %p)", event, entry.fid, rsocket->ep);
                        }

                        debug("Connection established\n");
                        // cannot

                        break;
                    // case FI_CONNECTED:
                    //     // connection established
                    //     break;
                    case FI_SHUTDOWN:
                        // connection shutdown
                        debug("Connection shutdown\n");
                        break;
                    default:
                        error("Unknown event: %d\n", event);
                }
            } else {
                // data event
                
            }
        }
    }
    
    return 0;
}