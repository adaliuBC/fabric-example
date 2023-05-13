#ifndef COMMON_H
#define COMMON_H

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <rdma/fabric.h>
#include <rdma/fi_eq.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_cm.h>
#include <rdma/fi_errno.h>

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

double now()
{
  struct timespec t;
  if(clock_gettime(CLOCK_MONOTONIC, &t))
    error("clock_gettime failed: %d (%s)", errno, strerror(errno));
  return ((double)t.tv_sec) + 1e-9*t.tv_nsec;
}


void handle_cq_error(struct fid_cq* cq)
{
  int r;
  struct fi_cq_err_entry err_entry;
  if((r = fi_cq_readerr(cq, &err_entry, 0)) < 0)
    {
      error("fi_eq_readerr failed: %d", r);
    }
  fprintf(stderr, "error in the CQ: %s / %s\n", fi_strerror(err_entry.err), fi_strerror(err_entry.prov_errno));
  exit(-1);
}

/* 
 * Checks if any completions arrived and returns their number.  Does
 * not block.
 */
int check_for_completions(struct fid_cq* cq, struct fi_cq_entry* completions, unsigned max_completions)
{
  ssize_t num_completions = fi_cq_read(cq, completions, max_completions);
  if(num_completions < 0)
    {
      if(num_completions == -FI_EAGAIN)
	return 0;
      if(num_completions == -FI_EAVAIL)
	{
	  handle_cq_error(cq);
	}

      error("fi_cq_read failed: %d (%s)", (int)num_completions, fi_strerror(-num_completions));
    }

  return num_completions;
}


/* 
 * Blocks until `num` completions arrived.
 */
int wait_for_completions(struct fid_cq* cq, unsigned num, struct fi_cq_entry* completions, unsigned max_completions)
{
  if(num == 0)
    return 0;
  
  unsigned completions_arrived = 0;
  while(completions_arrived < num)
    {
      ssize_t num_completions = fi_cq_read(cq, completions, max_completions);
      if(num_completions < 0)
	{
	  if(num_completions == -FI_EAGAIN)
	    continue;
	  if(num_completions == -FI_EAVAIL)
	    handle_cq_error(cq);
	  error("fi_cq_read failed: %d (%s)", (int)num_completions, fi_strerror(-num_completions));
	}
      completions_arrived += num_completions;
    }
  return completions_arrived;
}




#endif
