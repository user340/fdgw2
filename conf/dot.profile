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

SHELL=/bin/sh
export SHELL
PATH=/sbin:/bin:/usr/bin:/usr/sbin:/:/usr/pkg/sbin:/usr/pkg/bin
export PATH
TERM=wsvt25
export TERM
HOME=/
export HOME
BLOCKSIZE=1k
export BLOCKSIZE
EDITOR=ed
export EDITOR
BOOTMODEL=small
export BOOTMODEL

umask 022

ROOTDEV=/dev/md0a

if [ "X${DONEPROFILE}" = "X" ]; then
	DONEPROFILE=YES
	export DONEPROFILE

	# set up some sane defaults
	echo 'erase ^?, werase ^W, kill ^U, intr ^C'
	stty newcrt werase ^W intr ^C kill ^U erase ^? 9600
	echo ''

	# mount the ramdisk read write
	mount -u $ROOTDEV /

	# mount the kern_fs so that we can examine the dmesg state
	mount -t kernfs /kern /kern

	# pull in the functions that people will use from the shell prompt.
	dmesg() cat /kern/msgbuf
	grep() sed -n "/$1/p"

	# read configuration from /dev/sd0a (ffs)
	test -d conf || mkdir conf 
	mount /dev/sd0a /conf

	# symlink(2) for /etc
	mv /etc /etc.orig
	cp -pr /conf/etc /etc

	# warning
	touch /etc/00_NOT_EDIT_HERE
	echo "Please edit /conf/etc not here since /etc is a copy of /conf/etc." \
		> /etc/00_CAUTION

	# XXX not umount since ipnat needs /netbsd for kmem reading ;)
	# umount /conf

	# pwd.db for ps et.al.
	ln /etc.orig/pwd.db /etc/pwd.db

	rm -fr /etc.orig

        if [ -f /etc/rc.router ]
        then
                sh /etc/rc.router
        else
                echo "*** welcome to fdgw2 (one floppy NetBSD) ***"
                echo "error: no /etc (/dev/sd0a)";
                echo "       no configuration!";
		sh
        fi
fi
