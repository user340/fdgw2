#
# Copyright (C) 2001 Ken'ichi Fukamachi <fukachan@fml.org>
#
# All rights reserved. This program is free software; you can
# redistribute it and/or modify it under the same terms as NetBSD itself.
#
# $FML: Makefile,v 1.5 2001/12/16 09:42:27 fukachan Exp $
#

CFLAGS = -I ${BSDSRCDIR}/usr.sbin/dhcp/includes \
	 -I  ${BSDSRCDIR}/dist/dhcp/includes

__h_errno_set.o: __h_errno_set.c
	${CC} ${CFLAGS} -c __h_errno_set.c
