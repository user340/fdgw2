#!/bin/sh

TREE_DIR?=		img

OBJECTS=		obj.$1
CURRENT=		`pwd`

mkdir ${TREE_DIR}
mkdir ${TREE_DIR}/bin
mkdir ${TREE_DIR}/sbin
mkdir ${TREE_DIR}/dev

cp /dev/MAKEDEV ${TREE_DIR}/dev/
cd ${TREE_DIR}/dev/
sudo ./MAKEDEV usbs wscons ramdisk
cd ${CURRENT}

cp ${OBJECTS}/ramdisk ${TREE_DIR}/sbin/

ln ${OBJECTS}/sbin/ramdisk ${TREE_DIR}/bin/cat
ln ${OBJECTS}/sbin/ramdisk ${TREE_DIR}/bin/chmod
ln ${OBJECTS}/sbin/ramdisk ${TREE_DIR}/bin/chown
ln ${OBJECTS}/sbin/ramdisk ${TREE_DIR}/bin/chgrp
ln ${OBJECTS}/sbin/ramdisk ${TREE_DIR}/bin/chroot
ln ${OBJECTS}/sbin/ramdisk ${TREE_DIR}/bin/cp
ln ${OBJECTS}/sbin/ramdisk ${TREE_DIR}/bin/df
ln ${OBJECTS}/sbin/ramdisk ${TREE_DIR}/bin/ed
ln ${OBJECTS}/sbin/ramdisk ${TREE_DIR}/bin/more
ln ${OBJECTS}/sbin/ramdisk ${TREE_DIR}/bin/less
ln ${OBJECTS}/sbin/ramdisk ${TREE_DIR}/bin/ln
ln ${OBJECTS}/sbin/ramdisk ${TREE_DIR}/bin/ls
ln ${OBJECTS}/sbin/ramdisk ${TREE_DIR}/bin/mkdir
ln ${OBJECTS}/sbin/ramdisk ${TREE_DIR}/bin/mv
ln ${OBJECTS}/sbin/ramdisk ${TREE_DIR}/bin/rm
ln ${OBJECTS}/sbin/ramdisk ${TREE_DIR}/bin/pwd
ln ${OBJECTS}/sbin/ramdisk ${TREE_DIR}/bin/sed
ln ${OBJECTS}/sbin/ramdisk ${TREE_DIR}/bin/sh
ln ${OBJECTS}/sbin/ramdisk ${TREE_DIR}/bin/stty
ln ${OBJECTS}/sbin/ramdisk ${TREE_DIR}/bin/test
ln ${OBJECTS}/sbin/ramdisk ${TREE_DIR}/bin/hostname
ln ${OBJECTS}/sbin/ramdisk ${TREE_DIR}/bin/kill
ln ${OBJECTS}/sbin/ramdisk ${TREE_DIR}/bin/echo
ln ${OBJECTS}/sbin/ramdisk ${TREE_DIR}/bin/date
ln ${OBJECTS}/sbin/ramdisk ${TREE_DIR}/bin/expr
ln ${OBJECTS}/sbin/ramdisk ${TREE_DIR}/sbin/ifconfig
ln ${OBJECTS}/sbin/ramdisk ${TREE_DIR}/sbin/init
ln ${OBJECTS}/sbin/ramdisk ${TREE_DIR}/sbin/mount
ln ${OBJECTS}/sbin/ramdisk ${TREE_DIR}/sbin/mount_ffs
ln ${OBJECTS}/sbin/ramdisk ${TREE_DIR}/sbin/mount_msdos
ln ${OBJECTS}/sbin/ramdisk ${TREE_DIR}/sbin/mount_kernfs
ln ${OBJECTS}/sbin/ramdisk ${TREE_DIR}/sbin/ping
ln ${OBJECTS}/sbin/ramdisk ${TREE_DIR}/sbin/reboot
ln ${OBJECTS}/sbin/ramdisk ${TREE_DIR}/sbin/halt
ln ${OBJECTS}/sbin/ramdisk ${TREE_DIR}/sbin/route
ln ${OBJECTS}/sbin/ramdisk ${TREE_DIR}/sbin/shutdown
ln ${OBJECTS}/sbin/ramdisk ${TREE_DIR}/sbin/sync
ln ${OBJECTS}/sbin/ramdisk ${TREE_DIR}/sbin/umount
ln ${OBJECTS}/sbin/ramdisk ${TREE_DIR}/sbin/ps
ln ${OBJECTS}/sbin/ramdisk ${TREE_DIR}/sbin/logger
ln ${OBJECTS}/sbin/ramdisk ${TREE_DIR}/sbin/id
ln ${OBJECTS}/sbin/ramdisk ${TREE_DIR}/sbin/basename
ln ${OBJECTS}/sbin/ramdisk ${TREE_DIR}/sbin/uname
ln ${OBJECTS}/sbin/ramdisk ${TREE_DIR}/sbin/tput
ln ${OBJECTS}/sbin/ramdisk ${TREE_DIR}/sbin/dhcpd
