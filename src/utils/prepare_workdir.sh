#!/bin/sh
#
# $FML: prepare_workdir.sh,v 1.3 2003/01/17 07:07:26 fukachan Exp $
#

# work dir for building package.
test -d src/pkg || mkdir -p src/pkg

# alloc tar.gz area to fetch
test -d src/distfiles || mkdir -p src/distfiles

# alloc netbsd source area, which is shared among obj.$model
test -d src/NetBSD || mkdir -p src/NetBSD

# symlink(2)
for model in $*
do
	test -d obj.$model || mkdir obj.$model

	cd obj.$model || exit 1

	for x in ../src/[MN]* ../src/[a-z]*
	do
	   if [ -f $x -o -d $x ];then
		if [ ! -h `basename $x` ];then
		   ln -s $x .
		fi
	   fi
	done
done

exit 0;
