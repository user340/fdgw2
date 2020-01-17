#!/bin/sh

_bomb()
{
    echo "$@"
    exit 1
}

_mkdir_if_not_exists()
{
    test -d "${1}" || mkdir "${1}"
}

TREE_DIR=${TREE_DIR:-img}

OBJECTS="obj.${1}"
CURRENT="$(pwd)"

for directory in "${TREE_DIR}" "${TREE_DIR}/bin" "${TREE_DIR}/sbin" "${TREE_DIR}/dev"; do
    _mkdir_if_not_exists "${directory}"
done

cp /dev/MAKEDEV "${TREE_DIR}/dev/"
cd "${TREE_DIR}/dev/" && sudo ./MAKEDEV usbs wscons ramdisk

cd "${CURRENT}" || _bomb "Couldn't cd to ${CURRENT}"

cp "${OBJECTS}/ramdisk" "${TREE_DIR}/sbin/"

commands="${TREE_DIR}/bin/cat
${TREE_DIR}/bin/chmod
${TREE_DIR}/bin/chown
${TREE_DIR}/bin/chgrp
${TREE_DIR}/bin/chroot
${TREE_DIR}/bin/cp
${TREE_DIR}/bin/df
${TREE_DIR}/bin/ed
${TREE_DIR}/bin/more
${TREE_DIR}/bin/less
${TREE_DIR}/bin/ln
${TREE_DIR}/bin/ls
${TREE_DIR}/bin/mkdir
${TREE_DIR}/bin/mv
${TREE_DIR}/bin/rm
${TREE_DIR}/bin/pwd
${TREE_DIR}/bin/sed
${TREE_DIR}/bin/sh
${TREE_DIR}/bin/stty
${TREE_DIR}/bin/test
${TREE_DIR}/bin/hostname
${TREE_DIR}/bin/kill
${TREE_DIR}/bin/echo
${TREE_DIR}/bin/date
${TREE_DIR}/bin/expr
${TREE_DIR}/sbin/ifconfig
${TREE_DIR}/sbin/init
${TREE_DIR}/sbin/mount
${TREE_DIR}/sbin/mount_ffs
${TREE_DIR}/sbin/mount_msdos
${TREE_DIR}/sbin/mount_kernfs
${TREE_DIR}/sbin/ping
${TREE_DIR}/sbin/reboot
${TREE_DIR}/sbin/halt
${TREE_DIR}/sbin/route
${TREE_DIR}/sbin/shutdown
${TREE_DIR}/sbin/sync
${TREE_DIR}/sbin/umount
${TREE_DIR}/sbin/ps
${TREE_DIR}/sbin/logger
${TREE_DIR}/sbin/id
${TREE_DIR}/sbin/basename
${TREE_DIR}/sbin/uname
${TREE_DIR}/sbin/tput
${TREE_DIR}/sbin/dhcpd"

for cmd in $commands; do
    ln "${OBJECTS}/sbin/ramdisk" "${cmd}"
done
