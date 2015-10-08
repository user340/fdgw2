/*
 * Copyright (C) 2001 Ken'ichi Fukamachi <fukachan@fml.org>
 *
 * All rights reserved. This program is free software; you can
 * redistribute it and/or modify it under the same terms as NetBSD itself.
 *
 * $FML: __h_errno_set.c,v 1.2 2001/09/29 08:15:42 fukachan Exp $
 *
 */

#include <sys/types.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "minires/minires.h"
#include "arpa/nameser.h"

extern int h_errno;

void
__h_errno_set(struct __res_state *res, int err) {

	h_errno = res->res_h_errno = err;
}
