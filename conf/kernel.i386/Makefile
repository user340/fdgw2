# Copyright (C) 2015 Yuuki Enomoto <mail@e-yuuki.org>
#
# All rights reserved.
# This program is released under the BSD License as with NetBSD.
# For details, please see ...
#
# https://github.com/user340/fdgw2/wiki
# or
# http://e-yuuki.org/develop/fdgw2.html
#
# CREDITS
#
# Ken'ichi Fukamachi <fukachan@fml.org>
#     Original fdgw developer, and my advisor.


all: FDGW

FDGW:	convert.pl /sys/arch/i386/conf/INSTALL_LAPTOP
	perl convert.pl /sys/arch/i386/conf/INSTALL_LAPTOP |\
	sed 's/__FML__/\$$FML\$$/' > FDGW.new
	mv FDGW.new FDGW
