# Copyright (C) 2015 Yuuki Enomoto <mail@e-yuuki.org>
#
# All rights reserved.
# This program is released under the BSD License as with NetBSD.
# For details, please see ...
#
# https://github.com/user340/fdgw2/wiki
# or
# http://e-yuuki.org/develop/fdgw2.html
#
# CREDITS
#
# Ken'ichi Fukamachi <fukachan@fml.org>
#     Original fdgw developer, and my advisor.

# programs and directories

TOOL_DIR?= /vol/usr/tools
GMAKE?=		${TOOL_DIR}/bin/nbgmake
SH?=		/bin/sh

PKG_DIR?=	src/pkg
GNU_DIR?=	src/gnu
PDS_DIR?=	src/pds
UTILS_DIR?=	src/utils
STATUS_DIR?=	${PKG_DIR}

#
# specify default image
#
ARCH?=		i386
MODEL?=         standard
KERNEL_CONF?=	FDGW
BIOSBOOT?=	bootxx_ffsv1

# root privilege control
SU_CMD?=	su - root -c

MAKE_PARAMS = 	MODEL=${MODEL} \
		KERNEL_CONF=${KERNEL_CONF} \
		BIOSBOOT=${BIOSBOOT} \
		OPSYS=NetBSD \
		SH=${SH}	\
		GNU_DIR=${GNU_DIR:S|^src/||} \
		PDS_DIR=${PDS_DIR:S|^src/||} \
		PKG_DIR=${PKG_DIR:S|^src/||} \
		STATUS_DIR=${STATUS_DIR:S|^src/||} \
		UTILS_DIR=${UTILS_DIR:S|^src/||} \
		TOOL_DIR=${TOOL_DIR}

all:
	@ echo "make build   (need NOT priviledge)"
	@ echo "make image   (need root priviledge)"

# "dist*" is compiled by fdgw source itself and NetBSD source.
# "allmodels*" is dependenent on external packages.
dist: dist-build dist-image

allmodels: allmodels-build allmodels-image

dist-build:
	${MAKE} MODEL=standard     KERNEL_CONF=FDGW6  build

dist-image:
	${MAKE} MODEL=standard       KERNEL_CONF=FDGW6  image

allmodels-build:
	${MAKE} MODEL=standard     KERNEL_CONF=FDGW6 build

allmodels-image:
	${MAKE} MODEL=standard     KERNEL_CONF=FDGW6 image

build:
	${SH} ./${UTILS_DIR}/prepare_workdir.sh ${ARCH}.${MODEL}
	(cd obj.${ARCH}.${MODEL};${MAKE} ${MAKE_PARAMS} build )

image:
	@ echo ""	
	@ echo "\"make image\" needs root privilege"
	@ echo ""	
	(cd obj.${ARCH}.${MODEL};\
	   ${SU_CMD} "cd `pwd`; ${MAKE} ${MAKE_PARAMS} image" )

stat:	obj.*.*/log.*
	@ ${SH} ${UTILS_DIR}/stat.sh	

clean:
	rm -fr obj.${ARCH}.${MODEL}
	rm -fr src/NetBSD
	rm -fr image.${ARCH}

#
# utilities for debug
#
mount-ramdisk:
	if [ -f obj.${ARCH}.${MODEL}/ramdisk-small.fs ];then \
	  vnconfig -v -c /dev/vnd0d obj.${ARCH}.${MODEL}/ramdisk-small.fs;\
	  mount /dev/vnd0a /mnt;\
	fi

mount-img:
	vnconfig -v -c /dev/vnd0d image.${ARCH}/${MODEL}.img
	mount /dev/vnd0a /mnt

umount-img: umount
umount-ramdisk: umount
umount:
	umount /mnt
	vnconfig -u /dev/vnd0d
