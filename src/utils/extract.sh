#!/bin/sh -x
#
# $FML: extract.sh,v 1.5 2003/04/01 02:00:18 fukachan Exp $
#

RENAME () {
	local name=$1
	local target=$2

	if expr $name : $target
	then
		echo rename $target* to $target
		mv $target* $target || exit 1
	fi
}

tmpdir=./trash/$$

cd ${PKG_DIR} || exit 1

test -d $tmpdir || mkdir -p $tmpdir

for tgz in $*
do
	name=`basename $tgz .tar.gz`
	f=../distfiles/$tgz

	if [ -f .extract_${name}_done ];then
		echo "ignore ${name} building (.extract_${name}_done)"
		sleep 3
		continue
	fi

	echo Extracting $tgz

	checksum0=`grep $tgz ../distinfo |awk '{print $4}'`
	checksum1=`digest sha1 $f |awk '{print $4}'`
	if [ "X$checksum0" = "X$checksum1" ];then
		echo "Checksum ok"
	else
		echo "Error: wrong checksum"
		echo -n "	"; pwd
		echo "	$checksum0 $tgz"
		echo "	$checksum1 $f"
		exit 1		
	fi

	pwd
	ls -l $f
	echo tar zxf $f
	eval tar zxf $f

	for prog in squid jftpgw zebra racoon pim6sd
	do
		RENAME $name $prog

		if [ -d $prog ];then
			_prog=$prog
			echo o.k. $prog is extraced.		
			touch .extract_${name}_done
		fi
	done

	if [ -d ../patches/${_prog} ];then
		patches_dir=../patches/${_prog}

		echo Appling patches
		for p in $patches_dir/patch-*
		do
			(cd $_prog; patch < ../$p)
		done
	fi
done

exit 0
