# Copyright (C) 2015 Yuuki Enomoto <mail@e-yuuki.org>
#
# All rights reserved.
# Please read README for details.
#

# programs and directories

TOOL_DIR?=	/usr/tools
GMAKE?=		${TOOL_DIR}/bin/nbgmake
SH?=		/bin/sh

UTILS_DIR?=	src/utils

#
# specify default image
#
ARCH?=			i386
MODEL?=         standard
KERNEL_CONF?=	FDGW
BIOSBOOT?=		bootxx_ffsv1

# root privilege control
#SU_CMD?=	su - root -c

MAKE_PARAMS = 	MODEL=${MODEL} \
		KERNEL_CONF=${KERNEL_CONF} \
		BIOSBOOT=${BIOSBOOT} \
		OPSYS=NetBSD \
		SH=${SH}	\
		UTILS_DIR=${UTILS_DIR:S|^src/||} \
		TOOL_DIR=${TOOL_DIR}

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
#(cd obj.${ARCH}.${MODEL};\
#${SU_CMD} "cd `pwd`; ${MAKE} ${MAKE_PARAMS} image" )
	(cd obj.${ARCH}.${MODEL}; ${MAKE} ${MAKE_PARAMS} image)

stat:	obj.*.*/log.*
	@ ${SH} ${UTILS_DIR}/stat.sh	

clean:
	rm -fr obj.${ARCH}.${MODEL}
	rm -fr src/NetBSD
	rm -fr image.${ARCH}
	rm -fr src/etc/*

