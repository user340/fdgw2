#!/bin/sh
#
# $FML: stat.sh,v 1.2 2002/06/20 09:22:03 fukachan Exp $
#

echo "[ramdisk image]";
for model in model/[a-z]*
do
   model=`basename $model`
   if [ -f obj.i386.$model/log.df.ramdisk ];then
	printf "%10s  " $model;
	grep -v iused obj.i386.$model/log.df.ramdisk |\
	sed -e 's@/dev/vnd0a *@@' -e 's@ /.*$@@' 
   fi
done


echo "";
echo "[floppy image]";
for model in model/[a-z]*
do
   model=`basename $model`
   if [ -f obj.i386.$model/log.df.floppy ];then
	printf "%10s  " $model;
	grep -v iused obj.i386.$model/log.df.floppy |\
	sed -e 's@/dev/vnd0a *@@' -e 's@ /.*$@@' 
   fi
done

exit 0
