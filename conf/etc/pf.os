45046:64:0:44:M*:		AIX:4.3::AIX 4.3
16384:64:0:44:M512:		AIX:4.3:2-3:AIX 4.3.2 and earlier

16384:64:0:60:M512,N,W%2,N,N,T:		AIX:4.3:3:AIX 4.3.3-5.2
16384:64:0:60:M512,N,W%2,N,N,T:		AIX:5.1-5.2::AIX 4.3.3-5.2
32768:64:0:60:M512,N,W%2,N,N,T:		AIX:4.3:3:AIX 4.3.3-5.2
32768:64:0:60:M512,N,W%2,N,N,T:		AIX:5.1-5.2::AIX 4.3.3-5.2
65535:64:0:60:M512,N,W%2,N,N,T:		AIX:4.3:3:AIX 4.3.3-5.2
65535:64:0:60:M512,N,W%2,N,N,T:		AIX:5.1-5.2::AIX 4.3.3-5.2
65535:64:0:64:M*,N,W1,N,N,T,N,N,S:	AIX:5.3:ML1:AIX 5.3 ML1

# ----------------- Linux -------------------

# S1:64:0:44:M*:A:		Linux:1.2::Linux 1.2.x (XXX quirks support)
512:64:0:44:M*:			Linux:2.0:3x:Linux 2.0.3x
16384:64:0:44:M*:		Linux:2.0:3x:Linux 2.0.3x

# Endian snafu! Nelson says "ha-ha":
2:64:0:44:M*:			Linux:2.0:3x:Linux 2.0.3x (MkLinux) on Mac
64:64:0:44:M*:			Linux:2.0:3x:Linux 2.0.3x (MkLinux) on Mac


S4:64:1:60:M1360,S,T,N,W0:	Linux:google::Linux (Google crawlbot)

S2:64:1:60:M*,S,T,N,W0:		Linux:2.4::Linux 2.4 (big boy)
S3:64:1:60:M*,S,T,N,W0:		Linux:2.4:.18-21:Linux 2.4.18 and newer
S4:64:1:60:M*,S,T,N,W0:		Linux:2.4::Linux 2.4/2.6 <= 2.6.7
S4:64:1:60:M*,S,T,N,W0:		Linux:2.6:.1-7:Linux 2.4/2.6 <= 2.6.7
S4:64:1:60:M*,S,T,N,W7:		Linux:2.6:8:Linux 2.6.8 and newer (?)

S3:64:1:60:M*,S,T,N,W1:		Linux:2.5::Linux 2.5 (sometimes 2.4)
S4:64:1:60:M*,S,T,N,W1:		Linux:2.5-2.6::Linux 2.5/2.6
S3:64:1:60:M*,S,T,N,W2:		Linux:2.5::Linux 2.5 (sometimes 2.4)
S4:64:1:60:M*,S,T,N,W2:		Linux:2.5::Linux 2.5 (sometimes 2.4)

S20:64:1:60:M*,S,T,N,W0:	Linux:2.2:20-25:Linux 2.2.20 and newer
S22:64:1:60:M*,S,T,N,W0:	Linux:2.2::Linux 2.2
S11:64:1:60:M*,S,T,N,W0:	Linux:2.2::Linux 2.2

# Popular cluster config scripts disable timestamps and
# selective ACK:
S4:64:1:48:M1460,N,W0:		Linux:2.4:cluster:Linux 2.4 in cluster

# This needs to be investigated. On some systems, WSS
# is selected as a multiple of MTU instead of MSS. I got
# many submissions for this for many late versions of 2.4:
T4:64:1:60:M1412,S,T,N,W0:	Linux:2.4::Linux 2.4 (late, uncommon)

# This happens only over loopback, but let's make folks happy:
32767:64:1:60:M16396,S,T,N,W0:	Linux:2.4:lo0:Linux 2.4 (local)
S8:64:1:60:M3884,S,T,N,W0:	Linux:2.2:lo0:Linux 2.2 (local)

# Opera visitors:
16384:64:1:60:M*,S,T,N,W0:	Linux:2.2:Opera:Linux 2.2 (Opera?)
32767:64:1:60:M*,S,T,N,W0:	Linux:2.4:Opera:Linux 2.4 (Opera?)

# Some fairly common mods:
S4:64:1:52:M*,N,N,S,N,W0:	Linux:2.4:ts:Linux 2.4 w/o timestamps
S22:64:1:52:M*,N,N,S,N,W0:	Linux:2.2:ts:Linux 2.2 w/o timestamps


# ----------------- FreeBSD -----------------

16384:64:1:44:M*:		FreeBSD:2.0-2.2::FreeBSD 2.0-4.2
16384:64:1:44:M*:		FreeBSD:3.0-3.5::FreeBSD 2.0-4.2
16384:64:1:44:M*:		FreeBSD:4.0-4.2::FreeBSD 2.0-4.2
16384:64:1:60:M*,N,W0,N,N,T:	FreeBSD:4.4::FreeBSD 4.4

1024:64:1:60:M*,N,W0,N,N,T:	FreeBSD:4.4::FreeBSD 4.4

57344:64:1:44:M*:		FreeBSD:4.6-4.8:noRFC1323:FreeBSD 4.6-4.8 (no RFC1323)
57344:64:1:60:M*,N,W0,N,N,T:	FreeBSD:4.6-4.9::FreeBSD 4.6-4.9

32768:64:1:60:M*,N,W0,N,N,T:	FreeBSD:4.8-4.11::FreeBSD 4.8-5.1 (or MacOS X)
32768:64:1:60:M*,N,W0,N,N,T:	FreeBSD:5.0-5.1::FreeBSD 4.8-5.1 (or MacOS X)
65535:64:1:60:M*,N,W0,N,N,T:	FreeBSD:4.8-4.11::FreeBSD 4.8-5.2 (or MacOS X)
65535:64:1:60:M*,N,W0,N,N,T:	FreeBSD:5.0-5.2::FreeBSD 4.8-5.2 (or MacOS X)
65535:64:1:60:M*,N,W1,N,N,T:	FreeBSD:4.7-4.11::FreeBSD 4.7-5.2
65535:64:1:60:M*,N,W1,N,N,T:	FreeBSD:5.0-5.2::FreeBSD 4.7-5.2

# XXX need quirks support
# 65535:64:1:60:M*,N,W0,N,N,T:Z:FreeBSD:5.1-5.4::5.1-current (1)
# 65535:64:1:60:M*,N,W1,N,N,T:Z:FreeBSD:5.1-5.4::5.1-current (2)
# 65535:64:1:60:M*,N,W2,N,N,T:Z:FreeBSD:5.1-5.4::5.1-current (3)
# 65535:64:1:44:M*:Z:FreeBSD:5.2::FreeBSD 5.2 (no RFC1323)

# 16384:64:1:60:M*,N,N,N,N,N,N,T:FreeBSD:4.4:noTS:FreeBSD 4.4 (w/o timestamps)

# ----------------- NetBSD ------------------

16384:64:0:60:M*,N,W0,N,N,T:	NetBSD:1.3::NetBSD 1.3
65535:64:0:60:M*,N,W0,N,N,T0:	NetBSD:1.6:opera:NetBSD 1.6 (Opera)
16384:64:0:60:M*,N,W0,N,N,T0:	NetBSD:1.6::NetBSD 1.6
16384:64:1:60:M*,N,W0,N,N,T0:	NetBSD:1.6:df:NetBSD 1.6 (DF)
65535:64:1:60:M*,N,W1,N,N,T0:	NetBSD:1.6::NetBSD 1.6W-current (DF)
65535:64:1:60:M*,N,W0,N,N,T0:	NetBSD:1.6::NetBSD 1.6X (DF)
32768:64:1:60:M*,N,W0,N,N,T0:	NetBSD:1.6:randomization:NetBSD 1.6ZH-current (w/ ip_id randomization)

# ----------------- OpenBSD -----------------

16384:64:0:60:M*,N,W0,N,N,T:		OpenBSD:2.6::NetBSD 1.3 (or OpenBSD 2.6)
16384:64:1:64:M*,N,N,S,N,W0,N,N,T:	OpenBSD:3.0-4.0::OpenBSD 3.0-4.0
16384:64:0:64:M*,N,N,S,N,W0,N,N,T:	OpenBSD:3.0-4.0:no-df:OpenBSD 3.0-4.0 (scrub no-df)
57344:64:1:64:M*,N,N,S,N,W0,N,N,T:	OpenBSD:3.3-4.0::OpenBSD 3.3-4.0
57344:64:0:64:M*,N,N,S,N,W0,N,N,T:	OpenBSD:3.3-4.0:no-df:OpenBSD 3.3-4.0 (scrub no-df)

65535:64:1:64:M*,N,N,S,N,W0,N,N,T:	OpenBSD:3.0-4.0:opera:OpenBSD 3.0-4.0 (Opera)

# ----------------- Solaris -----------------

S17:64:1:64:N,W3,N,N,T0,N,N,S,M*:	Solaris:8:RFC1323:Solaris 8 RFC1323
S17:64:1:48:N,N,S,M*:			Solaris:8::Solaris 8
S17:255:1:44:M*:			Solaris:2.5-2.7::Solaris 2.5 to 7

S6:255:1:44:M*:				Solaris:2.6-2.7::Solaris 2.6 to 7
S23:255:1:44:M*:			Solaris:2.5:1:Solaris 2.5.1
S34:64:1:48:M*,N,N,S:			Solaris:2.9::Solaris 9
S44:255:1:44:M*:			Solaris:2.7::Solaris 7

4096:64:0:44:M1460:			SunOS:4.1::SunOS 4.1.x

S34:64:1:52:M*,N,W0,N,N,S:		Solaris:10:beta:Solaris 10 (beta)
32850:64:1:64:M*,N,N,T,N,W1,N,N,S:	Solaris:10::Solaris 10 1203

# ----------------- IRIX --------------------

49152:64:0:44:M*:			IRIX:6.4::IRIX 6.4
61440:64:0:44:M*:			IRIX:6.2-6.5::IRIX 6.2-6.5
49152:64:0:52:M*,N,W2,N,N,S:		IRIX:6.5:RFC1323:IRIX 6.5 (RFC1323)
49152:64:0:52:M*,N,W3,N,N,S:		IRIX:6.5:RFC1323:IRIX 6.5 (RFC1323)

61440:64:0:48:M*,N,N,S:			IRIX:6.5:12-21:IRIX 6.5.12 - 6.5.21
49152:64:0:48:M*,N,N,S:			IRIX:6.5:15-21:IRIX 6.5.15 - 6.5.21

49152:60:0:64:M*,N,W2,N,N,T,N,N,S:	IRIX:6.5:IP27:IRIX 6.5 IP27


# ----------------- Tru64 -------------------

32768:64:1:48:M*,N,W0:			Tru64:4.0::Tru64 4.0 (or OS/2 Warp 4)
32768:64:0:48:M*,N,W0:			Tru64:5.0::Tru64 5.0
8192:64:0:44:M1460:			Tru64:5.1:noRFC1323:Tru64 6.1 (no RFC1323) (or QNX 6)
61440:64:0:48:M*,N,W0:			Tru64:5.1a:JP4:Tru64 v5.1a JP4 (or OpenVMS 7.x on Compaq 5.x stack)

# ----------------- OpenVMS -----------------

6144:64:1:60:M*,N,W0,N,N,T:		OpenVMS:7.2::OpenVMS 7.2 (Multinet 4.4 stack)

# ----------------- MacOS -------------------

# XXX Need EOL tcp opt support
# S2:255:1:48:M*,W0,E:.:MacOS:8.6 classic

# XXX some of these use EOL too
16616:255:1:48:M*,W0:			MacOS:7.3-7.6:OTTCP:MacOS 7.3-8.6 (OTTCP)
16616:255:1:48:M*,W0:			MacOS:8.0-8.6:OTTCP:MacOS 7.3-8.6 (OTTCP)
16616:255:1:48:M*,N,N,N:		MacOS:8.1-8.6:OTTCP:MacOS 8.1-8.6 (OTTCP)
32768:255:1:48:M*,W0,N:			MacOS:9.0-9.2::MacOS 9.0-9.2
65535:255:1:48:M*,N,N,N,N:		MacOS:9.1::MacOS 9.1 (OT 2.7.4)


# ----------------- Windows -----------------

# Windows TCP/IP stack is a mess. For most recent XP, 2000 and
# even 98, the pathlevel, not the actual OS version, is more
# relevant to the signature. They share the same code, so it would
# seem. Luckily for us, almost all Windows 9x boxes have an
# awkward MSS of 536, which I use to tell one from another
# in most difficult cases.

8192:32:1:44:M*:			Windows:3.11::Windows 3.11 (Tucows)
S44:64:1:64:M*,N,W0,N,N,T0,N,N,S:	Windows:95::Windows 95
8192:128:1:64:M*,N,W0,N,N,T0,N,N,S:	Windows:95:b:Windows 95b

# There were so many tweaking tools and so many stack versions for
# Windows 98 it is no longer possible to tell them from each other
# without some very serious research. Until then, there's an insane
# number of signatures, for your amusement:

S44:32:1:48:M*,N,N,S:			Windows:98:lowTTL:Windows 98 (low TTL)
8192:32:1:48:M*,N,N,S:			Windows:98:lowTTL:Windows 98 (low TTL)
%8192:64:1:48:M536,N,N,S:		Windows:98::Windows 98
%8192:128:1:48:M536,N,N,S:		Windows:98::Windows 98
S4:64:1:48:M*,N,N,S:			Windows:98::Windows 98
S6:64:1:48:M*,N,N,S:			Windows:98::Windows 98
S12:64:1:48:M*,N,N,S:			Windows:98::Windows 98
T30:64:1:64:M1460,N,W0,N,N,T0,N,N,S:	Windows:98::Windows 98
32767:64:1:48:M*,N,N,S:			Windows:98::Windows 98
37300:64:1:48:M*,N,N,S:			Windows:98::Windows 98
46080:64:1:52:M*,N,W3,N,N,S:		Windows:98:RFC1323:Windows 98 (RFC1323)
65535:64:1:44:M*:			Windows:98:noSack:Windows 98 (no sack)
S16:128:1:48:M*,N,N,S:			Windows:98::Windows 98
S16:128:1:64:M*,N,W0,N,N,T0,N,N,S:	Windows:98::Windows 98
S26:128:1:48:M*,N,N,S:			Windows:98::Windows 98
T30:128:1:48:M*,N,N,S:			Windows:98::Windows 98
32767:128:1:52:M*,N,W0,N,N,S:		Windows:98::Windows 98
60352:128:1:48:M*,N,N,S:		Windows:98::Windows 98
60352:128:1:64:M*,N,W2,N,N,T0,N,N,S:	Windows:98::Windows 98

# What's with 1414 on NT?
T31:128:1:44:M1414:			Windows:NT:4.0:Windows NT 4.0 SP6a
64512:128:1:44:M1414:			Windows:NT:4.0:Windows NT 4.0 SP6a
8192:128:1:44:M*:			Windows:NT:4.0:Windows NT 4.0 (older)

# Windows XP and 2000. Most of the signatures that were
# either dubious or non-specific (no service pack data)
# were deleted and replaced with generics at the end.

65535:128:1:48:M*,N,N,S:		Windows:2000:SP4:Windows 2000 SP4, XP SP1
65535:128:1:48:M*,N,N,S:		Windows:XP:SP1:Windows 2000 SP4, XP SP1
%8192:128:1:48:M*,N,N,S:		Windows:2000:SP2+:Windows 2000 SP2, XP SP1 (seldom 98 4.10.2222)
%8192:128:1:48:M*,N,N,S:		Windows:XP:SP1:Windows 2000 SP2, XP SP1 (seldom 98 4.10.2222)
S20:128:1:48:M*,N,N,S:			Windows:2000::Windows 2000/XP SP3
S20:128:1:48:M*,N,N,S:			Windows:XP:SP3:Windows 2000/XP SP3
S45:128:1:48:M*,N,N,S:			Windows:2000:SP4:Windows 2000 SP4, XP SP 1
S45:128:1:48:M*,N,N,S:			Windows:XP:SP1:Windows 2000 SP4, XP SP 1
40320:128:1:48:M*,N,N,S:		Windows:2000:SP4:Windows 2000 SP4

S6:128:1:48:M*,N,N,S:			Windows:2000:SP2:Windows XP, 2000 SP2+
S6:128:1:48:M*,N,N,S:			Windows:XP::Windows XP, 2000 SP2+
S12:128:1:48:M*,N,N,S:			Windows:XP:SP1:Windows XP SP1
S44:128:1:48:M*,N,N,S:			Windows:2000:SP3:Windows Pro SP1, 2000 SP3
S44:128:1:48:M*,N,N,S:			Windows:XP:SP1:Windows Pro SP1, 2000 SP3
64512:128:1:48:M*,N,N,S:		Windows:2000:SP3:Windows SP1, 2000 SP3
64512:128:1:48:M*,N,N,S:		Windows:XP:SP1:Windows SP1, 2000 SP3
32767:128:1:48:M*,N,N,S:		Windows:2000:SP4:Windows SP1, 2000 SP4
32767:128:1:48:M*,N,N,S:		Windows:XP:SP1:Windows SP1, 2000 SP4

# Odds, ends, mods:

S52:128:1:48:M1260,N,N,S:		Windows:2000:cisco:Windows XP/2000 via Cisco
S52:128:1:48:M1260,N,N,S:		Windows:XP:cisco:Windows XP/2000 via Cisco
65520:128:1:48:M*,N,N,S:		Windows:XP::Windows XP bare-bone
16384:128:1:52:M536,N,W0,N,N,S:		Windows:2000:ZoneAlarm:Windows 2000 w/ZoneAlarm?
2048:255:0:40:.:			Windows:.NET::Windows .NET Enterprise Server

44620:64:0:48:M*,N,N,S:			Windows:ME::Windows ME no SP (?)
S6:255:1:48:M536,N,N,S:			Windows:95:winsock2:Windows 95 winsock 2
32768:32:1:52:M1460,N,W0,N,N,S:		Windows:2003:AS:Windows 2003 AS


# No need to be more specific, it passes:
# *:128:1:48:M*,N,N,S:U:-Windows:XP/2000 while downloading (leak!) XXX quirk
# there is an equiv similar generic sig w/o the quirk

# ----------------- HP/UX -------------------

32768:64:1:44:M*:			HP-UX:B.10.20::HP-UX B.10.20
32768:64:0:48:M*,W0,N:			HP-UX:11.0::HP-UX 11.0
32768:64:1:48:M*,W0,N:			HP-UX:11.10::HP-UX 11.0 or 11.11
32768:64:1:48:M*,W0,N:			HP-UX:11.11::HP-UX 11.0 or 11.11

# Whoa. Hardcore WSS.
0:64:0:48:M*,W0,N:			HP-UX:B.11.00:A:HP-UX B.11.00 A (RFC1323)

# ----------------- RiscOS ------------------

# We don't yet support the ?12 TCP option
#16384:64:1:68:M1460,N,W0,N,N,T,N,N,?12:	RISCOS:3.70-4.36::RISC OS 3.70-4.36
12288:32:0:44:M536:				RISC OS:3.70:4.10:RISC OS 3.70 inet 4.10

# XXX quirk
# 4096:64:1:56:M1460,N,N,T:T:			RISC OS:3.70:freenet:RISC OS 3.70 freenet 2.00



# ----------------- BSD/OS ------------------

# Once again, power of two WSS is also shared by MacOS X with DF set
8192:64:1:60:M1460,N,W0,N,N,T:		BSD/OS:3.1::BSD/OS 3.1-4.3 (or MacOS X 10.2 w/DF)
8192:64:1:60:M1460,N,W0,N,N,T:		BSD/OS:4.0-4.3::BSD/OS 3.1-4.3 (or MacOS X 10.2)


# ---------------- NewtonOS -----------------

4096:64:0:44:M1420:		NewtonOS:2.1::NewtonOS 2.1

# ---------------- NeXTSTEP -----------------

S4:64:0:44:M1024:		NeXTSTEP:3.3::NeXTSTEP 3.3
S8:64:0:44:M512:		NeXTSTEP:3.3::NeXTSTEP 3.3

# ------------------ BeOS -------------------

1024:255:0:48:M*,N,W0:		BeOS:5.0-5.1::BeOS 5.0-5.1
12288:255:0:44:M1402:		BeOS:5.0::BeOS 5.0.x

# ------------------ OS/400 -----------------

8192:64:1:60:M1440,N,W0,N,N,T:	OS/400:VR4::OS/400 VR4/R5
8192:64:1:60:M1440,N,W0,N,N,T:	OS/400:VR5::OS/400 VR4/R5
4096:64:1:60:M1440,N,W0,N,N,T:	OS/400:V4R5:CF67032:OS/400 V4R5 + CF67032

# XXX quirk
# 28672:64:0:44:M1460:A:OS/390:?

# ------------------ ULTRIX -----------------

16384:64:0:40:.:		ULTRIX:4.5::ULTRIX 4.5

# ------------------- QNX -------------------

S16:64:0:44:M512:		QNX:::QNX demodisk

# ------------------ Novell -----------------

16384:128:1:44:M1460:		Novell:NetWare:5.0:Novel Netware 5.0
6144:128:1:44:M1460:		Novell:IntranetWare:4.11:Novell IntranetWare 4.11
6144:128:1:44:M1368:		Novell:BorderManager::Novell BorderManager ?

6144:128:1:52:M*,W0,N,S,N,N:	Novell:Netware:6:Novell Netware 6 SP3


# ----------------- SCO ------------------
S3:64:1:60:M1460,N,W0,N,N,T:	SCO:UnixWare:7.1:SCO UnixWare 7.1
S17:64:1:60:M1380,N,W0,N,N,T:	SCO:UnixWare:7.1:SCO UnixWare 7.1.3 MP3
S23:64:1:44:M1380:		SCO:OpenServer:5.0:SCO OpenServer 5.0

# ------------------- DOS -------------------

2048:255:0:44:M536:		DOS:WATTCP:1.05:DOS Arachne via WATTCP/1.05
T2:255:0:44:M984:		DOS:WATTCP:1.05Arachne:Arachne via WATTCP/1.05 (eepro)

# ------------------ OS/2 -------------------

S56:64:0:44:M512:		OS/2:4::OS/2 4
28672:64:0:44:M1460:		OS/2:4::OS/2 Warp 4.0

# ----------------- TOPS-20 -----------------

# Another hardcore MSS, one of the ACK leakers hunted down.
# XXX QUIRK 0:64:0:44:M1460:A:TOPS-20:version 7
0:64:0:44:M1460:		TOPS-20:7::TOPS-20 version 7

# ----------------- FreeMiNT ----------------

S44:255:0:44:M536:		FreeMiNT:1:16A:FreeMiNT 1 patch 16A (Atari)

# ------------------ AMIGA ------------------

# XXX TCP option 12
# S32:64:1:56:M*,N,N,S,N,N,?12:.:AMIGA:3.9 BB2 with Miami stack

# ------------------ Plan9 ------------------

65535:255:0:48:M1460,W0,N:	Plan9:4::Plan9 edition 4

# ----------------- AMIGAOS -----------------

16384:64:1:48:M1560,N,N,S:	AMIGAOS:3.9::AMIGAOS 3.9 BB2 MiamiDX

###########################################
# Appliance / embedded / other signatures #
###########################################

# ---------- Firewalls / routers ------------

S12:64:1:44:M1460:			@Checkpoint:::Checkpoint (unknown 1)
S12:64:1:48:N,N,S,M1460:		@Checkpoint:::Checkpoint (unknown 2)
4096:32:0:44:M1460:			ExtremeWare:4.x::ExtremeWare 4.x

# XXX TCP option 12
# S32:64:0:68:M512,N,W0,N,N,T,N,N,?12:.:Nokia:IPSO w/Checkpoint NG FP3
# S16:64:0:68:M1024,N,W0,N,N,T,N,N,?12:.:Nokia:IPSO 3.7 build 026

S4:64:1:60:W0,N,S,T,M1460:		FortiNet:FortiGate:50:FortiNet FortiGate 50

8192:64:1:44:M1460:			Eagle:::Eagle Secure Gateway

S52:128:1:48:M1260,N,N,N,N:		LinkSys:WRV54G::LinkSys WRV54G VPN router



# ------- Switches and other stuff ----------

4128:255:0:44:M*:			Cisco:::Cisco Catalyst 3500, 7500 etc
S8:255:0:44:M*:				Cisco:12008::Cisco 12008
60352:128:1:64:M1460,N,W2,N,N,T,N,N,S:	Alteon:ACEswitch::Alteon ACEswitch
64512:128:1:44:M1370:			Nortel:Contivity Client::Nortel Conectivity Client


# ---------- Caches and whatnots ------------

S4:64:1:52:M1460,N,N,S,N,W0:		AOL:web cache::AOL web cache

32850:64:1:64:N,W1,N,N,T,N,N,S,M*:	NetApp:5.x::NetApp Data OnTap 5.x
16384:64:1:64:M1460,N,N,S,N,W0,N:	NetApp:5.3:1:NetApp 5.3.1
65535:64:0:64:M1460,N,N,S,N,W*,N,N,T:	NetApp:5.3-5.5::NetApp 5.3-5.5
65535:64:0:60:M1460,N,W0,N,N,T:		NetApp:CacheFlow::NetApp CacheFlow
8192:64:1:64:M1460,N,N,S,N,W0,N,N,T:	NetApp:5.2:1:NetApp NetCache 5.2.1
20480:64:1:64:M1460,N,N,S,N,W0,N,N,T:	NetApp:4.1::NetApp NetCache4.1

65535:64:0:60:M1460,N,W0,N,N,T:		CacheFlow:4.1::CacheFlow CacheOS 4.1
8192:64:0:60:M1380,N,N,N,N,N,N,T:	CacheFlow:1.1::CacheFlow CacheOS 1.1

S4:64:0:48:M1460,N,N,S:			Cisco:Content Engine::Cisco Content Engine

27085:128:0:40:.:			Dell:PowerApp cache::Dell PowerApp (Linux-based)

65535:255:1:48:N,W1,M1460:		Inktomi:crawler::Inktomi crawler
S1:255:1:60:M1460,S,T,N,W0:		LookSmart:ZyBorg::LookSmart ZyBorg

16384:255:0:40:.:			Proxyblocker:::Proxyblocker (what's this?)

65535:255:0:48:M*,N,N,S:		Redline:::Redline T|X 2200

32696:128:0:40:M1460:			Spirent:Avalanche::Spirent Web Avalanche HTTP benchmarking engine

# ----------- Embedded systems --------------

S9:255:0:44:M536:			PalmOS:Tungsten:C:PalmOS Tungsten C
S5:255:0:44:M536:			PalmOS:3::PalmOS 3/4
S5:255:0:44:M536:			PalmOS:4::PalmOS 3/4
S4:255:0:44:M536:			PalmOS:3:5:PalmOS 3.5
2948:255:0:44:M536:			PalmOS:3:5:PalmOS 3.5.3 (Handera)
S29:255:0:44:M536:			PalmOS:5::PalmOS 5.0
16384:255:0:44:M1398:			PalmOS:5.2:Clie:PalmOS 5.2 (Clie)
S14:255:0:44:M1350:			PalmOS:5.2:Treo:PalmOS 5.2.1 (Treo)

S23:64:1:64:N,W1,N,N,T,N,N,S,M1460:	SymbianOS:7::SymbianOS 7

8192:255:0:44:M1460:			SymbianOS:6048::Symbian OS 6048 (Nokia 7650?)
8192:255:0:44:M536:			SymbianOS:9210::Symbian OS (Nokia 9210?)
S22:64:1:56:M1460,T,S:			SymbianOS:P800::Symbian OS ? (SE P800?)
S36:64:1:56:M1360,T,S:			SymbianOS:6600::Symbian OS 60xx (Nokia 6600?)


# Perhaps S4?
5840:64:1:60:M1452,S,T,N,W1:		Zaurus:3.10::Zaurus 3.10

32768:128:1:64:M1460,N,W0,N,N,T0,N,N,S:	PocketPC:2002::PocketPC 2002

S1:255:0:44:M346:			Contiki:1.1:rc0:Contiki 1.1-rc0

4096:128:0:44:M1460:			Sega:Dreamcast:3.0:Sega Dreamcast Dreamkey 3.0
T5:64:0:44:M536:			Sega:Dreamcast:HKT-3020:Sega Dreamcast HKT-3020 (browser disc 51027)
S22:64:1:44:M1460:			Sony:PS2::Sony Playstation 2 (SOCOM?)

S12:64:0:44:M1452:			AXIS:5600:v5.64:AXIS Printer Server 5600 v5.64

3100:32:1:44:M1460:			Windows:CE:2.0:Windows CE 2.0

####################
# Fancy signatures #
####################

1024:64:0:40:.:				*NMAP:syn scan:1:NMAP syn scan (1)
2048:64:0:40:.:				*NMAP:syn scan:2:NMAP syn scan (2)
3072:64:0:40:.:				*NMAP:syn scan:3:NMAP syn scan (3)
4096:64:0:40:.:				*NMAP:syn scan:4:NMAP syn scan (4)

# Requires quirks support
# 1024:64:0:40:.:A:*NMAP:TCP sweep probe (1)
# 2048:64:0:40:.:A:*NMAP:TCP sweep probe (2)
# 3072:64:0:40:.:A:*NMAP:TCP sweep probe (3)
# 4096:64:0:40:.:A:*NMAP:TCP sweep probe (4)

1024:64:0:60:W10,N,M265,T:		*NMAP:OS:1:NMAP OS detection probe (1)
2048:64:0:60:W10,N,M265,T:		*NMAP:OS:2:NMAP OS detection probe (2)
3072:64:0:60:W10,N,M265,T:		*NMAP:OS:3:NMAP OS detection probe (3)
4096:64:0:60:W10,N,M265,T:		*NMAP:OS:4:NMAP OS detection probe (4)

32767:64:0:40:.:			*NAST:::NASTsyn scan

# Requires quirks support
# 12345:255:0:40:.:A:-p0f:sendsyn utility


#####################################
# Generic signatures - just in case #
#####################################

#*:64:1:60:M*,N,W*,N,N,T:		@FreeBSD:4.0-4.9::FreeBSD 4.x/5.x
#*:64:1:60:M*,N,W*,N,N,T:		@FreeBSD:5.0-5.1::FreeBSD 4.x/5.x

*:128:1:52:M*,N,W0,N,N,S:		@Windows:XP:RFC1323:Windows XP/2000 (RFC1323 no tstamp)
*:128:1:52:M*,N,W0,N,N,S:		@Windows:2000:RFC1323:Windows XP/2000 (RFC1323 no tstamp)
*:128:1:52:M*,N,W*,N,N,S:		@Windows:XP:RFC1323:Windows XP/2000 (RFC1323 no tstamp)
*:128:1:52:M*,N,W*,N,N,S:		@Windows:2000:RFC1323:Windows XP/2000 (RFC1323 no tstamp)
*:128:1:64:M*,N,W0,N,N,T0,N,N,S:	@Windows:XP:RFC1323:Windows XP/2000 (RFC1323)
*:128:1:64:M*,N,W0,N,N,T0,N,N,S:	@Windows:2000:RFC1323:Windows XP/2000 (RFC1323)
*:128:1:64:M*,N,W*,N,N,T0,N,N,S:	@Windows:XP:RFC1323:Windows XP (RFC1323, w+)
*:128:1:48:M536,N,N,S:			@Windows:98::Windows 98
*:128:1:48:M*,N,N,S:			@Windows:XP::Windows XP/2000
*:128:1:48:M*,N,N,S:			@Windows:2000::Windows XP/2000


