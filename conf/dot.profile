export PATH=/sbin:/bin:/usr/bin:/usr/sbin:/
export TERM=wsvt25
export HOME=/
export BLOCKSIZE=1k
export BOOTMODEL=small
umask 022
ROOTDEV=/dev/md0a
if [ "X${DONEPROFILE}" = "X" ]; then
	DONEPROFILE=YES
	export DONEPROFILE
	echo 'erase ^?, werase ^W, kill ^U, intr ^C'
	stty newcrt werase ^W intr ^C kill ^U erase ^? 9600
	echo ''
	mount -u $ROOTDEV /
	mount -t kernfs /kern /kern
	dmesg() cat /kern/msgbuf
	grep() sed -n "/$1/p"
	test -d conf || mkdir conf 
	mount /dev/sd0a /conf
	mv /etc /etc.orig
	cp -pr /conf/etc /etc
	touch /etc/00_NOT_EDIT_HERE
	echo "Please edit /conf/etc not here since /etc is a copy of /conf/etc." \
		> /etc/00_CAUTION
	ln /etc.orig/pwd.db /etc/pwd.db
	rm -fr /etc.orig
	if [ -f /etc/rc.router ]
	then
		sh /etc/rc.router
	else
		echo "*** welcome to fdgw2 ***"
		echo "error: no /etc (/dev/sd0a)";
		echo "       no configuration!";
		sh
	fi
fi
