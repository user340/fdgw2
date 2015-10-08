/*
/* library for IP Filter based transparent mode proxy
/*
/*   See http://www.fml.org/software/libtransparent/ for more details.
/* 
/* Copyright (C) 2001,2002 Ken'ichi Fukamachi <fukachan@fml.org>
/*  All rights reserved. This program is free software; you can
/*  redistribute it and/or modify it under the same terms as NetBSD itself. 
/*
/* $FML: ipfilter.h,v 1.7 2002/02/03 01:36:25 fukachan Exp $
/*
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <ctype.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <syslog.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/errno.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip_compat.h>
#include <netinet/ip_fil.h>
#include <netinet/ip_nat.h>

int ipfilter_get_real_dst();
