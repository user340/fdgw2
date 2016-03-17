# Copyright (C) 2015 Yuuki Enomoto <mail@e-yuuki.org>
#
# All rights reserved.
# Please read README for details.
#


ARCH?=		i386
_CONF_DIR=	conf/
_MODEL_DIR=	model/

MODEL?=		rescue

#
# directories for targets and sources
#

# destination to install
LOCALBASE?=	/usr/pkg

# NetBSD source
BSDSRCDIR?=	/usr/src

# NetBSD tools
TOOL_DIR?= /usr/tools

# dir to lndir
_BSDSRCDIR=	`pwd`/NetBSD

# utilities
_ENV= env PKG_DIR=${PKG_DIR}
_FETCH_PROGRAM=	  ${_ENV} ${SH} ${UTILS_DIR}/fetch.sh
_EXTRACT_PROGRAM= ${_ENV} ${SH} ${UTILS_DIR}/extract.sh

# individual directories to link to
_DIR1=	etc
_DIR1+=	bin/cat
_DIR1+=	bin/chio
_DIR1+=	bin/cp
_DIR1+=	bin/date
_DIR1+= bin/dd
_DIR1+= bin/df
_DIR1+= bin/domainname
_DIR1+=	bin/echo
_DIR1+=	bin/hostname
_DIR1+=	bin/kill
_DIR1+=	bin/ln
_DIR1+= bin/ls
_DIR1+=	bin/mkdir
_DIR1+=	bin/mv
_DIR1+=	bin/pax
_DIR1+= bin/pwd
_DIR1+=	bin/rm
_DIR1+=	bin/sh
_DIR1+=	bin/sleep
_DIR1+=	bin/stty
_DIR1+=	bin/test
_DIR1+= distrib/common
_DIR1+=	distrib/i386/liveimage
_DIR1+= distrib/i386/instkernel
_DIR1+= distrib/i386/ramdisks
_DIR1+=	distrib/utils
_DIR1+= lib/libc
_DIR1+= lib/libterminfo
_DIR1+= sbin/fdisk
_DIR1+= sbin/fsck
_DIR1+= sbin/fsck_ext2fs
_DIR1+= sbin/fsck_ffs
_DIR1+= sbin/fsck_lfs
_DIR1+= sbin/fsck_msdos
_DIR1+= sbin/fsdb
_DIR1+=	sbin/ifconfig
_DIR1+=	sbin/init
_DIR1+=	sbin/mount
_DIR1+=	sbin/mount_ffs
_DIR1+=	sbin/mount_kernfs
_DIR1+=	sbin/umount
_DIR1+=	sbin/ping
_DIR1+=	sbin/reboot
_DIR1+=	sbin/shutdown
_DIR1+=	sbin/sysctl
_DIR1+=	sys/netinet
_DIR1+=	usr.bin/sed
_DIR1+= usr.bin/touch
_DIR1+=	usr.bin/tty
_DIR1+= usr.bin/printf
_DIR1+= usr.sbin/pf
_DIR1+=	usr.sbin/rtadvd
_DIR1+=	usr.sbin/rtsold

_DIR2+= common/include
_DIR2+= common/lib/libc
_DIR2+= dist/pf
_DIR2+=	sys/arch/i386
_DIR2+=	sys/compat/linux
_DIR2+=	sys/conf
_DIR2+= sys/dist/pf
_DIR2+= sys/external
_DIR2+= sys/sys

_FLOPPY_DIR=	${_BSDSRCDIR}/distrib/${ARCH}/liveimage
_SYS_DIR=	${BSDSRCDIR}/sys

#
# Live-Image parameters
#
INO_BYTES=	40960

IMAGE=		boot.fs
MOUNT_POINT=	${.CURDIR}/mnt
_LISTS?=	model/${MODEL}/list
_CBIN?=		model/${MODEL}/ramdiskbin
_RC_CONF?=	model/${MODEL}/rc.conf

RAMDISK_S?=	ramdisk-small.fs
BIOSBOOT?=	bootxx_ffsv1


#
# compiler pragma / attributes / options
#
WARNS=1

ETC_DIR?=	conf/etc

_ARGS?=		MOUNT_POINT=${MOUNT_POINT} \
		TOP=${_FLOPPY_DIR} \
		_SYS_DIR=${_SYS_DIR} \
		_CONF_DIR=${_CONF_DIR} \
		BSDSRCDIR=${_BSDSRCDIR} \
		CURDIR=${PWD} _ETC_DIR=${ETC_DIR} \
		MODEL=${MODEL} ARCH=${ARCH} \
		INO_BYTES=${INO_BYTES} \
		RAMDISK_S=${RAMDISK_S} \
		BIOSBOOT=${BIOSBOOT} \
		TOOL_DIR=${TOOL_DIR}

#
# MAIN RULE
#

all: 
	@ echo "make build"
	@ echo "make install"

build: _prepare_build_system
	@ echo ""
	@ echo "build stage 1. make ramdiskbin for ${MODEL} model"
	@ echo ""
	${TOOL_DIR}/bin/nbmake-i386 -f Makefile.ramdisk ${_ARGS} build
	@ echo ""
	@ echo "build stage 2. make netbsd kernel for ${MODEL} model"
	@ echo ""
	${TOOL_DIR}/bin/nbmake-i386 -f Makefile.kernel ${_ARGS} build
	@ echo ""
	@ echo "build stage for ${MODEL} model done."
	@ echo ""	

image:
	@ echo ""
	@ echo "image 1. make ramdisk-small.fs for ${MODEL} model"
	@ echo ""
	${TOOL_DIR}/bin/nbmake-i386 -f Makefile.ramdisk ${_ARGS} image
	@ echo ""
	@ echo "image 2. inject mdsetimage into NetBSD Kernel for ${MODEL} model"
	@ echo ""
	${TOOL_DIR}/bin/nbmake-i386 -f Makefile.kernel ${_ARGS} image
	@ echo ""
	@ echo "image 3. make a bootable floppy and inject netbsd into it for ${MODEL} model"
	@ echo ""
	${TOOL_DIR}/bin/nbmake-i386 -f Makefile.bootfloppy ${_ARGS} image
	@ echo ""
	@ echo "building image for ${MODEL} model stage done."
	@ echo ""	


#
# initialization rules
#
INIT_RULES =	_init_mesg     \
		_init_dir      \
		_lndir         \
		_init_config   \
		_init_resolver

######################################################
#
# initialize process before main build process
#
_prepare_build_system:	${INIT_RULES}

_init_mesg:
	@ echo "
	@ echo "0. prepations ... "
	@ echo "

_lndir:
	pwd
	@ test -d ${_BSDSRCDIR} || mkdir -p ${_BSDSRCDIR}
	# From NetBSD source sets.
	for DIR in ${_DIR1} ${_DIR2}; do \
	  test -d ${_BSDSRCDIR}/$$DIR || mkdir -p ${_BSDSRCDIR}/$$DIR; \
	  lndir ${BSDSRCDIR}/$$DIR ${_BSDSRCDIR}/$$DIR; \
	done
	(cd ${_BSDSRCDIR}/distrib; ln -s -f ${BSDSRCDIR}/distrib/Makefile.inc)
	(cd ${_BSDSRCDIR}/sbin; ln -s -f ${BSDSRCDIR}/sbin/Makefile.inc)
	(cd ${_BSDSRCDIR}; ln -s -f ${BSDSRCDIR}/Makefile.inc)
	for DIR in ${_DIR1} ; do \
	  (cd ${_BSDSRCDIR}/$$DIR; echo cleaning in $$DIR; ${TOOL_DIR}/bin/nbmake-i386 cleandir) \
	done
	@ touch ${_BSDSRCDIR}/.lndir_done

_init_dir:
	@ test -d work || mkdir -p work
	@ test -d work/etc || mkdir -p work/etc
	@ test -d work_ramdisk || mkdir -p work_ramdisk

_init_config:
	-rm -f dot.profile mtree.conf conf model
	ln ../${_CONF_DIR}/dot.profile .
	ln ../${_CONF_DIR}/mtree.conf  .
	ln -s ../${_CONF_DIR} conf
	ln -s ../${_MODEL_DIR} model
	-rm -f ramdiskbin.conf
	${TOOL_DIR}/bin/nbsed "s/__ARCH__/${ARCH}/g" ${_CBIN}.conf |\
	sh utils/fix_ramdiskbin.conf.sh > ramdiskbin.conf.new
	mv ramdiskbin.conf.new ramdiskbin.conf
	-rm -f list
	ln -s ${_LISTS} list
	-rm -f rc.conf
	ln -s ${_RC_CONF} rc.conf

_init_resolver:
	(cd utils/resolver/; ${TOOL_DIR}/bin/nbmake-i386 ${_ARGS})

#
# cleaning
#
clean cleandir:
	-${TOOL_DIR}/bin/nbmake-i386 -f Makefile.ramdisk cleandir TOP=${_FLOPPY_DIR}
	-rm -f conf model
	-rm -f ramdiskbin.conf list rc.conf
	-rm -f boot *.fs boot.fs* netbsd* *.tmp *~
	-rm -f dot.profile mtree.conf

allclean: clean
	-rm -fr work
	-${TOOL_DIR}/bin/nbmake-i386 -f Makefile.ramdisk unconfig TOP=${_FLOPPY_DIR}
