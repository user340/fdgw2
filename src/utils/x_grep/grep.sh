#!/bin/sh
#
# Copyright (C) 2001 Ken'ichi Fukamachi <fukachan@fml.org>
#
# All rights reserved. This program is free software; you can
# redistribute it and/or modify it under the same terms as NetBSD itself.
#
# $FML: grep.sh,v 1.2 2001/09/29 08:15:42 fukachan Exp $
#

pat=$1
shift

sed -n "/$pat/p" $*
