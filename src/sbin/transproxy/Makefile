# vim:ai ts=8 sw=8
#
#	Makefile for tproxy.
#
# Currently supported OPTIONS flags are:
#	DEBUG			Print debugging info stdout.
#	DEBUG_INPUT_REQUESTS	Print requests as they are received.
#	DEBUG_OUTPUT_REQUESTS	Print requests after they have been translated.
#	DNS_LOOKUPS		Look up the hostnames via DNS to translate the HTTP
#				request. Only when no Host: header was found.
#	USELESS_DNS_LOOKUPS	Look up the hostnames only needed for the log
#				files. Normally turned off.
#	LOG_TO_SYSLOG		Log all faults and accesses to syslog(3).
#				This can be slow in a busy server.
#	LOG_FAULTS_TO_SYSLOG	Log only faults to syslog(3).
#	LOG_TO_FILE		Log all accesses to a dedicated file instead.
#				File is opened relative to the / directory.
#	LOG_TO_FILE_LINEBUFF	Log in line-buffered mode. Disable to speed
#				things up. Although log file will lag.
#	IPFILTER		Enable the BSD IP filter code.
#				FreeBSD 3.0, NetBSD 1.3.1 and OpenBSD come
#				with this as standard.
#				See http://coombs.anu.edu.au/~avalon/
#	IPTABLES		Enable support for iptables (linux-2.4).
#	TCP_WRAPPERS		Use hosts.allow (daemon tproxy) to limit access.
#	DO_DOUBLE_FORK		Do a double fork to prevent zombie processes on
#				systems that don't provide a mechanism for ignoring
#				the child termination event.
#

# Debugging options. Not normally useful.
#OPTIONS += -DDEBUG
#OPTIONS += -DDEBUG_INPUT_REQUESTS
#OPTIONS += -DDEBUG_OUTPUT_REQUESTS

# Control DNS lookups under various conditions.
OPTIONS += -DDNS_LOOKUPS
#OPTIONS += -DUSELESS_DNS_LOOKUPS

# Error and request logging.
OPTIONS += -DLOG_TO_SYSLOG
OPTIONS += -DLOG_FAULTS_TO_SYSLOG
#OPTIONS += -DLOG_TO_FILE
#OPTIONS += -DLOG_TO_FILE_LINEBUFF

# BSD IPFILTER mechanism for fetching intended destination address.
OPTIONS += -DIPFILTER

# linux-2.4 iptables mechanism for fetching intended destination address.
#OPTIONS += -DIPTABLES

# Double fork to make init(8) handle zombie processes.
#OPTIONS += -DDO_DOUBLE_FORK

# Define these to enable tcp_wrappers. You can use the built-in ACLs instead though.
#OPTIONS += -DTCP_WRAPPERS
#LIBS    += -lwrap

# You may need one or both of these to get a clean compile.
#OPTIONS += -DHAVE_PATHS_H
#OPTIONS += -DHAVE_GETOPT_H

# You may need to touch PREFIX, CC and CFLAGS.
#PREFIX = /usr/local
#INSTALL_PROGRAM = install -c -m 555 -o bin -g bin
#INSTALL_MAN = install -c -m 444 -o bin -g bin

# Some make's don't define this.
#RM      = rm -f

# Should be OK for GNU gcc.
#CC      = gcc
#CFLAGS  = -O2 -Wall
#LDFLAGS = -s

# For using BIND resolver instead of system resolver.
#LIBS    += -lresolv	# Really old Linux has this.
#LIBS    += -lbind

# For Solaris.
#LIBS    += -lsocket -lxnet
#CFLAGS  += -DUSE_STRSTR_BUG

# You shouldn't need to touch anything below this.
all:		tproxy

tproxy:		tproxy.o acl.o
	$(CC) $(LDFLAGS) tproxy.o acl.o -o $@ $(LIBS)

tproxy.o:	tproxy.c Makefile
	$(CC) $(CFLAGS) $(OPTIONS) -c tproxy.c -o $@

acl.o:		acl.c Makefile
	$(CC) $(CFLAGS) $(OPTIONS) -c acl.c -o $@

clean:
	$(RM) tproxy.o acl.o *core *~

clobber dist-clean:	clean
	$(RM) tproxy

install:	tproxy
	$(BSD_INSTALL_PROGRAM) tproxy $(PREFIX)/sbin
	$(BSD_INSTALL_PROGRAM) tproxyrun $(PREFIX)/sbin
	$(BSD_INSTALL_PROGRAM) tproxywatch $(PREFIX)/sbin
	$(BSD_INSTALL_MAN) tproxy.8 $(PREFIX)/man/man8
