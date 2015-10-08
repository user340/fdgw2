#!/bin/sh
#
#  $FML: master.sh,v 1.5 2002/02/22 13:28:47 fukachan Exp $
#

prefix=/usr/pkg
exec_prefix=${prefix}
logdir=/var/squid
PATH=${exec_prefix}/sbin:/bin:/usr/bin:/sbin:/usr/sbin
export PATH

configfile="/usr/pkg/etc/squid/squid.conf"
conf=""
if test "$1" ; then
	conf="-f $1"
	shift
else
	conf="-f $configfile"
fi


mkdir -p ${logdir}
mkdir -p ${logdir}/cache
mkdir -p ${logdir}/logs
mkdir -p ${logdir}/errors
cp /dev/null ${logdir}/logs/access.log
cp /dev/null ${logdir}/logs/cache.log
cp /dev/null ${logdir}/logs/store.log
chown -R nobody ${logdir}

logger -t squid install /usr/pkg/etc/squid/mime.conf
cp /etc/squid/mime.conf /usr/pkg/etc/squid/mime.conf

( sleep 15; logger -t squid start squidclean; /usr/pkg/sbin/squidclean &)&

failcount=0
while : ; do
	echo "Running: squid -Y $conf >> $logdir/squid.out 2>&1"
	echo "Startup: `date`" | logger -t squid

	# recreate cache area
	rm -fr ${logdir}/cache
	mkdir -p ${logdir}/cache
	chown -R nobody ${logdir}

	logger -t squid install $configfile
	cp /etc/squid/squid.conf $configfile

	squid -z $conf
	squid -NsY $conf | logger -t squid
	/usr/pkg/sbin/squidclean once
	sleep 10
done

logger -t squid squidmater dies

exit 0
