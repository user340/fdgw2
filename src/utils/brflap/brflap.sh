#!/bin/sh
#
# Copyright (C) 2004 Ken'ichi Fukamachi <fukachan@fml.org>
#
# All rights reserved. This program is free software; you can
# redistribute it and/or modify it under the same terms as NetBSD itself.
#
# $FML: brflap.sh,v 1.1 2004/02/06 04:42:52 fukachan Exp $
#

interface=${1-:bridge0}
interval=${2-:300}

while true
do
	sleep $interval;
	ifconfig $interface down
	logger $interface down

	sleep $interval;
	ifconfig $interface up
	logger $interface up
done

# not reach here
exit 0;
