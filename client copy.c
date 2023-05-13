#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <rdma/fi_errno.h>

#include <sys/epoll.h>

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


void print_sockaddr_type(struct fi_info* fi)
{
  switch(fi->addr_format) {
    case FI_SOCKADDR_IN:
      printf("sockaddr type: FI_SOCKADDR_IN\n");
      break;
    case FI_FORMAT_UNSPEC:
      printf("sockaddr type: FI_FORMAT_UNSPEC\n");
      break;
  case FI_SOCKADDR_IN6:
      printf("sockaddr type: FI_SOCKADDR_IN6\n");
      break;
  case FI_SOCKADDR_IB:
      printf("sockaddr type: FI_SOCKADDR_IB\n");
      break;
  case FI_SOCKADDR:
      printf("sockaddr type: FI_SOCKADDR\n");
      break;
  default:
      fprintf(stderr, "error: sockaddr type unkown\n");
      break;
  }
}



void send_socket_connect(int efd, struct send_socket * socket)
{
  struct fi_eq_cm_entry entry;
  uint32_t event;
  ssize_t rd;
  int ret;
  struct fi_info* hints;
  struct fi_eq_attr eq_attr = {
    .wait_obj = FI_WAIT_UNSPEC // waiting only through fi_ calls (no epoll)
  };

  hints = fi_allocinfo();
  hints->ep_attr->type  = FI_EP_MSG;
  hints->caps = FI_MSG;
  hints->mode   = FI_LOCAL_MR;
  hints->tx_attr->mode = FI_LOCAL_MR;
  hints->rx_attr->mode = FI_LOCAL_MR;

  if(ret = fi_getinfo(FI_VERSION(1, 0), "10.10.0.196", "12345", 0, hints, &socket->fi))
  {
    ERROR("fi_getinfo failed: %d '%s'", ret, fi_strerror(-ret));
  }

  print_sockaddr_type(socket->fi);

  printf("info: %s\n", fi_tostr(socket->fi, FI_TYPE_INFO));

  if(ret = fi_fabric(socket->fi->fabric_attr, &socket->fabric, NULL)) {
    ERROR("fi_fabric failed: %d", ret);
  }

  if(ret = fi_eq_open(socket->fabric, &eq_attr, &socket->eq, NULL)) {
    ERROR("fi_eq_open failed: %d", ret);
  }

  if(ret = fi_domain(socket->fabric, socket->fi, &socket->domain, NULL)) {
    ERROR("fi_domain failed: %d", ret);
  }

  if(ret = fi_endpoint(socket->domain, socket->fi, &socket->ep, NULL)) {
    ERROR("fi_endpoint failed: %d", ret);
  }

  if(ret = fi_ep_bind((socket->ep), &socket->eq->fid, 0)) {
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

  if(ret = fi_cq_open(socket->domain, &cq_attr, &socket->cq, NULL))
  {
    ERROR("fi_cq_open failed: %d '%s'", ret, fi_strerror(-ret));
  }

  if(ret = fi_ep_bind((socket->ep), &socket->cq->fid, FI_TRANSMIT|FI_RECV)) {
    ERROR("fi_ep_bind failed: %d", ret);
  }

  /*  if(ret = fi_enable(socket->ep)) {
    ERROR("fi_enable failed: %d", ret);
    }*/

  /* Connect to server */
  if(ret = fi_connect(socket->ep, socket->fi->dest_addr, NULL, 0)) {
    ERROR("fi_connect failed: %d '%s'", ret, fi_strerror(-ret));
  }

   /* Wait for the connection to be established */
  rd = fi_eq_sread(socket->eq, &event, &entry, sizeof entry, -1, 0);
  if(rd == -FI_EAVAIL)
  {
    struct fi_eq_err_entry err_entry;
    if((ret = fi_eq_readerr(socket->eq, &err_entry, 0)) < 0)
    {
      ERROR("fi_eq_readerr failed: %d", ret);
    }

    char buf[1024];
    fprintf(stderr, "error event: %s\n", fi_eq_strerror(socket->eq, err_entry.prov_errno, 
      err_entry.err_data, buf, 1024));
    exit(EXIT_FAILURE);
  }
  else if(rd < 0)
  {
    ERROR("fi_eq_sread failed: %d '%s'", rd, fi_strerror(-rd));
  }
  if (rd != sizeof entry) {
    ERROR("reading from event queue failed (after connect): rd=%d", rd);
  }
  printf("received event\n");

  if (event != FI_CONNECTED || entry.fid != &socket->ep->fid) {
    ERROR("Unexpected CM event %d fid %p (ep %p)", event, entry.fid, socket->ep);
  }

  printf("connected\n");

  if(ret = fi_control(&socket->cq->fid, FI_GETWAIT, &socket->fd))
  {
    ERROR("fi_control failed: %d", ret);
  }
  printf("CQ FD: %d\n", socket->fd);


  struct epoll_event ev;
  ev.events = EPOLLIN;
  ev.data.ptr = socket;
  if(ret = epoll_ctl(efd, EPOLL_CTL_ADD, socket->fd, &ev)) {
    ERROR("epoll_ctl failed: %d", ret);
  }
}


void send_socket_send(struct send_socket* socket, char* buf, size_t len)
{
  int ret;
  struct fid_mr *mr;

  if(ret = fi_mr_reg(socket->domain, buf, len, FI_SEND, 0, 0, 0, &mr, NULL))
  {
    ERROR("fi_mr_reg failed: %d", ret);
  }

  void* desc = fi_mr_desc(mr);

  if(ret = fi_send(socket->ep, buf, len, desc, 0, NULL))
  {
    ERROR("fi_send failed: %d, '%s'", ret, fi_strerror(-ret));
  }
}


int main()
{
  int ret, i, n;
  int efd = epoll_create(64);

  struct send_socket socket;
  send_socket_connect(efd, &socket);

  char buf[] = "hello, world!";
  size_t len = 14;
  send_socket_send(&socket, buf, len);

  struct epoll_event events[64];
  n = epoll_wait(efd, events, 64, -1);
  if(n < 0) {
    ERROR("epoll_wait failed: %d", n);
  }

  for(i=0; i<n; i++)
  {
    // we know these are send sockets
    struct send_socket* socket = events[i].data.ptr;
    struct fi_cq_entry completion_entry;
    fi_cq_read(socket->cq, &completion_entry, 1);
    printf("message sent\n");
  }

  return 0;
}

