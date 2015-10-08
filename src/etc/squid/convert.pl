#!/usr/bin/env perl
#
# $FML: convert.pl,v 1.5 2002/02/22 13:28:47 fukachan Exp $
#

my $allow = q{
http_access allow manager localhost
http_access allow local
http_access allow localhost
};

my $localnet = q{
acl local src 10.0.0.0/255.0.0.0 172.16.0.0/255.248.0.0 192.168.0.0/255.255.0.0
};

while (<>) {

	s!^\# mime_table.*!mime_table /usr/pkg/etc/squid/mime.conf!;
	s!^\# cache_dir.*!cache_dir ufs /var/squid/cache 1 2 2!;
	s!^\# cache_mem.*!cache_mem 1 MB!;
	s!^\# cache_swap_high.*!cache_swap_high 50!;
	s!^\# cache_swap_low.*!cache_swap_low  30!;
	s!^\# http_port.*!http_port 127.0.0.1:3128!;
	s!^\#.*: httpd_accel_host.*!httpd_accel_host virtual!;
	s!^\# httpd_accel_port.*!httpd_accel_port 80!;
	s!^\# httpd_accel_uses_host_header.*!httpd_accel_uses_host_header on!;
	s!^\# httpd_accel_with_proxy.*!httpd_accel_with_proxy on!;
	s!^\# maximum_object_size.*!maximum_object_size 1 KB!;
	s!^\# icon_directory.*!icon_directory /usr/pkg/share/squid/icons!;
	s!^\# error_directory.*!error_directory /usr/pkg/share/squid/errors!;
	s!^(acl localhost.*)!$1\n$localnet!;
	s!^acl local src.*!$localnet!;
	s!^http_access allow manager localhost!$allow!;

	print $_;
}

exit 0;
