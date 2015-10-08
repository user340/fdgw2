# Copyright (C) 2015 Yuuki Enomoto <mail@e-yuuki.org>
#
# All rights reserved.
# This program is released under the BSD License, please see LICENSE.
#
# CREDITS
#
# Ken'ichi Fukamachi <fukachan@fml.org>
# 	Original fdgw developer, and my advisor.
#

# programs and directories

TOOL_DIR?= /usr/tools
OBJ_DIR?= /usr/obj
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
		TOOL_DIR=${TOOL_DIR} \
		OBJ_DIR=${OBJ_DIR}

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
	(cd obj.${ARCH}.${MODEL};\
	   ${SU_CMD} "cd `pwd`; ${MAKE} ${MAKE_PARAMS} image" )


#
# clean up
#
clean cleandir: _clean_src _clean_fdgw_buildin_pkg _clean_pkg _clean_obj

_clean_src:
	@ echo "===> clearing src";
	- (cd src; ${MAKE} clean )

_clean_fdgw_buildin_pkg:
	@ echo "===> clearing rp-pppoe";
	- (cd ${GNU_DIR}/rp-pppoe/src ; ${GMAKE} distclean )
	- rm -f ${STATUS_DIR}/.pppoe_done
	@ echo "===> clearing stone";
	- (cd ${GNU_DIR}/stone ; ${GMAKE} distclean )
	- rm -f ${STATUS_DIR}/.stone_done
	@ echo "===> clearing transproxy";
	- (cd src/sbin/transproxy ; make clean RM="rm -f")
	- rm -f ${STATUS_DIR}/.transproxy_done
	@ echo "===> clearing pim6sd";
	- (cd src/usr.sbin/pim6sd/pim6sd ; make clean RM="rm -f")
	- rm -f ${STATUS_DIR}/.pim6sd_done

_clean_pkg:
	@ echo "===> clearing squid";
	- (cd ${PKG_DIR}/squid ; ${GMAKE} distclean )
	- rm -f ${STATUS_DIR}/.squid_done
	@ echo "===> clearing jftpgw";
	- (cd ${PKG_DIR}/jftpgw ; ${GMAKE} distclean )
	- rm -f ${STATUS_DIR}/.jftpgw_done

_clean_obj:
	@ for dir in obj.* ; do \
		if [ -d $$dir ];then\
		(\
		  	echo "===> clearing $$dir" ;\
			cd $$dir ;\
			${MAKE} clean;\
		);\
		fi;\
	  done

allclean: clean
	- rm -fr obj.* image.*
	- rm -fr build.log
	- rm -fr src/NetBSD
	- (rm -fr ${PKG_DIR} )

stat:	obj.*.*/log.*
	@ ${SH} ${UTILS_DIR}/stat.sh	

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
