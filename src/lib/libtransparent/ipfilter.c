/*
/* library for IP Filter based transparent mode proxy
/* 
/*   See http://www.fml.org/software/libtransparent/ for more details.
/*
/* Copyright (C) 2001,2002 Ken'ichi Fukamachi <fukachan@fml.org>
/*  All rights reserved. This program is free software; you can
/*  redistribute it and/or modify it under the same terms as NetBSD itself. 
/*
/* $FML: ipfilter.c,v 1.17 2002/04/13 15:09:45 fukachan Exp $
/*
*/

#include "ipfilter.h"


/* Descriptions: extract real destination from kernel.
/*               update *np in the argument as a result.
/*    Arguments: HANDLE(fd)
/*               POINTER(sockaddr_in *laddrp)
/*               POINTER(sockaddr_in *faddrp)
/*               POINTER(natlookup_t *np)
/* Side Effects: *laddrp == local address which accpetd this connection.
/*               *faddrp == source address of this connection
/*               We can know laddrp and faddrp from socket (fd).
/*               *np knows the real destination taken by system call.
/* Return Value: 1 (success) or 0 (failure)
/*               return information in *np.
*/
static int
get_realdst(fd, laddrp, faddrp, np)
     int fd;
     struct sockaddr_in *laddrp, *faddrp;
     natlookup_t *np;
{
  int natfd, slen;
  char errmsg[BUFSIZ];

  slen = sizeof(struct sockaddr_in);
  if (getsockname(fd, (struct sockaddr *)laddrp, &slen) < 0) {
    snprintf(errmsg, sizeof(errmsg), "getsockname failed: %s", strerror(errno));
    perror(errmsg);
    return(0);
  }

  if (getpeername(fd, (struct sockaddr *)faddrp, &slen) < 0) {
    snprintf(errmsg, sizeof(errmsg), "getpeername failed: %s", strerror(errno));
    perror(errmsg);
    return(0);
  }

  np->nl_inport   = ntohs(laddrp->sin_port);
  np->nl_outport  = ntohs(faddrp->sin_port);
  np->nl_inip     = laddrp->sin_addr;
  np->nl_outip    = faddrp->sin_addr;
  np->nl_flags    = IPN_TCP;

  if ((natfd = open(IPL_NAT, O_RDONLY)) < 0) {
    perror("open");
    return(0);
  }

  if (ioctl(natfd, SIOCGNATL, &np) == -1) {
    snprintf(errmsg, sizeof(errmsg), "SIOCGNATL failed: %s", strerror(errno));
    perror(errmsg);
    close(natfd);
    return(0);
  }
  close(natfd);

  return(1);
}


/* Descriptions: get the real destination.
/*    Arguments: HANDLE(fd) POINTER(struct sockaddr_in *dstp)
/* Side Effects: rewrite *dstp (destination address) to real destination
/* Return Value: NUM
*/
int
ipfilter_get_real_dst(fd, dstp)
     int fd;
     struct sockaddr_in *dstp;
{
  natlookup_t natlookup, *np;
  struct sockaddr_in laddr;
  struct sockaddr_in faddr;
  int status = 0;

  /* get real destination in "np" */
  memset((char *)&natlookup, 0, sizeof(natlookup));
  memset((char *)&laddr,     0, sizeof(laddr));
  memset((char *)&faddr,     0, sizeof(faddr));

  np     = &natlookup;
  status = get_realdst(fd, &laddr, &faddr, np);

  if (status) {
    dstp->sin_addr = np->nl_realip;
    dstp->sin_port = np->nl_realport;
  }

  return(status);
}

