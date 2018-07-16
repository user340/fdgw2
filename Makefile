# Copyright (C) 2015,2016,2017,2018 Yuuki Enomoto <uki@e-yuuki.org>
#
# All rights reserved. This program is free software; you can
# redistribute it and/or modify it under the same terms as NetBSD itself.
#
# Copyright (C) 2001,2002,2003,2004 Ken'ichi Fukamachi <fukachan@fml.org>
#
# All rights reserved. This program is free software; you can
# redistribute it and/or modify it under the same terms as NetBSD itself.
#

#
# specify default image
#
ARCH?=			i386
MODEL?=         rescue
KERNEL_CONF?=	FDGW
BIOSBOOT?=		bootxx_ffsv1

#
# programs and directories
#
BSDSRCDIR?=	 	/usr/src
TOOL_DIR?=		/usr/tools
OBJ_DIR?=		/usr/obj
SH?=			/bin/sh
GMAKE?=			${TOOL_DIR}/bin/nbgmake
DESTDIR=		${OBJ_DIR}/destdir.${ARCH}
OPSYS=			`uname`

UTILS_DIR?=		src/utils

MAKE_PARAMS = 	MODEL=${MODEL} \
				KERNEL_CONF=${KERNEL_CONF} \
				BIOSBOOT=${BIOSBOOT} \
				OPSYS=NetBSD \
				SH=${SH}	\
				UTILS_DIR=${UTILS_DIR:S|^src/||} \
				TOOL_DIR=${TOOL_DIR} \
				OBJ_DIR=${OBJ_DIR} \
				DESTDIR=${DESTDIR} \
				BSDSRCDIR=${BSDSRCDIR}

all:
	@ echo "make build   (need NOT priviledge)"
	@ echo "make image   (need root priviledge)"

build:
	${SH} ./${UTILS_DIR}/prepare_workdir.sh ${ARCH}.${MODEL}
	(cd obj.${ARCH}.${MODEL};${MAKE} ${MAKE_PARAMS} build )

image:
	@ echo ""	
	@ echo "\"make image\" needs root privilege"
	@ echo ""	
	(cd obj.${ARCH}.${MODEL}; ${MAKE} ${MAKE_PARAMS} image )

tools:
	cd ${BSDSRCDIR} && sh build.sh -U -u -O ${OBJ_DIR} -T ${TOOL_DIR} -m ${ARCH} tools

distribution:
	cd ${BSDSRCDIR} && sh build.sh -U -u -O ${OBJ_DIR} -T ${TOOL_DIR} -m ${ARCH} distribution

clean:
	rm -fr obj.${ARCH}.${MODEL}
	rm -fr src/NetBSD
	rm -fr image.${ARCH}
	rm -fr src/etc/*

