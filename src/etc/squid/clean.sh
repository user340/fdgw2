#!/bin/sh
#
#  $FML: clean.sh,v 1.4 2002/04/17 07:44:39 fukachan Exp $
#

prefix=/usr/pkg
exec_prefix=${prefix}
logdir=/var/squid
PATH=${exec_prefix}/sbin:/bin:/usr/bin:/sbin:/usr/sbin
export PATH

configfile="/usr/pkg/etc/squid/squid.conf"
mode=$1

cd ${logdir}/logs || exit 1

while true
do
   df . | sed 1d | logger -t squid/logdir

   if [ -s access.log ];then

	eval /usr/pkg/sbin/squid -k rotate -f $configfile
	if [ -f access.log.0 ];then
		cat access.log.0 | logger -t squid/log
	fi
	rm -f *.log.?

	if [ "X$mode" = Xonce ];then
		exit 0;
	fi
   fi

   sleep 5
done

exit 0
