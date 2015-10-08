Summary: PPP Over Ethernet (xDSL support)
Name: rp-pppoe
Version: 2.8
%if %(%{expand:test %{_vendor} != mandrake ; echo $?})
Release: 1mdk
%else
Release: 1
%endif
Copyright: GPL
Group: System Environment/Daemons
Source: http://www.roaringpenguin.com/pppoe/rp-pppoe-2.8.tar.gz
Url: http://www.roaringpenguin.com/pppoe/
Packager: David F. Skoll <dfs@roaringpenguin.com>
BuildRoot: /tmp/pppoe-build
Vendor: Roaring Penguin Software Inc.
Requires: ppp >= 2.3.7

%description
PPPoE (Point-to-Point Protocol over Ethernet) is a protocol used by
many ADSL Internet Service Providers. Roaring Penguin has a free
client for Linux systems to connect to PPPoE service providers.

The client is a user-mode program and does not require any kernel
modifications. It is fully compliant with RFC 2516, the official PPPoE
specification.

%prep
%setup
cd src
./configure

%build
cd src
make

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT
cd src
make install RPM_INSTALL_ROOT=$RPM_BUILD_ROOT

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%doc doc/CHANGES doc/HOW-TO-CONNECT doc/LICENSE doc/KERNEL-MODE-PPPOE README
%config /etc/ppp/pppoe.conf
%config /etc/ppp/pppoe-server-options
%config /etc/ppp/firewall-masq
%config /etc/ppp/firewall-standalone
/etc/ppp/plugins/*
/usr/sbin/pppoe
/usr/sbin/pppoe-server
/usr/sbin/pppoe-sniff
/usr/sbin/pppoe-relay
/usr/sbin/adsl-connect
/usr/sbin/adsl-start
/usr/sbin/adsl-stop
/usr/sbin/adsl-setup
/usr/sbin/adsl-status
%{_mandir}/man5/pppoe.conf.5*
%{_mandir}/man8/pppoe.8*
%{_mandir}/man8/pppoe-server.8*
%{_mandir}/man8/pppoe-relay.8*
%{_mandir}/man8/pppoe-sniff.8*
%{_mandir}/man8/adsl-connect.8*
%{_mandir}/man8/adsl-start.8*
%{_mandir}/man8/adsl-stop.8*
%{_mandir}/man8/adsl-status.8*
%{_mandir}/man8/adsl-setup.8*
/etc/rc.d/init.d/adsl

