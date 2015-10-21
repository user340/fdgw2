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


ARCH?=		i386
_CONF_DIR=	conf/
_MODEL_DIR=	model/

MODEL?=         standard

#
# directories for targets and sources
#

# destination to install
LOCALBASE?=	/usr/pkg

# NetBSD source
BSDSRCDIR?=	/vol/usr/src

# NetBSD tools
TOOL_DIR?= /vol/usr/tools
OBJ_DIR?= /vol/usr/obj

# dir to lndir
_BSDSRCDIR=	`pwd`/NetBSD

# utilities
_ENV= env PKG_DIR=${PKG_DIR}
_FETCH_PROGRAM=	  ${_ENV} ${SH} ${UTILS_DIR}/fetch.sh
_EXTRACT_PROGRAM= ${_ENV} ${SH} ${UTILS_DIR}/extract.sh

# individual directories to link to
_DIR1=	etc
_DIR1+=	bin/cat
_DIR1+= bin/chmod
_DIR1+=	bin/cp
_DIR1+=	bin/date
_DIR1+=	bin/df
_DIR1+=	bin/echo
_DIR1+=	bin/ed
_DIR1+=	bin/expr
_DIR1+=	bin/hostname
_DIR1+=	bin/kill
_DIR1+=	bin/ln
_DIR1+=	bin/ls
_DIR1+=	bin/mkdir
_DIR1+=	bin/mv
_DIR1+=	bin/ps
_DIR1+=	bin/pwd
_DIR1+=	bin/rm
_DIR1+=	bin/sh
_DIR1+=	bin/sleep
_DIR1+=	bin/stty
_DIR1+=	bin/sync
_DIR1+=	bin/test
_DIR1+=	distrib/utils
_DIR1+= distrib/common
_DIR1+=	distrib/i386/liveimage
_DIR1+= distrib/i386/instkernel
_DIR1+= distrib/i386/ramdisks
_DIR1+=	sbin/ifconfig
_DIR1+=	sbin/init
_DIR1+=	sbin/mount
_DIR1+=	sbin/mount_ffs
_DIR1+=	sbin/mount_kernfs
_DIR1+=	sbin/mount_msdos
_DIR1+=	sbin/umount
_DIR1+=	sbin/ping
_DIR1+=	sbin/ping6
_DIR1+=	sbin/route
_DIR1+=	sbin/routed
_DIR1+=	sbin/reboot
_DIR1+= sbin/chown
_DIR1+=	sbin/shutdown
_DIR1+=	sbin/sysctl
_DIR1+=	usr.bin/basename
_DIR1+=	usr.bin/id
_DIR1+=	usr.bin/logger
_DIR1+=	usr.bin/sed
_DIR1+= usr.bin/touch
_DIR1+=	usr.bin/tput
_DIR1+=	usr.bin/tty
_DIR1+=	usr.bin/uname
_DIR1+= usr.bin/printf
_DIR1+=	usr.sbin/chroot
_DIR1+=	usr.sbin/dhcp
_DIR1+=	usr.sbin/pppd
_DIR1+=	usr.sbin/route6d
_DIR1+=	usr.sbin/rtadvd
_DIR1+=	usr.sbin/rtsold
#_DIR1+=	usr.sbin/syslogd # XXX
_DIR1+=	usr.sbin/traceroute6
_DIR1+= lib/libc
_DIR1+= lib/libterminfo
_DIR1+=	sys/netinet
_DIR1+= external/bsd/libpcap
_DIR1+= usr.sbin/pf
_DIR1+= usr.sbin/npf

.if exists (${BSDSRCDIR}/dist/dhcp/Makefile)
_DIR2+=	dist/dhcp
.endif
_DIR2+=	sys/conf
_DIR2+=	sys/arch/i386/stand
_DIR2+= common/lib/libc
_DIR2+= common/include

#.if ${NetBSD_VERSION} == 1.6
# sysctl needs sys/compat/linux
_DIR2+=	sys/compat/linux
#.endif


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

# conf/etc not conf/1.5/etc in obj.*
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
		TOOL_DIR=${TOOL_DIR} \
		OBJ_DIR=${OBJ_DIR}

# import from pkgsrc/www/squid/Makefile
SQUID_CONFIGURE_ARGS?=  --enable-ipf-transparent \
                        --enable-underscores


#
# MAIN RULE
#

all: 
	@ echo "make build   (need NOT priviledge)"
	@ echo "make install (need root priviledge)"

build: _prepare_build_system
	@ echo ""
	@ echo "build stage 1. make ramdiskbin for ${MODEL} model"
	@ echo ""
	${TOOL_DIR}/bin/nbmake -f Makefile.ramdisk ${_ARGS} build
	@ echo ""
	@ echo "build stage 2. make netbsd kernel for ${MODEL} model"
	@ echo ""
	${TOOL_DIR}/bin/nbmake -f Makefile.kernel ${_ARGS} build
	@ echo ""
	@ echo "build stage for ${MODEL} model done."
	@ echo ""	

image:
	@ echo ""
	@ echo "image 1. make ramdisk-small.fs for ${MODEL} model"
	@ echo ""
	${TOOL_DIR}/bin/nbmake -f Makefile.ramdisk ${_ARGS} image ||\
		(make -f Makefile.ramdisk ${_ARGS} unconfig ; exit 1)
	@ echo ""
	@ echo "image 2. inject mdsetimage into NetBSD Kernel for ${MODEL} model"
	@ echo ""
	${TOOL_DIR}/bin/nbmake -f Makefile.kernel ${_ARGS} image
	@ echo ""
	@ echo "image 3. make a bootable floppy and inject netbsd into it for ${MODEL} model"
	@ echo ""
	${TOOL_DIR}/bin/nbmake -f Makefile.bootfloppy ${_ARGS} image ||\
		(make -f Makefile.bootfloppy ${_ARGS} unconfig; exit 1)
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
		_init_model_pre_configure \
		_init_dhcp     \
		_init_resolver

INIT_RULES +=	_init_model_post_configure

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
	@ test -d ${_BSDSRCDIR} || mkdir -p ${_BSDSRCDIR}
	for DIR in ${_DIR1} ${_DIR2}; do \
	  test -d ${_BSDSRCDIR}/$$DIR || mkdir -p ${_BSDSRCDIR}/$$DIR; \
	  lndir ${BSDSRCDIR}/$$DIR ${_BSDSRCDIR}/$$DIR; \
	done
	#test -d ${_BSDSRCDIR}/usr.sbin/dhcp || mkdir -p ${_BSDSRCDIR}/usr.sbin/dhcp
	#lndir /root/fdgw/src/usr.sbin/dhcp ${_BSDSRCDIR}/usr.sbin/dhcp
	#test -d ${_BSDSRCDIR}/usr.sbin/bind || mkdir -p ${_BSDSRCDIR}/usr.sbin/bind
	#lndir /root/fdgw/src/usr.sbin/bind ${_BSDSRCDIR}/usr.sbin/bind
	(cd ${_BSDSRCDIR}/distrib; ln -s -f ${BSDSRCDIR}/distrib/Makefile.inc)
	(cd ${_BSDSRCDIR}/sbin; ln -s -f ${BSDSRCDIR}/sbin/Makefile.inc)
	(cd ${_BSDSRCDIR}; ln -s -f ${BSDSRCDIR}/Makefile.inc)
	for DIR in ${_DIR1} ; do \
	  (cd ${_BSDSRCDIR}/$$DIR; echo cleaning in $$DIR; make cleandir) \
	done
	@ touch ${_BSDSRCDIR}/.lndir_done

_init_dir:
	@ test -d work || mkdir -p work
	@ test -d work/etc || mkdir -p work/etc

_init_config:
	-rm -f dot.profile mtree.conf conf model
	ln ../${_CONF_DIR}/dot.profile .
	ln ../${_CONF_DIR}/mtree.conf  .
	#-rm -f disktab.preinstall termcap.mini
	ln -s ../${_CONF_DIR} conf
	ln -s ../${_MODEL_DIR} model
	-rm -f ramdiskbin.conf
	sed "s/__ARCH__/${ARCH}/g" ${_CBIN}.conf |\
	sh utils/fix_ramdiskbin.conf.sh > ramdiskbin.conf.new
	mv ramdiskbin.conf.new ramdiskbin.conf
	-rm -f list
	ln -s ${_LISTS} list
	-rm -f rc.conf
	ln -s ${_RC_CONF} rc.conf

_init_model_pre_configure:
	if [ -x model/${MODEL}/pre_configure ]; then \
		model/${MODEL}/pre_configure ;\
	fi

_init_model_post_configure:
	if [ -x model/${MODEL}/post_configure ]; then \
		model/${MODEL}/post_configure ;\
	fi

_init_pppoe:
	@ echo "";
	@ echo "make rp-pppoe for ADSL router";
	@ echo "";
	@ if [ ! -f ${STATUS_DIR}/.pppoe_done ];then \
	  ( cd ${GNU_DIR}/rp-pppoe/src;\
		./configure --prefix=${LOCALBASE} --sysconf=/etc/ppp ;\
		make ;\
	  );\
	  fi
	@ touch ${STATUS_DIR}/.pppoe_done

_init_squid:
	@ echo "";
	@ echo "make squid for proxyfw model";
	@ echo "";
	@ ${_FETCH_PROGRAM} ${_SQUID_SOURCE}
	@ ${_EXTRACT_PROGRAM} ${_SQUID_SOURCE}
	@ if [ ! -f ${STATUS_DIR}/.squid_done ];then \
	  ( cd ${PKG_DIR}/squid;\
		./configure --prefix=${LOCALBASE} \
		                --localstatedir=/var/squid \
		                ${SQUID_CONFIGURE_ARGS} ;\
		make ;\
	  );\
	  fi
	@ touch ${STATUS_DIR}/.squid_done

_init_jftpgw:
	@ echo "";
	@ echo "make jftpgw for proxyfw model";
	@ echo "";
	@ ${_FETCH_PROGRAM} ${_JFTPGW_SOURCE}
	@ ${_EXTRACT_PROGRAM} ${_JFTPGW_SOURCE}
	@ if [ ! -f ${STATUS_DIR}/.jftpgw_done ];then \
	  ( cd ${PKG_DIR}/jftpgw/support;\
		rm -f ipfilter.? ;\
		ln -s ../../../lib/libtransparent/ipfilter.? . ;\
	  );\
	  ( cd ${PKG_DIR}/jftpgw;\
		./configure --prefix=${LOCALBASE} --sysconf=/etc/jftpgw \
			--disable-libwrap ;\
		make ;\
	  );\
	  fi
	@ touch ${STATUS_DIR}/.jftpgw_done

_init_zebra:
	@ echo "";
	@ echo "make zebra for proxyfw model";
	@ echo "";
	@ ${_FETCH_PROGRAM} ${_ZEBRA_SOURCE}
	@ ${_EXTRACT_PROGRAM} ${_ZEBRA_SOURCE}
	@ if [ ! -f ${STATUS_DIR}/.zebra_done ];then \
	  ( cd ${PKG_DIR}/zebra;\
		./configure --prefix=${LOCALBASE} --sysconf=/etc/zebra ;\
		make ;\
	  );\
	  fi
	@ touch ${STATUS_DIR}/.zebra_done

_init_transproxy:
	@ echo "";
	@ echo "make transproxy for proxyfw model";
	@ echo "";
	@ if [ ! -f sbin/.transproxy_done ];then \
	  ( cd sbin/transproxy;\
		make ;\
	  );\
	  fi
	@ touch sbin/.transproxy_done

_init_stone:
	@ echo "";
	@ echo "make stone for proxyfw model";
	@ echo "";
	@ if [ ! -f ${STATUS_DIR}/.stone_done ];then \
	  ( cd ${GNU_DIR}/stone;\
		rm -f ipfilter.? ;\
		ln -s ../../lib/libtransparent/ipfilter.? . ;\
		cc  -DUSE_IPF -c -o stone.o stone.c ;\
		cc  -DUSE_IPF -c -o ipfilter.o ipfilter.c ;\
	  );\
	  fi
	@ touch ${STATUS_DIR}/.stone_done

_init_dhcp:
	(cd ${_BSDSRCDIR}/usr.sbin/dhcp;make)
	#:

_init_resolver:
	(cd utils/resolver/; make ${_ARGS})

_init_pim6sd:
	@ echo "";
	@ echo "make pim6sd for pim6sd model";
	@ echo "";
	@ if [ ! -f ${STATUS_DIR}/.pim6sd_done ];then \
		(cd usr.sbin/pim6sd/pim6sd/; make );\
	fi
	@ touch ${STATUS_DIR}/.pim6sd_done

_init_racoon:
	@ echo "";
	@ echo "make racoon for vpn-racoon model";
	@ echo "";
	@ ${_FETCH_PROGRAM} ${_RACOON_SOURCE}
	@ ${_EXTRACT_PROGRAM} ${_RACOON_SOURCE}
	@ if [ ! -f ${STATUS_DIR}/.racoon_done ];then \
	  ( cd ${PKG_DIR}/racoon/libipsec;\
		make OPSYS=NetBSD ;\
	  );\
	  ( cd ${PKG_DIR}/racoon/racoon;\
		./configure --prefix=${LOCALBASE} --sysconf=/etc/racoon ;\
		make ;\
	  );\
	  fi
	@ touch ${STATUS_DIR}/.racoon_done

_init_ttcp:
	@ echo "";
	@ echo "make ttcp for gentraffic model";
	@ echo "";
	@ if [ ! -f ${STATUS_DIR}/.ttcp_done ];then \
		(cd ${PDS_DIR}/ttcp; make );\
	fi
	@ touch ${STATUS_DIR}/.ttcp_done


#_prepare_fixed_source: _init_x_syslogd

 #_init_x_syslogd:
	#@ echo ""
	#@ echo "import and modify syslogd for our use"
	#@ echo "   disable INET6 and use of libwrap"
	#@ echo ""
	#test -d x_syslogd || mkdir x_syslogd
	#cp -pr ${BSDSRCDIR}/usr.sbin/syslogd/* x_syslogd
	#sed 	-e '/^CPPFLAGS/d'      \
		#-e '/DPADD.*LIBWRAP/d' \
		#-e '/LDADD.*-lwrap/d'  \
		#x_syslogd/Makefile > x_syslogd/Makefile.bak
	#mv x_syslogd/Makefile.bak x_syslogd/Makefile
	#(cd x_syslogd; make )

#
# cleaning
#
clean cleandir:
	-make -f Makefile.ramdisk cleandir TOP=${_FLOPPY_DIR}
	-rm -f conf model
	-rm -f ramdiskbin.conf list rc.conf
	-rm -f boot *.fs boot.fs* netbsd* *.tmp *~
	-rm -f dot.profile mtree.conf

allclean: clean
	-rm -fr work
	-make -f Makefile.ramdisk unconfig TOP=${_FLOPPY_DIR}
