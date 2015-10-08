#!/bin/sh
#
# $FML: fetch.sh,v 1.3 2003/01/17 09:23:26 fukachan Exp $
#

workdir=`dirname $0`/..
cd $workdir || exit 1

test -d distfiles || mkdir -p distfiles
cd distfiles || exit 1

for file in $*
do
   if [ ! -f $file ];then
	for site in	\
		ftp://ftp.netbsd.org/pub/NetBSD/packages/distfiles/ \
		ftp://ftp.iij.ad.jp/pub/NetBSD/packages/distfiles/ \
		ftp://ftp.freebsd.org/pub/FreeBSD/ports/distfiles/ \
		ftp://ftp.fml.org/pub/fdgw/source/
	do
		if [ ! -f $file ];then
			echo ftp ${site}${file}
			eval ftp ${site}${file}
		fi
	done
   fi
done

exit 0
