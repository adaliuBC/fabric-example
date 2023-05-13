#include <stdio.h>
#include <stdlib.h>

#include <sys/epoll.h>
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

struct recv_socket
{
  struct fid_domain *domain;
  struct fid_ep *ep;
  struct fid_cq* cq;
  int fd;
  struct fid_mr *mr;

  char* buf;
  size_t len;
};

void listen_server(int efd, struct listen_socket* socket)
{
  int ret;
  struct epoll_event ev;
  struct fi_info* hints;
  struct fi_eq_attr eq_attr = {
    .wait_obj = FI_WAIT_FD
  };

  hints = fi_allocinfo();
  hints->ep_attr->type  = FI_EP_MSG;
  hints->caps = FI_MSG;
  hints->mode = FI_LOCAL_MR;

  if(ret = fi_getinfo(FI_VERSION(1, 0), "127.0.0.1", "12345", FI_SOURCE, hints, &socket->fi)) {
    ERROR("fi_getinfo failed: %d '%s'", ret, fi_strerror(-ret));
  }

  printf("info: %s\n", fi_tostr(socket->fi, FI_TYPE_INFO));

  if(ret = fi_fabric(socket->fi->fabric_attr, &socket->fabric, NULL)) {
    ERROR("fi_fabric failed: %d", ret);
  }

  if(ret = fi_eq_open(socket->fabric, &eq_attr, &socket->eq, NULL)) {
    ERROR("fi_eq_open failed: %d", ret);
  }

  if(ret = fi_passive_ep(socket->fabric, socket->fi, &socket->pep, NULL)) {
    ERROR("fi_passive_ep failed: %d", ret);
  }

  if(ret = fi_pep_bind(socket->pep, &socket->eq->fid, 0)) {
    ERROR("fi_pep_bind failed: %d", ret);
  }
  
  if(ret = fi_listen(socket->pep)) {
    ERROR("fi_listen failed: %d '%s'", ret, fi_strerror(-ret));
  }  

  if(ret = fi_control(&socket->eq->fid, FI_GETWAIT, &socket->fd))
  {
    ERROR("fi_control failed: %d '%s'", ret, fi_strerror(-ret));
  }

  ev.events = EPOLLIN;
  ev.data.ptr = socket;
  if(ret = epoll_ctl(efd, EPOLL_CTL_ADD, socket->fd, &ev)) {
    ERROR("epoll_ctl failed: %d", ret);
  }
}

void handle_connection_request(int efd, struct fi_info* info, struct listen_socket* lsocket)
{
  struct epoll_event ev;
  int ret;
  ssize_t rd;
  uint32_t event;
  struct fi_eq_cm_entry entry;

  struct recv_socket * rsocket = malloc(sizeof(struct recv_socket));

  if(ret = fi_domain(lsocket->fabric, info, &rsocket->domain, NULL)) {
    ERROR("fi_domain failed: %d", ret);
  }

  if(ret = fi_endpoint(rsocket->domain, info, &rsocket->ep, NULL)) {
    ERROR("fi_endpoint failed: %d", ret);
  }

  if(ret = fi_ep_bind((rsocket->ep), &lsocket->eq->fid, 0)) {
    ERROR("fi_ep_bind failed: %d", ret);
  }

  struct fi_cq_attr cq_attr;
  cq_attr.size = 64;      /* # entries for CQ */
  cq_attr.flags = 0;     /* operation flags */
  cq_attr.format = FI_CQ_FORMAT_CONTEXT;    /* completion format */
  cq_attr.wait_obj= FI_WAIT_FD;  /* requested wait object */
  cq_attr.signaling_vector = 0; /* interrupt affinity */
  cq_attr.wait_cond = FI_WAIT_NONE; /* wait condition format */
  cq_attr.wait_set = NULL;  /* optional wait set */

  if(ret = fi_cq_open(rsocket->domain, &cq_attr, &rsocket->cq, NULL))
  {
    ERROR("fi_cq_open failed: %d", ret);
  }

  if(ret = fi_ep_bind((rsocket->ep), &rsocket->cq->fid, FI_TRANSMIT|FI_RECV)) {
    ERROR("fi_ep_bind failed: %d", ret);
  }


  if(ret = fi_accept(rsocket->ep, NULL, 0)) {
    ERROR("fi_accept failed: %d '%s'", ret, fi_strerror(-ret));
  }

  /* Wait for the connection to be established */
  rd = fi_eq_sread(lsocket->eq, &event, &entry, sizeof entry, -1, 0);
  if(rd < 0)
  {
    ERROR("fi_eq_sread failed: %d '%s'", rd, fi_strerror(-rd));
  }
  if (rd != sizeof entry) {
    ERROR("reading from event queue failed (after listen): rd=%d", rd);
  }

  if (event != FI_CONNECTED || entry.fid != &rsocket->ep->fid) {
    ERROR("Unexpected CM event %d fid %p (ep %p)", event, entry.fid, rsocket->ep);
  }


  printf("connected\n");

  rsocket->len = 1024*1024;
  rsocket->buf = malloc(rsocket->len);
  if(ret = fi_mr_reg(rsocket->domain, rsocket->buf, rsocket->len, FI_RECV, 0, 0, 0, &rsocket->mr, NULL))
  {
    ERROR("fi_mr_reg failed: %d", ret);
  }

  void* desc = fi_mr_desc(rsocket->mr);

  if(ret = fi_recv(rsocket->ep, rsocket->buf, rsocket->len, desc, 0, NULL))
  {
    ERROR("fi_recv failed: %d, '%s'", ret, fi_strerror(-ret));
  }

  if(ret = fi_control(&rsocket->cq->fid, FI_GETWAIT, &rsocket->fd))
  {
    ERROR("fi_control failed: %d", ret);
  }
  printf("CQ FD: %d\n", rsocket->fd);


  ev.events = EPOLLIN;
  ev.data.ptr = rsocket;
  if(ret = epoll_ctl(efd, EPOLL_CTL_ADD, rsocket->fd, &ev)) {
    ERROR("epoll_ctl failed: %d", ret);
  }
}


int main()
{
  int ret;
  struct listen_socket lsocket;
  int efd = epoll_create(64);

  listen_server(efd, &lsocket);

  struct epoll_event ev;
  struct epoll_event events[1024];
  int i, n;
  while(1)
  {
    n = epoll_wait(efd, events, 1024, -1);
    for(i = 0; i < n; i++)
    {
      if(events[i].data.ptr == &lsocket)
      {
        // This is a connection request
        struct listen_socket * socket = events[i].data.ptr;
        struct fi_eq_cm_entry entry;
        ssize_t rd;
        uint32_t event;

        rd = fi_eq_sread(socket->eq, &event, &entry, sizeof entry, 0, 0);
        if(rd < 0)
        {
            ERROR("fi_eq_sread failed: %d '%s'", rd, fi_strerror(-rd));
        }
        if (rd != sizeof entry) {
        ERROR("reading from event queue failed (after listen): rd=%d", rd);
        }
        printf("received event\n");
        
        switch(event)
        {
        case FI_CONNREQ:
            handle_connection_request(efd, entry.info, socket);
            break;
        case FI_SHUTDOWN:
            printf("remote disconnected\n");
            break;
        default:
            ERROR("unexpected event: %d", event);
        } 
      }
      else
      {
        // This is a completion
        struct fi_cq_entry completion_entry;
        struct recv_socket * socket = events[i].data.ptr;
        ret = fi_cq_sread(socket->cq, &completion_entry, 1, NULL, 0);
        if(ret < 0) {
            // my guess why this happens: we set FI_TRANSMIT on the CQ, although we do not need it. This is due to a bug in libfabric 1.2.0
            if(ret == -FI_EAGAIN)
                continue; 
            ERROR("fi_cq_sread failed: %d '%s'", ret, fi_strerror(-ret));
        }
        printf("message received: %s\n", socket->buf);
      }
    }
  }

  return 0;
}

