#!/bin/sh
#
# $FML: uni.sh,v 1.2 2002/04/17 03:53:20 fukachan Exp $
#

wrapper="echo"

tmp=/tmp/uni.$$

trap "eval $wrapper umount /conf; rm -f $tmp" 0 1 3 15


######################################################################
#
# LIBRARIES
#

cleanup () {
	rm -f $tmp
}

# clean up screen for the next menu
reset_screen () {
	tput clear
}


wait_for_valid_input () {
	local name=$1
	local value=''

	ar0=''

	while true
	do
		echo -n "new ${name}: "; read value

		if [ "X$value" != X ];then
			break;
		fi
	done

	ar0=$value
}



######################################################################
#
# TOP
#
menu_top () {
	select='0-6'
	cmd_0="exit 0"
	cmd_1="menu_fundamental"
	cmd_2="menu_network"
	cmd_3=""
	cmd_4="menu_reboot"
	cmd_5="menu_shell"
	cmd_6="menu_editor"

	cat  <<_EOF_

	Welcome to Uni configuration interface.

	0. end
	1. fundamental (hostname et.al.)
	2. network (interface addresses et.al.)
	3. customize daemons/services
	4. reboot
	5. shell
	6. editor
_EOF_
}


######################################################################
#
# FUNDAMENTAL
#

menu_fundamental () {
	select='012'
	cmd_0="menu_top"
	cmd_1="menu_fundamental_hostname"
	cmd_2=""

	local hostname=`hostname`

	cat <<_EOF_

	Fundamental Configurations

	hostname: $hostname

	0. end
	1. hostname
_EOF_
}


menu_fundamental_hostname () {
	wait_for_valid_input hostname
	local hostname=$ar0

	eval $wrapper hostname $hostname
	echo $hostname > $tmp
	eval $wrapper mv $tmp /etc/myname

	reset_screen
	menu_fundamental
}


######################################################################
#
# NETWORK
#

menu_network () {
	select='0123'
	cmd_0="menu_top"
	cmd_1="menu_network_ifconfig"
	cmd_2="menu_network_routing"
	cmd_3="menu_network_dns"

	local routes="`route show | sed -n -e /^Destination/d -e /^::/d -e /^2002:/d -e /^fec0/d -e /^fe80/d -e /G/p `"

	local iflist=`ifconfig -l | sed 's/lo0.*//' `
	local ifdesc=''
	for if in $iflist
	do
		x=`ifconfig -m $if | sed -n /inet/p`
		ifdesc="${ifdesc}${if}:$x"
	done

	cat  <<_EOF_

	Network Configurations

[interfaces]
$ifdesc

[routes]
$routes

	0. end
	1. ifconfig (/etc/ifconfig.xxx)
	2. routing
	3. DNS      (/etc/resolv.conf)
_EOF_
}


menu_network_dns () {
	wait_for_valid_input domain
	local domain=$ar0

	wait_for_valid_input nameserver
	local nameserver=$ar0

	echo domain $domain		>> $tmp
	echo nameserver $nameserver	>> $tmp 

	cat $tmp
	eval $wrapper mv $tmp /etc/resolv.conf
}


######################################################################
#
# UTILITIES
#

menu_reboot () {
	sync;sync;sync;reboot
}


menu_shell () {
	reset_screen
	sh
	reset_screen
	menu_top
}


menu_editor () {
	reset_screen
	${EDITOR:-ed}
	reset_screen
	menu_top
}



######################################################################
menu_top

cmd_q="exit 0"

eval $wrapper mount /dev/fd0a /conf

while true
do
	# input
	echo ""; echo -n "Select [${select}q] "; read n

	# evaluate input and run the specified function.
	eval hook=\$cmd_${n}
	eval 'if [ "X$hook" = X ] ; then continue; fi'

	reset_screen
	eval $hook
	cleanup
done

exit 0
