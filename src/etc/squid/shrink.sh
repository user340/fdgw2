#!/bin/sh
#
# $FML: shrink.sh,v 1.2 2002/02/18 23:22:35 fukachan Exp $
#

perl convert.pl squid.conf.example |\
perl -nle 'print unless /EXAMPLE:|Note:|Default:|^\#\s{3,}|^\#\t|^\#\s*$/' |\
uniq > squid.conf
