#!/usr/bin/env perl
#-*- perl -*-
#
# Copyright (C) 2004 Ken'ichi Fukamachi <fukachan@fml.org>
#
# All rights reserved. This program is free software; you can
# redistribute it and/or modify it under the same terms as NetBSD itself.
#
# $FML: convert.pl,v 1.5 2004/02/09 12:25:27 fukachan Exp $
#

use strict;
use Carp;
use FileHandle;

my $source = shift || "/sys/arch/i386/conf/INSTALL_LAPTOP";
my $regexp = "\n";

load_config_disable();
load_config_enable();

my $fh = new FileHandle $source;
if (defined $fh) {
    print <<'_EOF_';
#
# Copyright (C) 2004 Ken'ichi Fukamachi <fukachan@fml.org>
#
# All rights reserved. This program is free software; you can
# redistribute it and/or modify it under the same terms as NetBSD itself.
#
# __FML__
#
# This file is derived from INSTALL_LAPTOP in NetBSD 1.6 stable branch.
#
_EOF_

    print "\n\n";

    my $s = qq{
	while (<\$fh>) {
	    $regexp;
	    print;
	}
    };

    eval $s;
    croak($@) if $@;
}
else {
    croak("cannot open $source");
}

exit 0;


sub load_config_enable
{
    $regexp .= "\n\t# enable\n";

    my $fh = new FileHandle "config.enable";
    if (defined $fh) {
	my @a;
	my $_regexp;
	while (<$fh>) {
	    chomp;
	    if ($_) {
		$_regexp = '';
		for my $e (split(/\s+/, $_)) {
		    $_regexp .= ".*" . quotemeta($e);
		}
		$regexp .= "\ts/^\\#// if /^\\\#$_regexp/;\n";
	    }
	}
	$fh->close();
    }
}


sub load_config_disable
{
    $regexp .= "\n\t# disable\n";

    my $fh = new FileHandle "config.disable";
    if (defined $fh) {
	my @a;
	my $_regexp;
	while (<$fh>) {
	    chomp;
	    if ($_) {
		$_regexp = '';
		for my $e (split(/\s+/, $_)) {
		    $_regexp .= ".*" . quotemeta($e);
		}
		$regexp .= "\ts/^/\\#/ if /^\\w/ &&/^$_regexp/;\n";
	    }
	}
	$fh->close();
    }
}
