/*
 * stone.c	simple repeater
 * Copyright(c)1995-2001 by Hiroaki Sengoku <sengoku@gcd.org>
 * Version 1.0	Jan 28, 1995
 * Version 1.1	Jun  7, 1995
 * Version 1.2	Aug 20, 1995
 * Version 1.3	Feb 16, 1996	relay UDP
 * Version 1.5	Nov 15, 1996	for Win32
 * Version 1.6	Jul  5, 1997	for SSL
 * Version 1.7	Aug 20, 1997	return packet of UDP
 * Version 1.8	Oct 18, 1997	pseudo parallel using SIGALRM
 * Version 2.0	Nov  3, 1997	http proxy & over http
 * Version 2.1	Nov 14, 1998	respawn & pop
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Emacs; see the file COPYING.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Usage: stone [-d] [-n] [-u <max>] [-f <n>] [-a <file>] [-L <file>] [-l]
 *              [-o <n>] [-g <n>] [-t <dir>] [-z <SSL>]
 *              [-C <file>] [-P <command>]
 *              <st> [-- <st>]...
 * <st> := <display> [<xhost>...]
 *        | <host>:<port> <sport> [<xhost>...]
 *        | proxy <sport> [<xhost>...]
 *        | <host>:<port#>/http <sport> <request> [<xhost>...]
 *        | <host>:<port#>/proxy <sport> <header> [<xhost>...]
 * <port>  := <port#>[/udp | /ssl | /apop]
 * <sport> := [<host>:]<port#>[/udp | /ssl | /http]
 * <xhost> := <host>[/<mask>]
 *
 *     Any packets received by <display> are passed to DISPLAY
 *     Any packets received by <sport> are passed to <host>:<port>
 *     as long as these packets are sent from <xhost>...
 *     if <xhost> are not given, any hosts are welcome.
 *
 * Make:
 * gcc -o stone stone.c
 * or
 * cl -DWINDOWS stone.c /MT wsock32.lib
 * or
 * gcc -DWINDOWS -o stone.exe stone.c -lwsock32
 *
 * POP -> APOP conversion
 * gcc -DUSE_POP -o stone stone.c md5c.c
 * or
 * cl -DWINDOWS -DUSE_POP stone.c md5c.c /MT wsock32.lib
 * or
 * gcc -DWINDOWS -DUSE_POP -o stone.exe stone.c md5c.c -lwsock32
 *
 * md5c.c global.h md5.h are contained in RFC1321
 *
 * Using OpenSSL
 * gcc -DUSE_SSL -I/usr/local/ssl/include -o stone stone.c \
 *               -L/usr/local/ssl/lib -lssl -lcrypto
 * or
 * cl -DWINDOWS -DUSE_SSL stone.c /MT wsock32.lib ssleay32.lib libeay32.lib
 * or
 * gcc -DWINDOWS -DUSE_SSL -o stone.exe stone.c -lwsock32 -lssl32 -leay32
 *
 * -DUSE_POP	  use POP -> APOP conversion
 * -DUSE_SSL	  use OpenSSL
 * -DCPP	  preprocessor for reading config. file
 * -DH_ERRNO	  h_errno is not defined in header files
 * -DIGN_SIGTERM  ignore SIGTERM signal
 * -DINET_ADDR	  use custom inet_addr(3)
 * -DNO_ALRM	  without SIGALRM signal
 * -DNO_BCOPY	  without bcopy(3)
 * -DNO_SNPRINTF  without snprintf(3)
 * -DNO_SYSLOG	  without syslog(2)
 * -DNO_THREAD	  without thread
 * -DOS2	  OS/2 with EMX
 * -DWINDOWS	  Windows95/98/NT
 * -DNT_SERVICE	  WindowsNT/2000 native service
 */
#define VERSION	"2.1d"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <ctype.h>
#include <stdarg.h>
#include <signal.h>
typedef void (*FuncPtr)(void*);

#ifdef WINDOWS
#define FD_SETSIZE	256
#include <process.h>
#include <winsock.h>
#include <time.h>
#ifdef NT_SERVICE
#include "service.h"
#include "svcbody.h"
#endif
#define NO_SNPRINTF
#define NO_SYSLOG
#define NO_FORK
#define NO_SETHOSTENT
#define NO_ALRM
#define NO_SETUID
#define NO_CHROOT
#define ValidSocket(sd)		((sd) != INVALID_SOCKET)
#undef EINTR
#define EINTR	WSAEINTR
#define NO_BCOPY
#define bzero(b,n)	memset(b,0,n)
#define ASYNC(func,arg)	\
    if (_beginthread((FuncPtr)func,0,arg) < 0) {\
	message(LOG_ERR,"_beginthread error err=%d",errno);\
	func(arg);\
    }
#else	/* ! WINDOWS */
#include <sys/param.h>
#ifdef OS2
#define INCL_DOSSEMAPHORES
#define INCL_DOSERRORS
#include <process.h>
#include <os2.h>
#define NO_ALRM
#define NO_SYSLOG
#define ASYNC(func,arg)	\
    if (_beginthread((FuncPtr)func,NULL,32768,arg) < 0) {\
	message(LOG_ERR,"_beginthread error err=%d",errno);\
	func(arg);\
    }
#else	/* ! OS2 */
#define ASYNC(func,arg)	func(arg)
#define NO_THREAD
#endif	/* ! WINDOWS */
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
typedef int SOCKET;
#define INVALID_SOCKET		-1
#define ValidSocket(sd)		((sd) >= 0)
#define closesocket(sd)		close(sd)
#endif
#define InvalidSocket(sd)	(!ValidSocket(sd))

#ifdef NO_THREAD
#define ASYNC_BEGIN	/* */
#define ASYNC_END	/* */
#define FD_SET_RIN(sd)	FD_SET(sd,&rin)
#define FD_SET_WIN(sd)	FD_SET(sd,&win)
#define FD_SET_EIN(sd)	FD_SET(sd,&ein)
#define FD_CLR_RIN(sd)	FD_CLR(sd,&rin)
#define FD_CLR_WIN(sd)	FD_CLR(sd,&win)
#define FD_CLR_EIN(sd)	FD_CLR(sd,&ein)
#else
#define ASYNC_BEGIN	\
    waitMutex(AsyncMutex);\
    AsyncCount++;\
    freeMutex(AsyncMutex)
#define ASYNC_END	\
    waitMutex(AsyncMutex);\
    AsyncCount--;\
    freeMutex(AsyncMutex)
#define FD_SET_RIN(sd)	\
    waitMutex(FdRinMutex);\
    FD_SET(sd,&rin);\
    freeMutex(FdRinMutex)
#define FD_SET_WIN(sd)	\
    waitMutex(FdWinMutex);\
    FD_SET(sd,&win);\
    freeMutex(FdWinMutex)
#define FD_SET_EIN(sd)	\
    waitMutex(FdEinMutex);\
    FD_SET(sd,&ein);\
    freeMutex(FdEinMutex)
#define FD_CLR_RIN(sd)	\
    waitMutex(FdRinMutex);\
    FD_CLR(sd,&rin);\
    freeMutex(FdRinMutex)
#define FD_CLR_WIN(sd)	\
    waitMutex(FdWinMutex);\
    FD_CLR(sd,&win);\
    freeMutex(FdWinMutex)
#define FD_CLR_EIN(sd)	\
    waitMutex(FdEinMutex);\
    FD_CLR(sd,&ein);\
    freeMutex(FdEinMutex)
#endif

#ifdef NO_SYSLOG
#define LOG_CRIT	2	/* critical conditions */
#define LOG_ERR		3	/* error conditions */
#define LOG_WARNING	4	/* warning conditions */
#define LOG_NOTICE	5	/* normal but signification condition */
#define LOG_INFO	6	/* informational */
#define LOG_DEBUG	7	/* debug-level messages */
#else	/* SYSLOG */
#include <syslog.h>
#endif

#ifndef EISCONN
#define EISCONN		56		/* Socket is already connected */
#endif
#ifndef EADDRINUSE
#define EADDRINUSE	48		/* Address already in use */
#endif

#define BACKLOG_MAX	5
#define XPORT		6000
#define BUFMAX		2048
#define STRMAX		30	/* > 16 */
#define NTRY_MAX	10
#define IDLE_MAX	(60 * 10)	/* 10 min */
#define CONN_TIMEOUT	60	/* 1 min */

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN	255
#endif

#ifdef NO_ALRM
#define TICK_SELECT	10000	/* 0.01 sec */
#define SPIN_MAX	100	/* 1 sec */
#else
#define TICK_SELECT	100000	/* 0.1 sec */
#define SPIN_MAX	10	/* 1 sec */
#define TICK_TIMER	10000	/* 0.01 sec */
#define RECURS_CNT	30	/* 0.3 sec */
#define RECURS_MAX	10
#endif

#ifdef USE_SSL
#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
SSL_CTX *ssl_ctx_server, *ssl_ctx_client;
char *keyfile, *certfile;
char ssl_file_path[BUFMAX];
int ssl_verbose_flag = 0;
int ssl_verify_flag = SSL_VERIFY_NONE;
char *cipher_list = NULL;
#else
#ifdef USE_POP
#include "global.h"
#include "md5.h"
#endif
#endif
#ifdef CPP
char *CppCommand = CPP;
#endif

typedef struct {
    struct in_addr addr;
    struct in_addr mask;
} XHost;

typedef struct _Stone {
    SOCKET sd;			/* socket descriptor to listen */
    int port;
    struct sockaddr_in sin;	/* destination */
    int proto;
    char *p;
    struct _Stone *next;
#ifdef USE_SSL
    SSL *ssl;			/* SSL handle */
#endif
    int nhosts;			/* # of hosts */
    XHost xhosts[0];		/* hosts permitted to connect */
} Stone;

typedef struct _Pair {
    struct _Pair *pair;
    struct _Pair *prev;
    struct _Pair *next;
#ifdef USE_SSL
    SSL *ssl;		/* SSL handle */
#endif
    time_t clock;
    SOCKET sd;		/* socket descriptor */
    int proto;
    int count;		/* reference counter */
    char *p;
    short start;	/* index of buf */
    short len;
    char buf[BUFMAX];
} Pair;

typedef struct _Conn {
    struct sockaddr_in sin;	/* destination */
    Pair *pair;
    int lock;
    struct _Conn *next;
} Conn;

typedef struct _Origin {
    SOCKET sd;			/* peer */
    Stone *stone;
    struct sockaddr_in sin;	/* origin */
    int lock;
    time_t clock;
    struct _Origin *next;
} Origin;

typedef struct _Comm {
    char *str;
    int (*func)(Pair*,char*,int);
} Comm;

Stone *stones = NULL;
Stone *oldstones = NULL;
Pair pairs;
Conn conns;
Origin origins;
int OriginMax = 10;
fd_set rin, win, ein;
unsigned int Generation = 0;
int Recursion = 0;
int AsyncCount = 0;
#ifdef H_ERRNO
extern int h_errno;
#endif

const state_mask =	    0x00ff;
const proto_command =	    0x0f00;	/* command (destination only) */
const proto_tcp	=	    0x1000;	/* transmission control protocol */
const proto_udp =	    0x2000;	/* user datagram protocol */
const proto_source =	    0x4000;	/* source flag */
const proto_first_r =	   0x10000;	/* first read packet */
const proto_first_w =	   0x20000;	/* first written packet */
const proto_ready_r =	   0x40000;	/* ready to read */
const proto_ready_w =	   0x80000;	/* ready to write */
const proto_connect =	  0x100000;	/* connection established */
const proto_close =	  0x200000;	/* request to close */
const proto_shutdown =	  0x400000;	/* request to shutdown */
const proto_ssl_intr =	  0x800000;	/* SSL accept/connect interrupted */
const proto_ssl_s =    	 0x1000000;	/* SSL source */
const proto_ssl_d =	 0x2000000;	/*     destination */
const proto_ohttp_s =	 0x4000000;	/* over http source */
const proto_ohttp_d =	 0x8000000;	/*           destination */
const proto_base_s =	0x10000000;	/* base64 source */
const proto_base_d =	0x20000000;	/*        destination */
#define command_proxy	    0x0100	/* http proxy */
#define command_ihead	    0x0200	/* insert header */
#define command_pop	    0x0300	/* POP -> APOP conversion */

#define proto_ssl	(proto_ssl_s|proto_ssl_d)
#define proto_ohttp	(proto_ohttp_s|proto_ohttp_d)
#define proto_base	(proto_base_s|proto_base_d)
#define proto_src	(proto_tcp|proto_udp|proto_first_r|proto_first_w|\
			 proto_ssl_s|proto_ohttp_s|proto_base_s|\
			 proto_source)
#define proto_dest	(proto_tcp|proto_udp|proto_first_r|proto_first_w|\
			 proto_ssl_d|proto_ohttp_d|proto_base_d|\
			 proto_command)
#define proto_all	(proto_src|proto_dest)

char *pkt_buf;		/* UDP packet buffer */
int pkt_len_max;	/* size of pkt_buf */
#define PKT_LEN_INI	2048	/* initial size */
int AddrFlag = 0;
#ifndef NO_SYSLOG
int Syslog = 0;
char SyslogName[STRMAX];
#endif
char *MyName = NULL; 
#ifdef USE_IPF
int TransparentMode = 0;
#endif
FILE *LogFp;
FILE *AccFp = NULL;
char *ConfigFile = NULL;
int ConfigArgc = 0;
int OldConfigArgc = 0;
char **ConfigArgv = NULL;
char **OldConfigArgv = NULL;
char *DispHost;
int DispPort;
#ifndef NO_CHROOT
char *RootDir = NULL;
#endif
#ifndef NO_SETUID
unsigned long SetUID = 0;
unsigned long SetGID = 0;
#endif
#ifndef NO_FORK
int NForks = 0;
pid_t *Pid;
#endif
int Debug = 0;		/* debugging level */
#ifdef WINDOWS
HANDLE PairMutex, ConnMutex, OrigMutex, AsyncMutex;
HANDLE FdRinMutex, FdWinMutex, FdEinMutex;
#endif
#ifdef OS2
HMTX PairMutex, ConnMutex, OrigMutex, AsyncMutex;
HMTX FdRinMutex, FdWinMutex, FdEinMutex;
#endif

#ifdef NO_SNPRINTF
#define vsnprintf(str,len,fmt,ap)	vsprintf(str,fmt,ap)
#define snprintf(str,len,fmt,ap)	sprintf(str,fmt,ap)
#endif

#ifdef NO_BCOPY
void bcopy(b1,b2,len)
char *b1, *b2;
int len;
{
    if (b1 < b2 && b2 < b1 + len) {	/* overlapping */
	char *p;
	b2 = b2 + len - 1;
	for (p=b1+len-1; b1 <= p; p--, b2--) *b2 = *p;
    } else {
	memcpy(b2,b1,len);
    }
}
#endif

char *strntime(str,len,clock)
char *str;
int len;
time_t *clock;
{
    char *p, *q;
    int i;
    p = ctime(clock);
    if (p) {
	q = p + strlen(p);
	while (*p++ != ' ')	;
	while (*--q != ' ')	;
	i = 0;
	len--;
	while (p <= q && i < len) str[i++] = *p++;
	str[i] = '\0';
    } else {
	snprintf(str,len,"%lu ",*clock);
    }
    return str;
}

void message(int pri, char *fmt, ...) {
    char str[BUFMAX];
    va_list ap;
#ifndef NO_SYSLOG
    if (Syslog) {
	va_start(ap,fmt);
	vsnprintf(str,BUFMAX,fmt,ap);
	va_end(ap);
	if (Recursion) syslog(pri,"(%d) %s",Recursion,str);
	else syslog(pri,"%s",str);
    } else {
#endif
	time_t clock;
	int i;
	time(&clock);
	strntime(str,BUFMAX,&clock);
	i = strlen(str);
#ifndef NO_FORK
	if (NForks) {
	    snprintf(&str[i],BUFMAX-i,"[%d] ",getpid());
	    i = strlen(str);
	}
#endif
	if (Recursion) {
	    snprintf(&str[i],BUFMAX-i,"(%d) ",Recursion);
	    i = strlen(str);
	}
	va_start(ap,fmt);
	vsnprintf(&str[i],BUFMAX-i-2,fmt,ap);
	va_end(ap);
#ifdef NT_SERVICE
	if (FALSE == bSvcDebug) AddToMessageLog(str);
	else
#endif
	fprintf(LogFp,"%s\n",str);
#ifndef NO_SYSLOG
    }
#endif
}

char *addr2ip(addr,str)
struct in_addr *addr;
char *str;
{
    union {
	u_long	l;
	unsigned char	c[4];
    } u;
    u.l = addr->s_addr;
    sprintf(str,"%d.%d.%d.%d",u.c[0],u.c[1],u.c[2],u.c[3]);
    return str;
}

char *addr2str(addr)
struct in_addr *addr;
{
    static char str[STRMAX];
    struct hostent *ent;
    int ntry = NTRY_MAX;
    addr2ip(addr,str);
    if (!AddrFlag) {
	do {
#ifndef NO_SETHOSTENT
	    sethostent(0);
#endif
	    ent = gethostbyaddr((char*)&addr->s_addr,
				sizeof(addr->s_addr),AF_INET);
	    if (ent) return ent->h_name;
	} while (h_errno == TRY_AGAIN && ntry-- > 0);
	message(LOG_ERR,"Unknown address err=%d: %s",h_errno,str);
    }
    return str;
}

char *port2str(port,flag,mask)
int port;	/* network byte order */
int flag;
int mask;
{
    static char str[STRMAX];
    char *proto;
    struct servent *ent;
    if (flag & proto_udp) {
	proto = "udp";
    } else {
	proto = "tcp";
    }
    str[0] = '\0';
    if (!AddrFlag) {
	ent = getservbyport(port,proto);
	if (ent) strncpy(str,ent->s_name,STRMAX-5);
    }
    if (str[0] == '\0') {
	sprintf(str,"%d",ntohs((unsigned short)port));
    }
    if (flag & proto_udp) {
	strcat(str,"/udp");
    } else if (flag & proto_ohttp & mask) {
	strcat(str,"/http");
    } else if (flag & proto_ssl & mask) {
	strcat(str,"/ssl");
    } else if (flag & proto_base & mask) {
	strcat(str,"/base");
    } else switch(flag & proto_command & mask) {
    case command_ihead:
	strcat(str,"/proxy");
	break;
    case command_pop:
	strcat(str,"/apop");
	break;
    }
    return str;
}

int isdigitstr(str)
char *str;
{
    while (*str && !isspace(*str)) {
	if (!isdigit(*str)) return 0;
	str++;
    }
    return 1;
}

int str2port(str,flag)	/* host byte order */
char *str;
int flag;
{
    struct servent *ent;
    char *proto;
    if (flag & proto_udp) {
	proto = "udp";
    } else {
	proto = "tcp";
    }
    ent = getservbyname(str,proto);
    if (ent) {
	return ntohs(ent->s_port);
    } else if (isdigitstr(str)) {
	return atoi(str);
    } else {
	return -1;
    }
}

int isdigitaddr(name)
char *name;
{
    while(*name) {
	if (*name != '.' && !isdigit(*name)) return 0;	/* not digit */
	name++;
    }
    return 1;
}

#ifdef INET_ADDR
unsigned long inet_addr(name)	/* inet_addr(3) is too tolerant */
char *name;
{
    unsigned long ret;
    int d[4];
    int i;
    char c;
    if (sscanf(name,"%d.%d.%d.%d%c",&d[0],&d[1],&d[2],&d[3],&c) != 4)
	return -1;
    ret = 0;
    for (i=0; i < 4; i++) {
	if (d[i] < 0 || 255 < d[i]) return -1;
	ret <<= 8;
	ret |= d[i];
    }
    return htonl(ret);
}
#endif

int host2addr(name,addrp,familyp)
char *name;
struct in_addr *addrp;
short *familyp;
{
    struct hostent *hp;
    int ntry = NTRY_MAX;
    if (isdigitaddr(name)) {
	if ((addrp->s_addr=inet_addr(name)) != -1) {
	    if (familyp) *familyp = AF_INET;
	    return 1;
	}
    } else {
	do {
#ifndef NO_SETHOSTENT
	    sethostent(0);
#endif
	    hp = gethostbyname(name);
	    if (hp) {
		bcopy(hp->h_addr,(char *)addrp,hp->h_length);
		if (familyp) *familyp = hp->h_addrtype;
		return 1;
	    }
	} while (h_errno == TRY_AGAIN && ntry-- > 0);
    }
    message(LOG_ERR,"Unknown host err=%d: %s",h_errno,name);
    return 0;
}

#ifdef USE_LIBWRAP
#include <syslog.h>
#include <tcpd.h>
#ifndef LIBWRAP_ALLOW_FACILITY
# define LIBWRAP_ALLOW_FACILITY LOG_AUTH
#endif
#ifndef LIBWRAP_ALLOW_SEVERITY
# define LIBWRAP_ALLOW_SEVERITY LOG_INFO
#endif
#ifndef LIBWRAP_DENY_FACILITY
# define LIBWRAP_DENY_FACILITY LOG_AUTH
#endif
#ifndef LIBWRAP_DENY_SEVERITY
# define LIBWRAP_DENY_SEVERITY LOG_WARNING
#endif
int allow_severity = LIBWRAP_ALLOW_FACILITY | LIBWRAP_ALLOW_SEVERITY;
int deny_severity  = LIBWRAP_DENY_FACILITY  | LIBWRAP_DENY_SEVERITY;

int libwrap_hosts_access(addrp)
struct in_addr *addrp;
{
  struct request_info req;
  request_init(&req, 
	       RQ_DAEMON, MyName ? MyName : "stone",
	       RQ_CLIENT_ADDR, inet_ntoa(addrp->s_addr),
	       NULL);
  hosts_access(&req);
}
#endif

/* *addrp is permitted to connect to *stonep ? */
int checkXhost(stonep,addrp)
Stone *stonep;
struct in_addr *addrp;
{
    int i;
#ifdef USE_LIBWRAP
   return libwrap_hosts_access(addrp);
#endif
    if (!stonep->nhosts) return 1; /* any hosts can access */
    for (i=0; i < stonep->nhosts; i++) {
	if ((addrp->s_addr & stonep->xhosts[i].mask.s_addr)
	    == (stonep->xhosts[i].addr.s_addr & stonep->xhosts[i].mask.s_addr))
	    return 1;
    }
    return 0;
}

#ifdef NO_ALRM
#define utimer(usec)	/* */
#else
void utimer(usec)
int usec;
{
    struct itimerval val;
    static struct itimerval oval;
    switch(usec) {
      case -1:	/* resume */
	setitimer(ITIMER_REAL,&oval,NULL);
	break;
      case 0:	/* stop */
	val.it_interval.tv_sec = val.it_interval.tv_usec
	    = val.it_value.tv_sec = val.it_value.tv_usec = 0;
	setitimer(ITIMER_REAL,&val,&oval);
	break;
      default:	/* initialize & start */
	oval.it_interval.tv_sec = oval.it_interval.tv_usec
	    = oval.it_value.tv_sec = oval.it_value.tv_usec = 0;
	val.it_interval.tv_sec = val.it_interval.tv_usec
	    = val.it_value.tv_sec = 0;
	val.it_value.tv_usec = usec;
	setitimer(ITIMER_REAL,&val,NULL);	/* start timer */
    }
}
#endif

#ifdef WINDOWS
void waitMutex(h)
HANDLE h;
{
    DWORD ret;
    if (h) {
	ret = WaitForSingleObject(h,500);	/* 0.5 sec */
	if (ret == WAIT_FAILED) {
	    message(LOG_ERR,"Fail to wait mutex err=%d",GetLastError());
	} else if (ret == WAIT_TIMEOUT) {
	    message(LOG_WARNING,"timeout to wait mutex");
	}
    }
}

void freeMutex(h)
HANDLE h;
{
    if (h) {
	if (!ReleaseMutex(h)) {
	    message(LOG_ERR,"Fail to release mutex err=%d",GetLastError());
	}
    }
}
#else	/* ! WINDOWS */
#ifdef OS2
void waitMutex(h)
HMTX h;
{
    APIRET ret;
    if (h) {
	ret = DosRequestMutexSem(h,500);	/* 0.5 sec */
	if (ret == ERROR_TIMEOUT) {
	    message(LOG_WARNING,"timeout to wait mutex");
	} else if (ret) {
	    message(LOG_ERR,"Fail to request mutex err=%d",ret);
	}
    }
}

void freeMutex(h)
HMTX h;
{
    APIRET ret;
    if (h) {
	ret = DosReleaseMutexSem(h);
	if (ret) {
	    message(LOG_ERR,"Fail to release mutex err=%d",ret);
	}
    }
}
#else	/* ! OS2 & ! WINDOWS */
#define waitMutex(sem)	/* */
#define freeMutex(sem)	/* */
#endif
#endif

/* relay UDP */

void message_origin(origin)
Origin *origin;
{
    struct sockaddr_in name;
    SOCKET sd;
    int len, i;
    char str[BUFMAX];
    strntime(str,BUFMAX,&origin->clock);
    i = strlen(str);
    if (ValidSocket(origin->sd)) {
	len = sizeof(name);
	if (getsockname(origin->sd,(struct sockaddr*)&name,&len) < 0) {
#ifdef WINDOWS
	    errno = WSAGetLastError();
#endif
	    if (Debug > 3)
		message(LOG_DEBUG,"UDP %d: Can't get socket's name err=%d",
			origin->sd,errno);
	} else {
	    strncpy(&str[i],port2str(name.sin_port,proto_udp,0),BUFMAX-i);
	    i = strlen(str);
	    if (i < BUFMAX-2) str[i++] = ' ';
	}
    }
    if (i >= BUFMAX) i = BUFMAX-1;
    str[i] = '\0';
    if (origin->stone) sd = origin->stone->sd;
    else sd = INVALID_SOCKET;
    message(LOG_INFO,"UDP%3d:%3d %s%s:%s",
	    origin->sd,sd,str,
	    addr2str(&origin->sin.sin_addr),
	    port2str(&origin->sin.sin_port,proto_udp,proto_all));
}

/* enlarge packet buffer */
static void enlarge_buf(sd)
SOCKET sd;
{
    char *buf;
    buf = malloc(pkt_len_max << 1);
    if (buf) {
	pkt_len_max = (pkt_len_max << 1);
	free(pkt_buf);
	pkt_buf = buf;
	message(LOG_INFO,"UDP %d: Packet buffer is enlarged: %d bytes",
		sd,pkt_len_max);
    }
}

static int recvUDP(sd,from)
SOCKET sd;
struct sockaddr_in *from;
{
    struct sockaddr_in sin;
    int len, pkt_len;
    if (!from) from = &sin;
    len = sizeof(*from);
    pkt_len = recvfrom(sd,pkt_buf,pkt_len_max,0,
		       (struct sockaddr*)from,&len);
    if (Debug > 4) message(LOG_DEBUG,"UDP %d: %d bytes received from %s:%s",
			   sd,pkt_len,
			   addr2str(&from->sin_addr),
			   port2str(from->sin_port,proto_udp,proto_all));
    if (pkt_len > pkt_len_max) {
	message(LOG_NOTICE,"UDP %d: recvfrom failed: larger packet (%d bytes) "
		"arrived from %s:%s",
		sd,pkt_len,
		addr2str(&from->sin_addr),
		port2str(from->sin_port,proto_udp,0));
	enlarge_buf(sd);
	pkt_len = 0;		/* drop */
    }
    return pkt_len;
}   

static int sendUDP(sd,sinp,len)
SOCKET sd;
struct sockaddr_in *sinp;
int len;
{
    if (sendto(sd,pkt_buf,len,0,
	       (struct sockaddr*)sinp,sizeof(*sinp))
	!= len) {
#ifdef WINDOWS
	errno = WSAGetLastError();
#endif
	message(LOG_ERR,"UDP %d: sendto failed err=%d: to %s:%s",
		sd,errno,
		addr2str(&sinp->sin_addr),
		port2str(sinp->sin_port,proto_udp,0));
	return -1;
    }
    if (Debug > 4)
	message(LOG_DEBUG,"UDP %d: %d bytes sent to %s:%s",
		sd,len,
		addr2str(&sinp->sin_addr),
		port2str(sinp->sin_port,proto_udp,0));
    return len;
}

static Origin *getOrigins(addr,port)
struct in_addr *addr;
int port;	/* network byte order */
{
    Origin *origin;
    for (origin=origins.next; origin != NULL; origin=origin->next) {
	if (InvalidSocket(origin->sd)) continue;
	if (origin->sin.sin_addr.s_addr == addr->s_addr
	    && origin->sin.sin_port == port) {
	    origin->lock = 1;	/* lock origin */
	    return origin;
	}
    }
    return NULL;
}

void docloseUDP(origin)
Origin *origin;
{
    if (Debug > 2) message(LOG_DEBUG,"UDP %d: close",origin->sd);
    if (ValidSocket(origin->sd)) {
	FD_CLR_RIN(origin->sd);
	FD_CLR_EIN(origin->sd);
    }
    origin->lock = -1;	/* request to close */
}

void asyncOrg(origin)
Origin *origin;
{
    int len;
    ASYNC_BEGIN;
    utimer(TICK_TIMER);
    len = recvUDP(origin->sd,NULL);
    if (Debug > 4)
	message(LOG_DEBUG,"UDP %d: send %d bytes to %d",
		origin->sd,len,origin->stone->sd);
    if (len > 0) sendUDP(origin->stone->sd,&origin->sin,len);
    utimer(0);
    time(&origin->clock);
    if (len > 0) {
	FD_SET_EIN(origin->sd);
	FD_SET_RIN(origin->sd);
    } else if (len < 0) {
	docloseUDP(origin);
    }
    ASYNC_END;
}

int scanUDP(rop,eop)
fd_set *rop, *eop;
{
    Origin *origin, *prev, *old;
    int n = 0;
    unsigned int g = Generation;
    prev = &origins;
    for (origin=origins.next; origin != NULL;
	 prev=origin, origin=origin->next) {
	if (InvalidSocket(origin->sd) || origin->lock > 0) {
	    old = origin;
	    waitMutex(OrigMutex);
	    if (prev->next == origin) {
		origin = prev;
		origin->next = old->next;	/* remove `old' from list */
		if (InvalidSocket(old->sd)) {
		    free(old);
		} else {
		    old->lock = 0;
		    old->next = origins.next;	/* insert old on top */
		    origins.next = old;
		}
	    }
	    freeMutex(OrigMutex);
	    goto next;
	}
	if (origin->lock < 0) {
	    if (!FD_ISSET(origin->sd,&rin) &&
		!FD_ISSET(origin->sd,&ein)) {
		closesocket(origin->sd);
		origin->sd = INVALID_SOCKET;
	    } else {
		FD_CLR_RIN(origin->sd);
		FD_CLR_EIN(origin->sd);
	    }
	    goto next;
	}
	if (FD_ISSET(origin->sd,eop) && FD_ISSET(origin->sd,&ein)) {
	    message(LOG_ERR,"UDP %d: exception",origin->sd);
	    message_origin(origin);
	    docloseUDP(origin);
	} else if (FD_ISSET(origin->sd,rop) && FD_ISSET(origin->sd,&rin)) {
	    FD_CLR_EIN(origin->sd);
	    FD_CLR_RIN(origin->sd);
	    ASYNC(asyncOrg,origin);
	} else {
	    if (++n >= OriginMax) docloseUDP(origin);
	}
      next:
	if (g != Generation) return 0;
    }
    return 1;
}

/* *stonep repeat UDP connection */
Origin *doUDP(stonep)
Stone *stonep;
{
    struct sockaddr_in from;
    SOCKET dsd;
    int len;
    Origin *origin;
    if ((len=recvUDP(stonep->sd,&from)) <= 0) return NULL;	/* drop */
    if (!checkXhost(stonep,&from.sin_addr)) {
	message(LOG_WARNING,"stone %d: recv UDP denied: from %s:%s",
		stonep->sd,
		addr2str(&from.sin_addr),
		port2str(from.sin_port,stonep->proto,proto_src));
	return NULL;
    }
    origin = getOrigins(&from.sin_addr,from.sin_port);
    if (origin) {
	dsd = origin->sd;
	if (Debug > 5)
	    message(LOG_DEBUG,"UDP %d: reuse %d to send",stonep->sd,dsd);
    } else if (InvalidSocket(dsd=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP))) {
#ifdef WINDOWS
	errno = WSAGetLastError();
#endif
	message(LOG_ERR,"UDP: can't create datagram socket err=%d.",
		errno);
	return NULL;
    }
    if (Debug > 4)
	message(LOG_DEBUG,"UDP %d: send %d bytes to %d",stonep->sd,len,dsd);
    if (sendUDP(dsd,&stonep->sin,len) <= 0) {
	if (origin) docloseUDP(origin);
	else closesocket(dsd);
	return NULL;
    }
    if (!origin) {
	origin = malloc(sizeof(Origin));
	if (!origin) {
	    message(LOG_ERR,"UDP %d: Out of memory, closing socket",dsd);
	    return NULL;
	}
	origin->sd = dsd;
	origin->stone = stonep;
	bcopy(&from,&origin->sin,sizeof(origin->sin));
	origin->lock = 0;
	waitMutex(OrigMutex);
	origin->next = origins.next;	/* insert origin */
	origins.next = origin;
	freeMutex(OrigMutex);
    }
    return origin;
}

void asyncUDP(stone)
Stone *stone;
{
    Origin *origin;
    ASYNC_BEGIN;
    utimer(TICK_TIMER);
    origin = doUDP(stone);
    utimer(0);
    if (origin) {
	time(&origin->clock);
	FD_SET_RIN(origin->sd);
	FD_SET_EIN(origin->sd);
    }
    ASYNC_END;
}

/* relay TCP */

void message_pair(pair)
Pair *pair;
{
    struct sockaddr_in name;
    SOCKET sd;
    Pair *p;
    int len, i;
    char str[BUFMAX];
    strntime(str,BUFMAX,&pair->clock);
    i = strlen(str);
    if (ValidSocket(pair->sd)) {
	len = sizeof(name);
	if (getsockname(pair->sd,(struct sockaddr*)&name,&len) < 0) {
#ifdef WINDOWS
	    errno = WSAGetLastError();
#endif
	    if (Debug > 3)
		message(LOG_DEBUG,"TCP %d: Can't get socket's name err=%d",
			pair->sd,errno);
	} else {
	    strncpy(&str[i],port2str(name.sin_port,pair->proto,0),BUFMAX-i);
	    i = strlen(str);
	    if (i < BUFMAX-2) str[i++] = ' ';
	}
	len = sizeof(name);
	if (getpeername(pair->sd,(struct sockaddr*)&name,&len) < 0) {
#ifdef WINDOWS
	    errno = WSAGetLastError();
#endif
	    if (Debug > 3)
		message(LOG_DEBUG,"TCP %d: Can't get peer's name err=%d",
			pair->sd,errno);
	} else {
	    strncpy(&str[i],addr2str(&name.sin_addr),BUFMAX-i);
	    i = strlen(str);
	    if (i < BUFMAX-2) str[i++] = ':';
	    strncpy(&str[i],port2str(name.sin_port,pair->proto,proto_all),
		    BUFMAX-i);
	    i = strlen(str);
	}
    }
    if (i >= BUFMAX) i = BUFMAX-1;
    str[i] = '\0';
    p = pair->pair;
    if (p) sd = p->sd;
    else sd = INVALID_SOCKET;
    if (p && p->p) {
	message(LOG_INFO,"TCP%3d:%3d %08x %d %s %s",
		pair->sd,sd,pair->proto,pair->count,str,p->p);
    } else {
	message(LOG_INFO,"TCP%3d:%3d %08x %d %s",
		pair->sd,sd,pair->proto,pair->count,str);
    }
}

#ifdef USE_SSL
static void printSSLinfo(ssl)
SSL *ssl;
{
    X509 *peer;
    char *p = (char *)SSL_get_cipher(ssl);
    if (p == NULL) p = "<NULL>";
    message(LOG_INFO,"[SSL cipher=%s]",p);
    peer = SSL_get_peer_certificate(ssl);
    if (peer) {
	p = X509_NAME_oneline(X509_get_subject_name(peer),NULL,0);
	if (p) message(LOG_INFO,"[SSL subject=%s]",p);
	free(p);
	p = X509_NAME_oneline(X509_get_issuer_name(peer),NULL,0);
	if (p) message(LOG_INFO,"[SSL issuer=%s]",p);
	free(p);
	X509_free(peer);
    }
}

int trySSL_accept(pair)
Pair *pair;
{
    int ret;
    unsigned long err;
    if (pair->proto & proto_ssl_intr) {
	if (SSL_want_nothing(pair->ssl)) {
	    pair->proto &= ~proto_ssl_intr;
	    return 1;
	}
    }
    ret = SSL_accept(pair->ssl);
    if (Debug > 4)
	message(LOG_DEBUG,"TCP %d: SSL_accept ret=%d, state=%x, "
		"finished=%x, in_init=%x/%x",
		pair->sd,ret,
		SSL_state(pair->ssl),
		SSL_is_init_finished(pair->ssl),
		SSL_in_init(pair->ssl),
		SSL_in_accept_init(pair->ssl));
    if (ret < 0) {
	err = ERR_get_error();
	if (err) {
	    message(LOG_ERR,"TCP %d: SSL_accept error err=%d",pair->sd,err);
	    if (ssl_verbose_flag)
		message(LOG_INFO,"TCP %d: %s",
			pair->sd,ERR_error_string(err,NULL));
	    message_pair(pair);
	    SSL_free(pair->ssl);
	    pair->ssl = NULL;
	    return -1;
	}
	if (Debug > 4)
	    message(LOG_DEBUG,"TCP %d: SSL_accept interrupted",pair->sd);
	pair->proto |= proto_ssl_intr;
	return 0;	/* EINTR */
    }
    if (SSL_in_accept_init(pair->ssl)) {
	message(LOG_NOTICE,"TCP %d: SSL_accept unexpected EOF",pair->sd);
	message_pair(pair);
	SSL_free(pair->ssl);
	pair->ssl = NULL;
	return -1;	/* unexpected EOF */
    }
    pair->proto &= ~proto_ssl_intr;
    pair->proto |= proto_connect;
    return 1;
}

int doSSL_accept(pair)
Pair *pair;
{
    int ret;
    pair->ssl = SSL_new(ssl_ctx_server);
    SSL_set_fd(pair->ssl,pair->sd);
    if (!SSL_use_RSAPrivateKey_file(pair->ssl,keyfile,X509_FILETYPE_PEM)) {
	message(LOG_ERR,"SSL_use_RSAPrivateKey_file(%s) error",keyfile);
	if (ssl_verbose_flag)
	    message(LOG_INFO,"%s",ERR_error_string(ERR_get_error(),NULL));
	SSL_free(pair->ssl);
	pair->ssl = NULL;
	return -1;
    }
    if (!SSL_use_certificate_file(pair->ssl,certfile,X509_FILETYPE_PEM)) {
	message(LOG_ERR,"SSL_use_certificate_file(%s) error",certfile);
	if (ssl_verbose_flag)
	    message(LOG_INFO,"%s",ERR_error_string(ERR_get_error(),NULL));
	SSL_free(pair->ssl);
	pair->ssl = NULL;
	return -1;
    }
    if (cipher_list) SSL_set_cipher_list(pair->ssl,cipher_list);
    SSL_set_verify(pair->ssl,ssl_verify_flag,NULL);
    ret = trySSL_accept(pair);
    return ret;
}

int doSSL_connect(pair)
Pair *pair;
{
    unsigned long err;
    if (!(pair->proto & proto_ssl_intr)) {
	pair->ssl = SSL_new(ssl_ctx_client);
	SSL_set_fd(pair->ssl,pair->sd);
	if (cipher_list) SSL_set_cipher_list(pair->ssl,cipher_list);	    
	SSL_set_verify(pair->ssl,ssl_verify_flag,NULL);
    } else {
	if (SSL_want_nothing(pair->ssl)) {
	    pair->proto |= proto_connect;
	    return 1;
	}
    }
    if (SSL_connect(pair->ssl) < 0) {
	err = ERR_get_error();
	if (err) {
	    message(LOG_ERR,"TCP %d: SSL_connect error err=%d",pair->sd,err);
	    if (ssl_verbose_flag)
		message(LOG_INFO,"TCP %d: %s",
			pair->sd,ERR_error_string(err,NULL));
	    message_pair(pair);
	    SSL_free(pair->ssl);
	    pair->ssl = NULL;
	    return -1;
	}
	if (Debug > 4)
	    message(LOG_DEBUG,"TCP %d: SSL_connect interrupted",pair->sd);
	return 0;	/* EINTR */
    }
    return 1;
}
#endif	/* USE_SSL */

/* close pair */
void doclose(pair)
Pair *pair;
{
    Pair *p = NULL;
    SOCKET sd;
#ifdef USE_SSL
    if (pair->ssl) {
	SSL_free(pair->ssl);
	pair->ssl = NULL;
    }
#endif
    if (pair->p) {
	free(pair->p);
	pair->p = NULL;
    }
    if (!(pair->proto & proto_close)) {
	pair->proto |= proto_close;	/* request to close */
	if (ValidSocket(pair->sd)) {
	    p = pair->pair;
	    sd = pair->sd;
	    if (Debug > 2) message(LOG_DEBUG,"TCP %d: close",sd);
	    pair->proto &= ~proto_ready_r;
	    FD_CLR_RIN(sd);
	    pair->proto &= ~proto_ready_w;
	    FD_CLR_WIN(sd);
	    FD_CLR_EIN(sd);
	}
    }
    if (p && !(p->proto & proto_close)) {
	if (Debug > 2)
	    message(LOG_DEBUG,"TCP %d: request to shutdown %d",sd,p->sd);
	p->proto |= proto_shutdown;	/* request to shutdown */
	if (ValidSocket(p->sd)
	    && !(p->proto & proto_connect)) {	/* not yet connected */
#ifdef USE_SSL
	    if (p->ssl) {
		SSL_free(p->ssl);
		p->ssl = NULL;
	    }
#endif
	}
    }
}

/* pair connect to destination */
int doconnect(pair,sinp)
Pair *pair;
struct sockaddr_in *sinp;	/* connect to */
{
    int ret;
    Pair *p = pair->pair;
    if (!(pair->proto & proto_ssl_intr)) {
	ret = connect(pair->sd,(struct sockaddr*)sinp,sizeof(*sinp));
	if (pair->proto & proto_close) return -1;
	if (ret < 0) {
#ifdef WINDOWS
	    errno = WSAGetLastError();
#endif
	    if (errno == EINTR) {
		if (Debug > 4)
		    message(LOG_DEBUG,"TCP %d: connect interrupted",pair->sd);
		return 0;
	    } else if (errno == EISCONN || errno == EADDRINUSE) {
		if (Debug > 4) {	/* SunOS's bug ? */
		    message(LOG_INFO,"TCP %d: connect bug err=%d",
			    pair->sd,errno);
		    message_pair(pair);
		}
	    } else {
		message(LOG_ERR,"TCP %d: can't connect err=%d: to %s:%s",
			pair->sd,
			errno,
			addr2str(&sinp->sin_addr),
			port2str(sinp->sin_port,pair->proto,proto_all));
		return -1;
	    }
	}
	if (Debug > 2)
	    message(LOG_DEBUG,"TCP %d: established to %d",
		    p->sd,pair->sd);
    }
    ret = 1;
#ifdef USE_SSL
    if (pair->proto & proto_ssl) {
	ret = doSSL_connect(pair);
	if (ret == 0) {	/* EINTR */
	    pair->proto |= proto_ssl_intr;
	} else if (ret > 0) {
	    pair->proto &= ~proto_ssl_intr;
	}
    }
#endif
    if (ret > 0) pair->proto |= proto_connect;
    return ret;
}

void message_conn(conn)
Conn *conn;
{
    SOCKET sd = INVALID_SOCKET;
    Pair *p1, *p2;
    int proto = 0;
    int i = 0;
    char str[BUFMAX];
    p1 = conn->pair;
    if (p1) {
	p2 = p1->pair;
	strntime(str,BUFMAX,&p1->clock);
	i = strlen(str);
	proto = p1->proto;
	if (p2) sd = p2->sd;
    }
    strncpy(&str[i],addr2str(&conn->sin.sin_addr),BUFMAX-i);
    i = strlen(str);
    if (i < BUFMAX-2) str[i++] = ':';
    strncpy(&str[i],port2str(conn->sin.sin_port,proto,proto_all),BUFMAX-i);
    i = strlen(str);
    if (i >= BUFMAX) i = BUFMAX-1;
    str[i] = '\0';
    message(LOG_INFO,"Conn %d: %08x %s",sd,proto,str);
}

/* request pair to connect to destination */
int reqconn(pair,sinp)
Pair *pair;
struct sockaddr_in *sinp;	/* connect to */
{
    Conn *conn;
    Pair *p = pair->pair;
    if ((pair->proto & proto_command) == command_proxy) {
	if (p) {
	    p->proto &= ~proto_ready_r;
	    FD_SET_RIN(p->sd);	/* must read request header */
	}
	return 0;
    }
    conn = malloc(sizeof(Conn));
    if (!conn) {
	message(LOG_ERR,"TCP %d: out of memory",(p ? p->sd : -1));
	return -1;
    }
    time(&pair->clock);
    pair->count++;	/* request to connect */
    conn->pair = pair;
    conn->sin = *sinp;
    conn->lock = 0;
    waitMutex(ConnMutex);
    conn->next = conns.next;
    conns.next = conn;
    freeMutex(ConnMutex);
    return 0;
}

void asyncConn(conn)
Conn *conn;
{
    int ret;
    Pair *p1, *p2;
    time_t clock;
    ASYNC_BEGIN;
    p1 = conn->pair;
    if (p1 == NULL) goto finish;
    p2 = p1->pair;
    if (p2 == NULL) goto finish;
    time(&clock);
    utimer(TICK_TIMER);
#ifdef USE_SSL
    if (p2->proto & proto_ssl_intr)
	ret = trySSL_accept(p2);	/* accept not completed */
    else ret = 1;
    if (ret > 0)
#endif
	ret = doconnect(p1,&conn->sin);
    utimer(0);
    if (ret == 0) {	/* EINTR */
	if (clock - p1->clock < CONN_TIMEOUT) {
	    conn->lock = 0;	/* unlock conn */
	    return;
	}
	message(LOG_ERR,"TCP %d: connect timeout to %s:%s",
		p2->sd,
		addr2str(&conn->sin.sin_addr),
		port2str(conn->sin.sin_port,p1->proto,proto_all));
	ret = -1;
    }
    if (ret < 0		/* fail to connect */
	|| (p1->proto & proto_close)
	|| (p2->proto & proto_close)) {	
	doclose(p2);
	doclose(p1);
    } else {	/* success to connect */
	if (p1->len > 0) {
	    p1->proto &= ~proto_ready_w;
	    FD_SET_WIN(p1->sd);
	} else {
	    p2->proto &= ~proto_ready_r;
	    FD_SET_RIN(p2->sd);
	}
	if (!(p2->proto & proto_ohttp)) {
	    p1->proto &= ~proto_ready_r;
	    FD_SET_RIN(p1->sd);
	}
	FD_SET_EIN(p2->sd);
	FD_SET_EIN(p1->sd);
    }
 finish:
    if (p1) p1->count--;	/* no more request to connect */
    conn->pair = NULL;
    conn->lock = -1;
    ASYNC_END;
}

/* scan conn request */
int scanConns() {
    Conn *conn, *pconn;
    Pair *p1, *p2;
    unsigned int g = Generation;
    pconn = &conns;
    for (conn=conns.next; conn != NULL; conn=conn->next) {
	p1 = conn->pair;
	if (p1) p2 = p1->pair;
	if (p1 && !(p1->proto & proto_close) &&
	    p2 && !(p2->proto & proto_close)) {
	    if (conn->lock == 0) {
		conn->lock = 1;		/* lock conn */
		if (Debug > 4) message_conn(conn);
		ASYNC(asyncConn,conn);
	    }
	} else {
	    waitMutex(ConnMutex);
	    if (pconn->next == conn) {
		pconn->next = conn->next;	/* remove conn */
		free(conn);
		conn = pconn;
	    }
	    freeMutex(ConnMutex);
	}
	pconn = conn;
	if (g != Generation) return 0;
    }
    return 1;
}

/* *stonep accept connection */
Pair *doaccept(stonep)
Stone *stonep;
{
    struct sockaddr_in from;
    SOCKET nsd;
    int len;
    Pair *pair1, *pair2;
    int ret;
    nsd = INVALID_SOCKET;
    pair1 = pair2 = NULL;
    len = sizeof(from);
    nsd = accept(stonep->sd,(struct sockaddr*)&from,&len);
    if (InvalidSocket(nsd)) {
#ifdef WINDOWS
	errno = WSAGetLastError();
#endif
	if (errno == EINTR) {
	    if (Debug > 4)
		message(LOG_DEBUG,"stone %d: accept interrupted",stonep->sd);
	    return NULL;
	}
	message(LOG_ERR,"stone %d: accept error err=%d.",stonep->sd,errno);
	return NULL;
    }
    if (!checkXhost(stonep,&from.sin_addr)) {
	message(LOG_WARNING,"stone %d: access denied: from %s:%s",
		stonep->sd,
		addr2str(&from.sin_addr),
		port2str(from.sin_port,stonep->proto,proto_src));
	shutdown(nsd,2);
	closesocket(nsd);
	return NULL;
    }
    if (AccFp) {
	char buf[BUFMAX], str[STRMAX];
	time_t clock;
	time(&clock);
	fprintf(AccFp,"%s%d[%d] %s[%s]%d\n",
		strntime(buf,BUFMAX,&clock),
		stonep->port,stonep->sd,
		addr2str(&from.sin_addr),
		addr2ip(&from.sin_addr,str),
		ntohs(from.sin_port));
    }
    if (Debug > 1) {
	message(LOG_DEBUG,"stone %d: accepted TCP %d from %s:%s",
		stonep->sd,
		nsd,
		addr2str(&from.sin_addr),
		port2str(from.sin_port,stonep->proto,proto_src));
    }
    pair1 = malloc(sizeof(Pair));
    pair2 = malloc(sizeof(Pair));
    if (!pair1 || !pair2) {
	message(LOG_ERR,"stone %d: out of memory, closing TCP %d",
		stonep->sd,nsd);
      error:
	closesocket(nsd);
	if (pair1) free(pair1);
	if (pair2) free(pair2);
	return NULL;
    }
    pair1->sd = nsd;
    pair1->proto = ((stonep->proto & proto_src) |
		    proto_first_r | proto_first_w | proto_source);
    pair1->count = 0;
    pair1->start = 0;
    pair1->p = NULL;
    time(&pair1->clock);
    pair1->pair = pair2;
    pair2->sd = socket(PF_INET,SOCK_STREAM,0);
    if (InvalidSocket(pair2->sd)) {
#ifdef WINDOWS
	errno = WSAGetLastError();
#endif
	message(LOG_ERR,"TCP %d: can't create socket err=%d.",pair1->sd,errno);
	goto error;
    }
    pair2->proto = ((stonep->proto & proto_dest) |
		    proto_first_r | proto_first_w);
    pair2->count = 0;
    pair2->start = 0;
    pair2->p = NULL;
    time(&pair2->clock);
    pair2->pair = pair1;
    pair1->len = pair2->len = 0;
    ret = 1;
#ifdef USE_SSL
    pair1->ssl = pair2->ssl = NULL;
    if (stonep->proto & proto_ssl_s) {
	ret = doSSL_accept(pair1);
	if (ret < 0) {
	    closesocket(nsd);
	    free(pair1);
	    free(pair2);
	    return NULL;
	}
    }
#endif
    if (ret > 0) pair1->proto |= proto_connect;
    return pair1;
}

void asyncAccept(stone)
Stone *stone;
{
    Pair *p1, *p2;
    ASYNC_BEGIN;
    utimer(TICK_TIMER);
    p1 = doaccept(stone);
    utimer(0);
    if (p1 == NULL) {
	ASYNC_END;
	return;
    }
    p2 = p1->pair;
    p1->next = p2;	/* link pair each other */
    p2->prev = p1;
    if ((p2->proto & proto_command) == command_ihead) {
	sprintf(p2->buf,"%s\r%c",stone->p,'\n');
	p2->start = strlen(p2->buf);
    }
    if (p2->proto & proto_ohttp) {
	sprintf(p2->buf,
		"%s\r%c"
		"\r%c",
		stone->p,'\n','\n');
	p2->len = strlen(p2->buf);
    }
#ifdef USE_IPF
    /* transparent illusion by IPFilter by <fukachan@fml.org>:
     * replace &stone->sin (host we try to connect to) with real destination. 
     */
    if (TransparentMode) {
      ipfilter_get_real_dst(p1->sd, &stone->sin);
      message(LOG_DEBUG, "dst=%s", inet_ntoa(stone->sin.sin_addr));
    }
#endif
    if (reqconn(p2,&stone->sin) < 0) {
	if (ValidSocket(p2->sd)) closesocket(p2->sd);
	if (ValidSocket(p1->sd)) closesocket(p1->sd);
	p1->pair = p2->pair = NULL;
	doclose(p2);
	doclose(p1);
	free(p2);
	free(p1);
	ASYNC_END;
	return;
    }
    waitMutex(PairMutex);
    p2->next = pairs.next;	/* insert pair */
    if (pairs.next != NULL) pairs.next->prev = p2;
    p1->prev = &pairs;
    pairs.next = p1;
    freeMutex(PairMutex);
    if (Debug > 4) {
	message(LOG_DEBUG,"TCP %d: pair %d inserted",p1->sd,p2->sd);
	message_pair(p1);
    }
    ASYNC_END;
}

/* scan close request */
int scanClose() {
    Pair *p1, *p2, *old;
    for (p1=pairs.next; p1 != NULL; p1=p1->next) {
	if (InvalidSocket(p1->sd)) {
	    old = p1;
	    waitMutex(PairMutex);
	    p2 = p1->pair;
	    if (p2) {
	      p2->pair = NULL;
	      doclose(p2);
	    }
	    p1 = p1->prev;
	    p1->next = old->next;	/* remove `old' from list */
	    if (p1->next != NULL) p1->next->prev = p1;
	    freeMutex(PairMutex);
	    free(old);
	} else if (p1->proto & proto_close) {
	    if (!FD_ISSET(p1->sd,&rin) &&
		!FD_ISSET(p1->sd,&win) &&
		!FD_ISSET(p1->sd,&ein) &&
		p1->count <= 0) {
		closesocket(p1->sd);
		p1->sd = INVALID_SOCKET;
	    } else {
		p1->proto &= ~proto_ready_r;
		FD_CLR_RIN(p1->sd);
		p1->proto &= ~proto_ready_w;
		FD_CLR_WIN(p1->sd);
		FD_CLR_EIN(p1->sd);
	    }
	} else if ((p1->proto & proto_shutdown) && p1->len == 0) {
	    if (Debug > 2) message(LOG_DEBUG,"TCP %d: shutdown",p1->sd);
	    shutdown(p1->sd,2);
	    p1->proto &= ~proto_shutdown;
	}
    }
    return 1;
}

void message_buf(pair,len,str)	/* dump for debug */
Pair *pair;
int len;
char *str;
{
    int i, j, k, l;
    char buf[BUFMAX];
    Pair *p = pair->pair;
    if (p == NULL) return;
    k = 0;
    for (i=pair->start; i < pair->start+len; i += j) {
	l = 0;
	buf[l++] = ' ';
	for (j=0; k <= j/10 && i+j < pair->start+len && l < BUFMAX-10;
	     j++) {
	    if (' ' <= pair->buf[i+j]
		&& pair->buf[i+j] <= '~')
		buf[l++] = pair->buf[i+j];
	    else {
		sprintf(&buf[l],"<%02x>",pair->buf[i+j]);
		l += strlen(&buf[l]);
		if (pair->buf[i+j] == '\n') {
		    k = 0;
		    j++;
		    break;
		}
		if (pair->buf[i+j] != '\t' && pair->buf[i+j] != '\r' &&
		    pair->buf[i+j] != '\033')
		    k++;
	    }
	}
	if (k > j/10) {
	    j = l = 0;
	    for (j=0; j < 16 && i+j < pair->start+len; j++) {
		if (' ' <= pair->buf[i+j]
		    && pair->buf[i+j] <= '~')
		    sprintf(&buf[l]," %c ",pair->buf[i+j]);
		else {
		    sprintf(&buf[l]," %02x",
			    (unsigned char)pair->buf[i+j]);
		    if (pair->buf[i+j] == '\n') k = 0; else k++;
		}
		l += strlen(&buf[l]);
	    }
	}
	buf[l] = '\0';
	if (pair->proto & proto_source) {
	    message(LOG_DEBUG,"%s%d<%d%s",str,pair->sd,p->sd,buf);
	} else {
	    message(LOG_DEBUG,"%s%d>%d%s",str,p->sd,pair->sd,buf);
	}
    }
}

int dowrite(pair)	/* write from buf from pair->start */
Pair *pair;
{
    int len;
    if (Debug > 5) message(LOG_DEBUG,"TCP %d: write ...",pair->sd);
    FD_CLR_WIN(pair->sd);
#ifdef USE_SSL
    if (pair->ssl) {
	len = SSL_write(pair->ssl,&pair->buf[pair->start],pair->len);
	if (pair->proto & proto_close) return -1;
	if (len < 0) {
	    unsigned long err;
	    err = ERR_get_error();
	    if (!err) {
		if (Debug > 4)
		    message(LOG_DEBUG,"TCP %d: SSL_write interrupted",
			    pair->sd);
		return 0;	/* EINTR */
	    }
	    message(LOG_ERR,"TCP %d: SSL_write error err=%d, closing",
		    pair->sd,err);
	    message_pair(pair);
	    return len;	/* error */
	}
	if (ssl_verbose_flag &&
	    (pair->proto & proto_first_r) &&
	    (pair->proto & proto_first_w)) printSSLinfo(pair->ssl);
    } else {
#endif
	len = send(pair->sd,&pair->buf[pair->start],pair->len,0);
	if (pair->proto & proto_close) return -1;
	if (len < 0) {
#ifdef WINDOWS
	    errno = WSAGetLastError();
#endif
	    if (errno == EINTR) {
		if (Debug > 4)
		    message(LOG_DEBUG,"TCP %d: write interrupted",pair->sd);
		return 0;
	    }
	    message(LOG_ERR,"TCP %d: write error err=%d, closing",
		    pair->sd,errno);
	    message_pair(pair);
	    return len;	/* error */
	}
#ifdef USE_SSL
    }
#endif
    if (Debug > 4) message(LOG_DEBUG,"TCP %d: %d bytes written",pair->sd,len);
    if (Debug > 7 || ((pair->proto & proto_first_w) && Debug > 3))
	message_buf(pair,len,"");
    time(&pair->clock);
    if (pair->len <= len) {
	pair->start = 0;
    } else {
	pair->start += len;
	message(LOG_NOTICE,
		"TCP %d: write %d bytes, but only %d bytes written",
		pair->sd,pair->len,len);
	message_pair(pair);
    }
    pair->len -= len;
    return len;
}

static unsigned char basis_64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int baseEncode(buf,len)
unsigned char *buf;
int len;
{
    unsigned char bbuf[BUFMAX];
    unsigned char c1, c2, c3;
    int blen = 0;
    int i;
    for (i=0; i < len; i += 3) {
	switch (len - i) {
	case 1:
	    c2 = '\0';
	    bbuf[blen+2] = '=';
	case 2:
	    c3 = '\0';
	    bbuf[blen+3] = '=';
	}
	switch (len - i) {
	default:
	    c3 = buf[i+2];
	    bbuf[blen+3] = basis_64[c3 & 0x3F];
	case 2:
	    c2 = buf[i+1];
	    bbuf[blen+2] = basis_64[((c2 & 0xF) << 2) | ((c3 & 0xC0) >>6)];
	case 1:
	    c1 = buf[i];
	    bbuf[blen+1] = basis_64[((c1 & 0x3)<< 4) | ((c2 & 0xF0) >> 4)];
	    bbuf[blen] = basis_64[c1>>2];
	}
	blen += 4;
    }
    if (bbuf[blen-1] != '=') bbuf[blen++] = '=';
    strncpy(buf,bbuf,blen);
    return blen;
}

#define XX      255	/* illegal base64 char */
#define EQ      254	/* padding */

static unsigned char index_64[256] = {
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,62, XX,XX,XX,63,
    52,53,54,55, 56,57,58,59, 60,61,XX,XX, XX,EQ,XX,XX,
    XX, 0, 1, 2,  3, 4, 5, 6,  7, 8, 9,10, 11,12,13,14,
    15,16,17,18, 19,20,21,22, 23,24,25,XX, XX,XX,XX,XX,
    XX,26,27,28, 29,30,31,32, 33,34,35,36, 37,38,39,40,
    41,42,43,44, 45,46,47,48, 49,50,51,XX, XX,XX,XX,XX,

    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
};

int baseDecode(buf,len,rest)
unsigned char *buf;
int len;
unsigned char *rest;
{
    int blen = 0;
    unsigned char c[4], o[4];
    int i, j;
    j = 0;
    for (i=0; i < len; i++) {
	c[j] = index_64[buf[i]];
	if (c[j] == XX) continue;
	if (j == 0 && c[j] == EQ) continue;
	o[j++] = buf[i];
	if (j == 4) {
	    j = 0;
	    buf[blen++] = (c[0] << 2) | ((c[1] & 0x30) >> 4);
	    if (c[2] == EQ) continue;
	    buf[blen++] = ((c[1] & 0x0F) << 4) | ((c[2] & 0x3C) >> 2);
	    if (c[3] == EQ) continue;
	    buf[blen++] = ((c[2] & 0x03) << 6) | c[3];
	}
    }
    *rest = (unsigned char)j;
    for (i=0; i < j; i++) *(rest-1-i) = o[i];
    return blen;
}

int doread(pair)	/* read into buf from pair->pair->start */
Pair *pair;
{
    Pair *p;
    int len, i;
    char buf[BUFMAX];
    int bufmax, start;
    if (Debug > 5) message(LOG_DEBUG,"TCP %d: read ...",pair->sd);
    FD_CLR_RIN(pair->sd);
    p = pair->pair;
    if (p == NULL) {	/* no pair, no more read */
#ifdef USE_SSL
	if (pair->ssl)
	    len = SSL_read(pair->ssl,buf,BUFMAX);
	else
#endif
	    len = recv(pair->sd,buf,BUFMAX,0);
	if (pair->proto & proto_close) return -1;
	if (Debug > 4) message(LOG_DEBUG,"TCP %d: read %d bytes",pair->sd,len);
	if (len == 0) return -1;	/* EOF */
	if (len > 0) {
	    message(LOG_ERR,"TCP %d: no pair, closing",pair->sd);
	    message_pair(pair);
	    len = -1;
	}
	return len;
    }
    bufmax = BUFMAX - p->start;
    start = p->start;
    if (p->proto & proto_base) bufmax = (bufmax - 1) / 4 * 3;
    else if (pair->proto & proto_base) {
	if (!(pair->proto & proto_first_r)) {
	    len = p->buf[BUFMAX-1];
	    for (i=0; i < len; i++) {
		p->buf[start++] = p->buf[BUFMAX-2-i];
	    }
	    bufmax -= len;
	}
	p->buf[BUFMAX-1] = 0;
	bufmax -= 5;
    }
#ifdef USE_SSL
    if (pair->ssl) {
	len = SSL_read(pair->ssl,&p->buf[start],bufmax);
	if (pair->proto & proto_close) return -1;
	if (len < 0) {
	    unsigned long err;
	    err = ERR_get_error();
	    if (!err) {
		if (Debug > 4)
		    message(LOG_DEBUG,"TCP %d: SSL_read interrupted",pair->sd);
		return 0;	/* EINTR */
	    }
	    message(LOG_ERR,"TCP %d: SSL_read error err=%d, closing",
		    pair->sd,err);
	    message_pair(pair);
	    doclose(p);	/* close pair */
	    return len;	/* error */
	}
	if (ssl_verbose_flag &&
	    (pair->proto & proto_first_r) &&
	    (pair->proto & proto_first_w)) printSSLinfo(pair->ssl);
    } else {
#endif
	len = recv(pair->sd,&p->buf[start],bufmax,0);
	if (pair->proto & proto_close) return -1;
	if (len < 0) {
#ifdef WINDOWS
	    errno = WSAGetLastError();
#endif
	    if (errno == EINTR) {
		if (Debug > 4)
		    message(LOG_DEBUG,"TCP %d: read interrupted",pair->sd);
		return 0;	/* EINTR */
	    }
	    message(LOG_ERR,"TCP %d: read error err=%d, closing",
		    pair->sd,errno);
	    message_pair(pair);
	    doclose(p);	/* close pair */
	    return len;	/* error */
	}
#ifdef USE_SSL
    }
#endif
    if (len == 0) return -1;	/* EOF */
    p->len = start + len - p->start;
    if (Debug > 4) {
	if (start > p->start) {
	    message(LOG_DEBUG,"TCP %d: read %d+%d bytes to %d",
		    pair->sd,len,start - p->start,p->sd);
	} else {
	    message(LOG_DEBUG,"TCP %d: read %d bytes to %d",
		    pair->sd,p->len,p->sd);
	}
    }
    time(&pair->clock);
    if (p->proto & proto_base) {
	p->len = baseEncode(&p->buf[p->start],p->len);
    } else if (pair->proto & proto_base) {
	p->len = baseDecode(&p->buf[p->start],p->len,&p->buf[BUFMAX-1]);
	if (Debug > 4 && p->buf[BUFMAX-1]) {
	    for (i=0; i < p->buf[BUFMAX-1]; i++) buf[i] = p->buf[BUFMAX-2-i];
	    buf[i] = '\0';
	    message(LOG_DEBUG,"TCP %d: save %d bytes (\"%s\")",
		    pair->sd,p->buf[BUFMAX-1],buf);
	}
    }
    return p->len;
}

/* http */

#define METHOD_LEN_MAX	10

int commOutput(Pair *pair, char *fmt, ...) {
    Pair *p = pair->pair;
    char *str;
    va_list ap;
    if (p == NULL) return -1;
    str = &p->buf[p->start + p->len];
    va_start(ap,fmt);
    vsnprintf(str,BUFMAX - (p->start + p->len), fmt, ap);
    va_end(ap);
    if (p->proto & proto_base) p->len += baseEncode(str,strlen(str));
    else p->len += strlen(str);
    p->proto &= ~proto_ready_w;
    FD_SET_WIN(p->sd);
    return p->len;
}

int doproxy(pair,host,port)
Pair *pair;
char *host;
int port;
{
    struct sockaddr_in sin;
    short family;
    bzero((char *)&sin,sizeof(sin)); /* clear sin struct */
    sin.sin_port = htons((u_short)port);
    if (!host2addr(host,&sin.sin_addr,&family)) {
	return -1;
    }
    sin.sin_family = family;
    pair->proto &= ~proto_command;
    return reqconn(pair,&sin);
}

int proxyCONNECT(Pair *pair, char *parm, int start) {
    int port = 443;	/* host byte order */
    char *p = parm;
    message(LOG_INFO,": CONNECT %s",parm);
    while (*p) {
	if (isspace(*p)) {
	    *p = '\0';
	    break;
	}
	if (*p == ':') {
	    port = atoi(p+1);
	    *p = '\0';
	}
	p++;
    }
    pair->len += pair->start;
    pair->start = 0;
    pair->pair->proto |= proto_ohttp;	/* remove request header */
    return doproxy(pair,parm,port);
}

int proxyCommon(pair,parm,start)
Pair *pair;
char *parm;
int start;
{
    int port = 80;
    char *host;
    char *top = &pair->buf[start];
    char *p, *q;
    int i;
    for (i=0; i < METHOD_LEN_MAX; i++) {
	if (parm[i] == ':') break;
    }
    if (strncmp(parm,"http",i) != 0
	|| parm[i+1] != '/' || parm[i+2] != '/') {
	message(LOG_ERR,"Unknown URL format: %s",parm);
	return -1;
    }
    host = &parm[i+3];
    p = host;
    while (*p) {
	if (*p == ':') {
	    port = atoi(p+1);
	    *p++ = '\0';
	    continue;
	}
	if (isspace(*p) || *p == '/') {
	    *p = '\0';
	    break;
	}
	p++;
    }
    i = p - parm;		/* length of 'http://host' */
    p = top;
    while (!isspace(*p)) p++;	/* skip 'GET http://host' */
    while (isspace(*p)) p++;	/* now p points url */
    q = p + i;			/* now q points path */
    if (*q != '/') *--q = '/';
    i = p - top;		/* length of 'GET ' */
    bcopy(top,q-i,i);
    pair->len += (pair->start - (q - top - i));
    pair->start = q - pair->buf - i;
    if (Debug > 1) {
	message(LOG_DEBUG,"proxy %d -> http://%s:%d",
		pair->pair->sd,host,port);
    }
    return doproxy(pair,host,port);
}

int proxyGET(Pair *pair, char *parm, int start) {
    message(LOG_INFO,": GET %s",parm);
    return proxyCommon(pair,parm,start);
}

int proxyPOST(Pair *pair, char *parm, int start) {
    message(LOG_INFO,": POST %s",parm);
    return proxyCommon(pair,parm,start);
}

int proxyErr(Pair *pair, char *parm, int start) {
    message(LOG_ERR,"Unknown method: %s",parm);
    return -1;
}

Comm proxyComm[] = {
    { "CONNECT", proxyCONNECT },
    { "POST", proxyPOST },
    { "GET", proxyGET },
    { NULL, proxyErr },
};

#ifdef USE_POP
int popUSER(Pair *pair, char *parm, int start) {
    int ulen, tlen;
    message(LOG_INFO,": USER %s",parm);
    ulen = strlen(parm);
    tlen = strlen(pair->p);
    if (ulen + 1 + tlen + 1 >= BUFMAX) {
	commOutput(pair,"+Err Too long user name\r\n");
	return -1;
    }
    bcopy(pair->p, pair->p + ulen + 1, tlen + 1);
    strcpy(pair->p, parm);
    commOutput(pair,"+OK Password required for %s\r\n",parm);
    pair->proto &= ~state_mask;
    pair->proto |= 1;
    return -2;	/* read more */
}

#define DIGEST_LEN 16

int popPASS(Pair *pair, char *parm, int start) {
    MD5_CTX context;
    unsigned char digest[DIGEST_LEN];
    char *str;
    int ulen, tlen, plen, i;
    int state = (pair->proto & state_mask);
    if (Debug > 5) message(LOG_DEBUG,": PASS %s",parm);
    if (state < 1) {
	commOutput(pair,"-ERR USER first\r\n");
	return -2;	/* read more */
    }
    ulen = strlen(pair->p);
    str = pair->p + ulen + 1;
    tlen = strlen(str);
    plen = strlen(parm);
    if (ulen + 1 + tlen + plen + 1 >= BUFMAX) {
	commOutput(pair,"+Err Too long password\r\n");
	return -1;
    }
    strcat(str, parm);
    sprintf(pair->buf,"APOP %s ",pair->p);
    ulen = strlen(pair->buf);
    MD5Init(&context);
    MD5Update(&context, str, tlen + plen);
    MD5Final(digest, &context);
    free(pair->p);
    pair->p = NULL;
    for (i=0; i < DIGEST_LEN; i++) {
	sprintf(pair->buf + ulen + i*2, "%02x", digest[i]);
    }
    message(LOG_INFO,": POP -> %s",pair->buf);
    strcat(pair->buf,"\r\n");
    pair->start = 0;
    pair->len = strlen(pair->buf);
    return 0;
}

int popAUTH(Pair *pair, char *parm, int start) {
    message(LOG_INFO,": AUTH %s",parm);
    commOutput(pair,"-ERR authorization first\r\n");
    return -2;	/* read more */
}

int popAPOP(Pair *pair, char *parm, int start) {
    message(LOG_INFO,": APOP %s",parm);
    pair->len += pair->start - start;
    pair->start = start;
    return 0;
}

int popErr(Pair *pair, char *parm, int start) {
    message(LOG_ERR,"Unknown POP command: %s",parm);
    return -1;
}

Comm popComm[] = {
    { "USER", popUSER },
    { "PASS", popPASS },
    { "APOP", popAPOP },
    { "AUTH", popAUTH },
    { NULL, popErr },
};
#endif

static char *comm_match(buf,str)
char *buf;
char *str;
{
    while (*str) {
	if (toupper(*buf++) != *str++) return NULL;	/* unmatch */
    }
    if (*buf) {
	if (!isspace(*buf)) return NULL;
/*	while (isspace(*buf)) buf++;	*/
	if (*buf == ' ') buf++;
    }
    return buf;
}

int docomm(pair,comm)
Pair *pair;
Comm *comm;
{
    char buf[BUFMAX];
    char *p;
    char *q = &pair->buf[pair->start + pair->len];
    int start, i;
    for (p=&pair->buf[pair->start]; p < q; p++) {
	if (*p == '\r' || *p == '\n') break;
    }
    if (p >= q && p < &pair->buf[BUFMAX]) {
	pair->start += pair->len;
	pair->len = 0;
	return -2;	/* read more */
    }
    for (start=p-pair->buf-1; start >= 0; start--) {
	if (pair->buf[start] == '\r' || pair->buf[start] == '\n') break;
    }
    start++;
    while ((*p == '\r' || *p == '\n') && p < q) p++;
    pair->start = p - pair->buf;
    if (p < q) {
	pair->len = q - p;
    } else {
	pair->len = 0;
    }
    while (comm->str) {
	if ((q=comm_match(&pair->buf[start],comm->str)) != NULL) break;
	comm++;
    }
    if (q == NULL) q = &pair->buf[start];
    for (i=0; q < p && i < BUFMAX-1; i++) {
	if (*q == '\r' || *q == '\n') break;
	buf[i] = *q++;
    }
    buf[i] = '\0';
    return (*comm->func)(pair,buf,start);
}

int insheader(pair)	/* insert header */
Pair *pair;
{
    char buf[BUFMAX];
    int i;
    for (i=0; i < pair->len; i++) {
	if (pair->buf[pair->start+i] == '\n') break;
    }
    if (i >= pair->len) return -1;
    i++;
    bcopy(&pair->buf[pair->start],buf,i);	/* save leading header */
    bcopy(pair->buf,pair->buf+i,pair->start);	/* insert */
    bcopy(buf,pair->buf,i);			/* restore */
    pair->len += pair->start;
    pair->start = 0;
    return pair->len;
}

int rmheader(pair)	/* remove header */
Pair *pair;
{
    char *p;
    char *q = &pair->buf[pair->start+pair->len];
    int state = (pair->proto & state_mask);
    if (Debug > 3) message_buf(pair,pair->len,"rm");
    for (p=&pair->buf[pair->start]; p < q; p++) {
	if (*p == '\r') continue;
	if (*p == '\n') {
	    state++;
	    if (state >= 3) {
		p++;
		break;	/* end of header */
	    }
	} else {
	    state = 1;
	}
    }
    if (state < 3) {
	pair->len = pair->start = 0;
	pair->proto = ((pair->proto & ~state_mask) | state);
	return -2;	/* header will continue... */
    }
    pair->len = q - p;	/* remove header */
    pair->start = p - pair->buf;
    pair->proto &= ~state_mask;
    return pair->len;
}

int first_read(pair)
Pair *pair;
{
    Pair *p = pair->pair;
    int len;
    if (p == NULL) return -1;
    len = p->len;
    pair->proto &= ~proto_first_r;
    if (p->proto & proto_command) {	/* proxy */
	switch(p->proto & proto_command) {
	case command_proxy:
	    len = docomm(p,proxyComm);
	    break;
#ifdef USE_POP
	case command_pop:
	    if (p->p) len = docomm(p,popComm);
	    break;
#endif
	default:
	    ;
	}
	if (len == -2) {	/* read more */
	    if (Debug > 3) {
		message(LOG_DEBUG,"TCP %d: read more from %d",p->sd,pair->sd);
	    }
	} else if (len < 0) {
	    doclose(p);
	    doclose(pair);
	    return -1;
	} else {
	    len = p->len;
	}
    }
    if (pair->proto & proto_ohttp) {	/* over http */
	len = rmheader(p);
	if (len >= 0) {
	    if (pair->proto & proto_ohttp_s)
		commOutput(p,"HTTP/1.0 200 OK\r\n\r\n");
	}
    }
#ifdef USE_POP
    if ((pair->proto & proto_command) == command_pop) {	/* apop */
	int i;
	char *q;
	for (i=p->start; i < p->start + p->len; i++) {
	    if (p->buf[i] == '<') {	/* time stamp of APOP banner */
		q = pair->p = malloc(BUFMAX);
		if (!q) {
		    message(LOG_ERR,"TCP %d: out of memory",pair->sd);
		    break;
		}
		for (; i < p->start + p->len; i++) {
		    *q++ = p->buf[i];
		    if (p->buf[i] == '>') break;
		}
		*q = '\0';
		break;
	    }
	}
    }
#endif
    if (len <= 0 && !(pair->proto & proto_close)) {
	pair->proto &= ~proto_ready_r;
	FD_SET_RIN(pair->sd);		/* read more */
	if (len < 0) pair->proto |= proto_first_r;
    }
    return len;
}

void asyncRead(pair)
Pair *pair;
{
    Pair *p = pair->pair;
    int len;
    ASYNC_BEGIN;
    utimer(TICK_TIMER);
    len = doread(pair);
    utimer(0);
    pair->count--;
    if (p) p->count--;
    if (len < 0 || (pair->proto & proto_close) || p == NULL) {
	doclose(pair);	/* EOF or error */
    } else {
	FD_SET_EIN(pair->sd);
	if (pair->proto & proto_ready_w) {
	    pair->proto &= ~proto_ready_w;
	    FD_SET_WIN(pair->sd);
	}
	if (len > 0) {
	    if (pair->proto & proto_first_r) len = first_read(pair);
	    if (len > 0 &&
		ValidSocket(p->sd) &&
		!(pair->proto & proto_close)) {
		p->proto &= ~proto_ready_w;
		FD_SET_WIN(p->sd);
	    }
	} else {		/* EINTR */
	    pair->proto &= ~proto_ready_r;
	    FD_SET_RIN(pair->sd);
	}
    }
    ASYNC_END;
}

void asyncWrite(pair)
Pair *pair;
{
    Pair *p = pair->pair;
    int len;
    ASYNC_BEGIN;
    utimer(TICK_TIMER);
    len = dowrite(pair);
    utimer(0);
    pair->count--;
    if (p) p->count--;
    if (len < 0 || (pair->proto & proto_close)) {
	if (p) doclose(p);	/* if error, close */
	doclose(pair);
    } else {
	FD_SET_EIN(pair->sd);
	if (pair->proto & proto_ready_r) {
	    pair->proto &= ~proto_ready_r;
	    FD_SET_RIN(pair->sd);
	}
	if (pair->len <= 0) {	/* all written */
	    if (pair->proto & proto_first_w) pair->proto &= ~proto_first_w;
	    if (p && ValidSocket(p->sd) && !(pair->proto & proto_close)) {
		p->proto &= ~proto_ready_r;
		FD_SET_RIN(p->sd);
	    }
	} else {		/* EINTR */
	    pair->proto &= ~proto_ready_w;
	    FD_SET_WIN(pair->sd);
	}
    }
    ASYNC_END;
}

int scanPairs(rop,wop,eop)
fd_set *rop, *wop, *eop;
{
    Pair *pair;
    unsigned int g = Generation;
    int ret = 1;
    if (Debug > 8) message(LOG_DEBUG,"scanPairs ...");
    for (pair=pairs.next; pair != NULL; pair=pair->next) {
	if (ValidSocket(pair->sd) && !(pair->proto & proto_close)) {
	    if (FD_ISSET(pair->sd,eop) && FD_ISSET(pair->sd,&ein)) {
		message(LOG_ERR,"TCP %d: exception",pair->sd);
		message_pair(pair);
		doclose(pair);
	    } else if (FD_ISSET(pair->sd,rop) && FD_ISSET(pair->sd,&rin)) {
		FD_CLR_EIN(pair->sd);
		pair->proto &= ~proto_ready_r;
		FD_CLR_RIN(pair->sd);
		if (FD_ISSET(pair->sd,&win)) pair->proto |= proto_ready_w;
		else pair->proto &= ~proto_ready_w;
		FD_CLR_WIN(pair->sd);
		pair->count++;
		if (pair->pair) pair->pair->count++;
		ASYNC(asyncRead,pair);
	    } else if (FD_ISSET(pair->sd,wop) && FD_ISSET(pair->sd,&win)) {
		if ((pair->proto & proto_command) == command_ihead) {
		    if (insheader(pair) >= 0)		/* insert header */
			pair->proto &= ~proto_command;
		}
		FD_CLR_EIN(pair->sd);
		pair->proto &= ~proto_ready_w;
		FD_CLR_WIN(pair->sd);
		if (FD_ISSET(pair->sd,&rin)) pair->proto |= proto_ready_r;
		else pair->proto &= ~proto_ready_r;
		FD_CLR_RIN(pair->sd);
		pair->count++;
		if (pair->pair) pair->pair->count++;
		ASYNC(asyncWrite,pair);
	    } else {
		time_t clock;
		if (time(&clock), clock - pair->clock > IDLE_MAX) {
		    if (pair->count > 0 || Debug > 2) {
			message(LOG_NOTICE,"TCP %d: idle time exceeds",
				pair->sd);
			message_pair(pair);
			if (pair->count > 0) pair->count--;
		    }
		}
	    }
	}
	if (g != Generation) {
	    ret = 0;
	    break;
	}
    }
    if (Debug > 8) message(LOG_DEBUG,"scanPairs done");
    return ret;
}

/* stone */

int scanStones(rop,eop)
fd_set *rop, *eop;
{
    Stone *stone;
    unsigned int g = Generation;
    for (stone=stones; stone != NULL; stone=stone->next) {
	if (FD_ISSET(stone->sd,eop) && FD_ISSET(stone->sd,&ein)) {
	    FD_CLR_EIN(stone->sd);
	    message(LOG_ERR,"stone %d: exception",stone->sd);
	} else if (FD_ISSET(stone->sd,rop) && FD_ISSET(stone->sd,&rin)) {
	    if (stone->proto & proto_udp) {
		ASYNC(asyncUDP,stone);
	    } else {
		ASYNC(asyncAccept,stone);
	    }
	}
	if (g != Generation) return 0;
    }
    return 1;
}

void rmoldstone() {
    Stone *stone, *next;
    stone = oldstones;
    oldstones = NULL;
    for ( ; stone != NULL; stone=next) {
	next = stone->next;
	if (stone->port) {
	    FD_CLR(stone->sd,&rin);
	    FD_CLR(stone->sd,&ein);
	    closesocket(stone->sd);
	}
#ifdef USE_SSL
	if (stone->ssl) SSL_free(stone->ssl);
#endif
	free(stone);
    }
}

void rmoldconfig() {
    int i;
    for (i=0; i < OldConfigArgc; i++) {
	free(OldConfigArgv[i]);
    }
    OldConfigArgc = 0;
    free(OldConfigArgv);
    OldConfigArgv = NULL;
}

void repeater() {
    int ret;
    fd_set rout, wout, eout;
    struct timeval tv, *timeout;
    static int spin = 0;
    rout = rin;
    wout = win;
    eout = ein;
    if (Recursion > 0 || conns.next || spin > 0 || AsyncCount > 0) {
	if (AsyncCount == 0 && spin > 0) spin--;
	timeout = &tv;
	timeout->tv_sec = 0;
	timeout->tv_usec = TICK_SELECT;
    } else timeout = NULL;		/* block indefinitely */
    ret = select(FD_SETSIZE,&rout,&wout,&eout,timeout);
#ifndef NO_ALRM
    Generation++;
#endif
    if (ret > 0) {
	spin = SPIN_MAX;
	(void)(scanStones(&rout,&eout) > 0 &&
	       scanPairs(&rout,&wout,&eout) > 0 &&
	       scanUDP(&rout,&eout) > 0);
    } else if (ret < 0) {
#ifdef WINDOWS
	errno = WSAGetLastError();
#endif
	if (errno != EINTR) {
	    message(LOG_ERR,"select error err=%d",errno);
	    exit(1);
	}
    }
    scanConns();
    scanClose();
    if (oldstones) rmoldstone();
    if (OldConfigArgc) rmoldconfig();
}

int reusestone(stone)
Stone *stone;
{
    Stone *s;
    if (!oldstones) return 0;
    for (s=oldstones; s != NULL; s=s->next) {
	if (s->port == stone->port && s->proto == stone->proto) {
	    if (Debug > 5)
		message(LOG_DEBUG,"stone %d: reused port %d",s->sd,s->port);
	    stone->sd = s->sd;
	    s->port = 0;
	    return 1;
	}
    }
    return 0;
}

/* make stone */
Stone *mkstone(dhost,dport,host,port,nhosts,hosts,proto)
char *dhost;	/* destination hostname */
int dport;	/* destination port (host byte order) */
char *host;	/* listening host */
int port;	/* listening port (host byte order) */
int nhosts;	/* # of hosts to permit */
char *hosts[];	/* hosts to permit */
int proto;	/* UDP/TCP/SSL */
{
    Stone *stonep;
    struct sockaddr_in sin;
    char xhost[256], *p;
    short family;
    int i;
    stonep = calloc(1,sizeof(Stone)+sizeof(XHost)*nhosts);
    if (!stonep) {
	message(LOG_ERR,"Out of memory.");
	exit(1);
    }
    stonep->p = NULL;
    stonep->nhosts = nhosts;
    stonep->port = port;
    bzero((char *)&sin,sizeof(sin)); /* clear sin struct */
    sin.sin_family = AF_INET;
    sin.sin_port = htons((u_short)port);/* convert to network byte order */
    if (host) {
	if (!host2addr(host,&sin.sin_addr,&family)) {
	    exit(1);
	}
	sin.sin_family = family;
    }
    if ((proto & proto_command) != command_proxy) {
	if (!host2addr(dhost,&stonep->sin.sin_addr,&family)) {
	    exit(1);
	}
	stonep->sin.sin_family = family;
	stonep->sin.sin_port = htons((u_short)dport);
    }
    stonep->proto = proto;
    if (!reusestone(stonep)) {	/* recycle stone */
	if (proto & proto_udp) {
	    stonep->sd = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);/* UDP */
	} else {
	    stonep->sd = socket(AF_INET,SOCK_STREAM,0);		/* TCP */
	}
	if (InvalidSocket(stonep->sd)) {
#ifdef WINDOWS
	    errno = WSAGetLastError();
#endif
	    message(LOG_ERR,"stone %d: Can't get socket err=%d.",
		    stonep->sd,errno);
	    exit(1);
	}
	if (bind(stonep->sd,(struct sockaddr*)&sin,sizeof(sin)) < 0) {
#ifdef WINDOWS
	    errno = WSAGetLastError();
#endif
	    message(LOG_ERR,"stone %d: Can't bind err=%d.",stonep->sd,errno);
	    exit(1);
	}
	if (sin.sin_port == 0) {
	    i = sizeof(sin);
	    getsockname(stonep->sd,(struct sockaddr*)&sin,&i);
	}
	if (!(proto & proto_udp)) {	/* TCP */
	    if (listen(stonep->sd,BACKLOG_MAX) < 0) {
#ifdef WINDOWS
		errno = WSAGetLastError();
#endif
		message(LOG_ERR,"stone %d: Can't listen err=%d.",
			stonep->sd,errno);
		exit(1);
	    }
	}
    }
    for (i=0; i < nhosts; i++) {
	strcpy(xhost,hosts[i]);
	p = strchr(xhost,'/');
	if (p != NULL) {
	    *p++ = '\0';
	    if (!host2addr(p,&stonep->xhosts[i].mask,NULL)) {
		exit(1);
	    }
	} else {
	    stonep->xhosts[i].mask.s_addr = (u_long)~0;
	}
	if (!host2addr(xhost,&stonep->xhosts[i].addr,NULL)) {
	    exit(1);
	}
	if (Debug > 1) {
	    strcpy(xhost,addr2str(&stonep->xhosts[i].addr));
	    if ((proto & proto_command) == command_proxy) {
		message(LOG_DEBUG,
			"stone %d: permit %s (mask %x) to connecting to proxy",
			stonep->sd,
			xhost,
			ntohl((unsigned long)stonep->xhosts[i].mask.s_addr));
	    } else {
		message(LOG_DEBUG,
			"stone %d: permit %s (mask %x) to connecting to %s:%s",
			stonep->sd,
			xhost,
			ntohl((unsigned long)stonep->xhosts[i].mask.s_addr),
			addr2str(&stonep->sin.sin_addr),
			port2str(stonep->sin.sin_port,
				 stonep->proto,proto_dest));
	    }
	}
    }
    if ((u_long)sin.sin_addr.s_addr) {
	strcpy(xhost,addr2str(&sin.sin_addr));
	strcat(xhost,":");
    } else {
	xhost[0] = '\0';
    }
    strcpy(xhost + strlen(xhost),
	   port2str(sin.sin_port,stonep->proto,proto_src));
    if ((proto & proto_command) == command_proxy) {
	message(LOG_INFO,"stone %d: proxy <- %s",
		stonep->sd,
		xhost);
    } else {
	message(LOG_INFO,"stone %d: %s:%s <- %s",
		stonep->sd,
		addr2str(&stonep->sin.sin_addr),
		port2str(stonep->sin.sin_port,stonep->proto,proto_dest),
		xhost);
    }
    return stonep;
}

/* main */

void help(com)
char *com;
{
    message(LOG_INFO,"stone %s  http://www.gcd.org/sengoku/stone/",VERSION);
    message(LOG_INFO,"%s",
	    "Copyright(C)2001 by Hiroaki Sengoku <sengoku@gcd.org>");
#ifdef USE_SSL
    message(LOG_INFO,"%s",
	    "using " OPENSSL_VERSION_TEXT "  http://www.openssl.org/");
#endif
#ifndef NT_SERVICE
    fprintf(stderr,
	    "Usage: %s <opt>... <stone> [-- <stone>]...\n"
	    "opt:  -C <file>         ; configuration file\n"
#ifdef CPP
	    "      -P <command>      ; preprocessor for config. file\n"
#endif
	    "      -d                ; increase debug level\n"
	    "      -n                ; numerical address\n"
	    "      -u <max>          ; # of UDP sessions\n"
#ifndef NO_FORK
	    "      -f <n>            ; # of child processes\n"
#endif
#ifndef NO_SYSLOG
	    "      -l                ; use syslog\n"
#endif
	    "      -L <file>         ; write log to <file>\n"
	    "      -a <file>         ; write accounting to <file>\n"
#ifndef NO_SETUID
	    "      -o <n>            ; set uid to <n>\n"
	    "      -g <n>            ; set gid to <n>\n"
#endif
#ifndef NO_CHROOT
	    "      -t <dir>          ; chroot to <dir>\n"
#endif
#ifdef USE_SSL
	    "      -z <SSL>          ; OpenSSL option\n"
#endif
#ifdef USE_IPF
            "      -T                ; enable IPFilter based transparency\n"
#endif
            "      -N <name>         ; my name\n"
	    "stone: <display> [<xhost>...]\n"
	    "       <host>:<port> <sport> [<xhost>...]\n"
	    "       proxy <sport> [<xhost>...]\n"
	    "       <host>:<port#>/http <sport> <Request-Line> [<xhost>...]\n"
	    "       <host>:<port#>/proxy <sport> <header> [<xhost>...]\n"
	    "port:  <port#>[/udp"
#ifdef USE_SSL
	    " | /ssl"
#endif
#ifdef USE_POP
	    " | /apop"
#endif
	    " | /base]\n"
	    "sport: [<host>:]<port#>[/udp"
#ifdef USE_SSL
	    " | /ssl"
#endif
	    " | /http | /base]\n"
	    "xhost: <host>[/<mask>]\n",
	    com);
#endif
    exit(1);
}

static void skipcomment(fp)
FILE *fp;
{
    int c;
    while ((c=getc(fp)) != EOF && c != '\r' && c != '\n')	;
    while ((c=getc(fp)) != EOF && (c == '\r' || c == '\n'))	;
    if (c != EOF) ungetc(c,fp);
}

static int getvar(fp,buf,bufmax)
FILE *fp;
char *buf;
int bufmax;
{
    char var[STRMAX];
    char *val;
    int i = 0;
    int paren = 0;
    int c = getc(fp);
    if (c == EOF) {
	return 0;
    } else if (c == '{') {
	paren = 1;
    } else {
	ungetc(c,fp);
    }
    while ((c=getc(fp)) != EOF && i < STRMAX-1) {
	if (paren && c == '}') {
	    break;
	} else if (isalnum(c) || c == '_') {
	    var[i++] = c;
	} else {
	    ungetc(c,fp);
	    break;
	}
    }
    var[i] = '\0';
    if (*var == '\0') return 0;
    val = getenv(var);
    if (val == NULL) return 0;
    i = strlen(val);
    if (i > bufmax) i = bufmax;
    strncpy(buf,val,i);
    return i;
}

static int gettoken(fp,buf)
FILE *fp;
char *buf;
{
    int i = 0;
    int quote = 0;
    int c;
    while ((c=getc(fp)) != EOF) {
	if (c == '#') {
	    skipcomment(fp);
	    continue;
	}
	if (!isspace(c)) {
	    ungetc(c,fp);
	    break;
	}
    }
    while ((c=getc(fp)) != EOF && i < BUFMAX-1) {
	if (quote != '\'') {
	    if (c == '$') {
		i += getvar(fp,&buf[i],BUFMAX-1-i);
		continue;
	    }
	    if (c == '\\') {	/* escape a char */
		c = getc(fp);
		if (c == EOF) break;
	    }
	}
	if (quote) {
	    if (c == quote) {
		quote = 0;
		continue;
	    }
	} else if (c == '\'' || c == '\"') {
	    quote = c;
	    continue;
	} else if (isspace(c)) {
	    c = getc(fp);
	    if (c != ':' && c != '=') {
		ungetc(c,fp);
		break;
	    }
	} else if (c == '#') {
	    skipcomment(fp);
	    continue;
	}
	buf[i++] = c;
    }
    buf[i] = '\0';
    return i;
}

FILE *openconfig() {
    int pfd[2];
    char host[MAXHOSTNAMELEN];
#ifdef CPP
    if (CppCommand != NULL && *CppCommand != '\0') {
	if (gethostname(host,MAXHOSTNAMELEN-1) < 0) {
	    message(LOG_ERR,"gethostname err=%d",errno);
	    exit(1);
	}
	if (pipe(pfd) < 0) {
	    message(LOG_ERR,"Can't get pipe err=%d",errno);
	    exit(1);
	}
	if (!fork()) {
	    char *argv[BUFMAX/2];
	    int i = 0;
	    char buf[BUFMAX];
	    int len = 0;
	    char *p;
	    strcpy(buf,CppCommand);
	    argv[i] = "cpp";
	    while (buf[len]) {
		if (isspace(buf[len])) {
		    buf[len++] = '\0';
		    while (buf[len] && isspace(buf[len])) len++;
		    if (buf[len]) argv[++i] = &buf[len];
		    else break;
		}
		len++;
	    }
	    len++;
	    argv[++i] = buf + len;
	    snprintf(argv[i],BUFMAX-len,"-DHOST=%s",host);
	    len += strlen(argv[i]) + 1;
	    argv[++i] = buf + len;
	    for (p=host; *p; p++) if (*p == '.') *p = '_';
	    snprintf(argv[i],BUFMAX-len,"-DHOST_%s",host);
	    len += strlen(argv[i]) + 1;
	    if (getenv("HOME")) {
		argv[++i] = buf + len;
		snprintf(argv[i],BUFMAX-len,"-DHOME=%s",getenv("HOME"));
		len += strlen(argv[i]) + 1;
	    }
	    argv[++i] = ConfigFile;
	    argv[++i] = NULL;
	    close(pfd[0]);
	    close(1);
	    dup(pfd[1]);
	    close(pfd[1]);
	    if (Debug > 9) {
		char str[BUFMAX];
		snprintf(str,BUFMAX,"%s: ",buf);
		for (i=0; argv[i]; i++) {
		    len = strlen(str);
		    snprintf(&str[len],BUFMAX-len," %s",argv[i]);
		}
		message(LOG_DEBUG,"%s",str);
	    }
	    execv(buf,argv);
	}
	close(pfd[1]);
	wait(NULL);
	return fdopen(pfd[0],"r");
    } else
#endif
	return fopen(ConfigFile,"r");
}

void getconfig() {
    FILE *fp;
    int nptr = 0;
    char **new;
    char buf[BUFMAX];
    int len;
    if (ConfigFile == NULL) return;
    ConfigArgc = 0;
    ConfigArgv = NULL;
    fp = openconfig();
    if (fp == NULL) {
	message(LOG_ERR,"Can't open config file err=%d: %s",
		errno,ConfigFile);
	exit(1);
    }
    strcpy(buf,ConfigFile);
    len = strlen(buf);
    do {
	if (Debug > 9) message(LOG_DEBUG,"token: \"%s\"",buf);
	if (ConfigArgc >= nptr) {	/* allocate new ptrs */
	    new = malloc((nptr+BUFMAX)*sizeof(*ConfigArgv));
	    if (new == NULL) {
		message(LOG_ERR,"Out of memory.");
		exit(1);
	    }
	    if (ConfigArgv) {
		bcopy(ConfigArgv,new,nptr*sizeof(*ConfigArgv));
		free(ConfigArgv);
	    }
	    ConfigArgv = new;
	    nptr += BUFMAX;
	}
	ConfigArgv[ConfigArgc] = malloc(len+1);
	bcopy(buf,ConfigArgv[ConfigArgc],len+1);
	ConfigArgc++;
    } while ((len=gettoken(fp,buf)) > 0);
    fclose(fp);
}

int getdist(p,portp,protop)
char *p;
int *portp;	/* host byte order */
int *protop;
{
    char *port_str, *proto_str, *top;
    top = p;
    port_str = proto_str = NULL;
    while (*p) {
	if (*p == ':') {
	    *p++ = '\0';
	    port_str = p;
	} else if (*p == '/') {
	    *p++ = '\0';
	    proto_str = p;
	}
	p++;
    }
    if (!proto_str) {
	*protop = proto_tcp;	/* default */
    } else if (!strcmp(proto_str,"udp")) {
	*protop = proto_udp;
    } else if (!strcmp(proto_str,"tcp")) {
	*protop = proto_tcp;
    } else if (!strcmp(proto_str,"http")) {
	*protop = proto_ohttp;
    } else if (!strcmp(proto_str,"base")) {
	*protop = proto_base;
    } else if (!strcmp(proto_str,"proxy")) {
	*protop = command_ihead;
#ifdef USE_SSL
    } else if (!strcmp(proto_str,"ssl")) {
	*protop = proto_ssl;
#endif
#ifdef USE_POP
    } else if (!strcmp(proto_str,"apop")) {
	*protop = command_pop;
#endif
    } else return -1;	/* error */
    if (port_str) {
	*portp = str2port(port_str,*protop);
	if (*portp < 0) {
	    message(LOG_ERR,"Unknown service: %s",port_str);
	    exit(1);
	}
	return 1;
    } else {
	if (!strcmp(top,"proxy")) {
	    *protop &= ~proto_command;
	    *protop |= command_proxy;
	    *portp = 0;
	    return 1;
	}
	*portp = str2port(top,*protop);
	if (*portp < 0) {
	    message(LOG_ERR,"Unknown service: %s",top);
	    exit(1);
	}
	return 0;	/* no hostname */
    }
}

char *argstr(p)
char *p;
{
    char *ret = malloc(strlen(p)+1);
    char c, *q;
    if (ret == NULL) {
	message(LOG_ERR,"Out of memory.");
	exit(1);
    }
    q = ret;
    while ((c = *p++)) {
	if (c == '\\') {
	    switch(c = *p++) {
	      case 'n':  c = '\n';  break;
	      case 'r':  c = '\r';  break;
	      case 't':  c = '\t';  break;
	      case '\0':
		c = '\\';
		p--;
	    }
	}
	*q++ = c;
    }
    *q = '\0';
    return ret;
}

int doopts(argc,argv)
int argc;
char *argv[];
{
    int i;
    char *p;
    for (i=1; i < argc; i++) {
	p = argv[i];
	if (*p == '-') {
	    p++;
	    while(*p) switch(*p++) {
	      case 'd':
		Debug++;
		break;
#ifndef NO_SYSLOG
	      case 'l':
		Syslog = 1;
		break;
#endif
	      case 'L':
		i++;
		if (!strcmp(argv[i],"-")) {
		    LogFp = stdout;
		} else {
		    if (LogFp && LogFp != stderr) fclose(LogFp);
		    LogFp = fopen(argv[i],"a");
		    if (LogFp == NULL) {
			LogFp = stderr;
			message(LOG_ERR,"Can't create log file err=%d: %s",
				errno,argv[i]);
			exit(1);
		    }
		}
		setbuf(LogFp,NULL);
		break;
	      case 'a':
		i++;
		if (!strcmp(argv[i],"-")) {
		    AccFp = stdout;
		} else {
		    if (AccFp && AccFp != stdout) fclose(AccFp);
		    AccFp = fopen(argv[i],"a");
		    if (AccFp == NULL) {
			message(LOG_ERR,
				"Can't create account log file err=%d: %s",
				errno,argv[i]);
			exit(1);
		    }
		}
		setbuf(AccFp,NULL);
		break;
#ifndef NO_CHROOT
	      case 't':
		RootDir = strdup(argv[++i]);
		break;
#endif
	      case 'n':
		AddrFlag = 1;
		break;
	      case 'u':
		OriginMax = atoi(argv[++i]);
		break;
#ifndef NO_SETUID
	      case 'o':
		SetUID = atoi(argv[++i]);
		break;
	      case 'g':
		SetGID = atoi(argv[++i]);
		break;
#endif
#ifndef NO_FORK
	      case 'f':
		NForks = atoi(argv[++i]);
		break;
#endif
#ifdef USE_SSL
	      case 'z':
		if (++i >= argc) help(argv[0]);
		if (!strncmp(argv[i],"cert=",5)) {
		    certfile = strdup(argv[i]+5);
		} else if (!strncmp(argv[i],"key=",4)) {
		    keyfile = strdup(argv[i]+4);
		} else if (!strncmp(argv[i],"verify=",7)) {
		    ssl_verify_flag = atoi(argv[i]+7);
		} else if (!strcmp(argv[i],"certrequired")) {
		    if (!ssl_verify_flag) ssl_verify_flag++;
		} else if (!strcmp(argv[i],"secure")) {
		    if (!ssl_verify_flag) ssl_verify_flag++;
		} else if (!strncmp(argv[i],"cipher=",7)) {
		    cipher_list = strdup(argv[i]+7);
		} else if (!strcmp(argv[i],"verbose")) {
		    ssl_verbose_flag++;
		} else {
		    message(LOG_ERR,"Invalid SSL Option: %s",argv[i]);
		    help(argv[0]);
		}
		break;
#endif
#ifdef CPP
	      case 'P':
		CppCommand = strdup(argv[++i]);
		break;
#endif
	      case 'C':
		if (!ConfigFile) {
		    i++;
		    ConfigFile = malloc(strlen(argv[i]) + 1);
		    if (ConfigFile == NULL) {
			message(LOG_ERR,"Out of memory.");
			exit(1);
		    }
		    strcpy(ConfigFile,argv[i]);
		    break;
		}	/* drop through */
	      case 'N':
		MyName = strdup(argv[++i]);
#ifdef __NetBSD__
		if (*MyName) { setproctitle("%s", MyName);}
#endif
		break;
#ifdef USE_IPF
	      case 'T':
		TransparentMode = 1;
		break;
#endif
	      default:
		message(LOG_ERR,"Invalid Option: %s",argv[i]);
		help(argv[0]);
	    }
	} else break;
    }
    return i;
}

void doargs(argc,i,argv)
int argc;
int i;
char *argv[];
{
    Stone *stone;
    char *host, *shost;
    int port, sport;
    int proto, sproto, dproto;
    char *p;
    int j, k;
    proto = sproto = dproto = proto_tcp;	/* default: TCP */
    if (argc - i < 1) help(argv[0]);
    for (; i < argc; i++) {
	j = getdist(argv[i],&port,&dproto);
	if (j > 0) {	/* with hostname */
	    host = argv[i++];
	    if (argc <= i) help(argv[0]);
	    j = getdist(argv[i],&sport,&sproto);
	    if (j > 0) {
		shost = argv[i];
	    } else if (j == 0) {
		shost = NULL;
	    } else help(argv[0]);
	} else if (j == 0 && DispHost != NULL) {
	    shost = NULL;	/* without hostname i.e. Display Number */
	    sport = port+XPORT;
	    host = DispHost;
	    port = DispPort;
	    dproto = proto_tcp;
	} else help(argv[0]);
	i++;
	j = 0;
	k = i;
	for (; i < argc; i++, j++) if (!strcmp(argv[i],"--")) break;
	if ((dproto & proto_udp) || (sproto & proto_udp)) {
	    proto = proto_udp;
	} else {
	    proto = proto_tcp;
	    if (sproto & proto_ohttp) proto |= proto_ohttp_s;
	    if (sproto & proto_ssl) proto |= proto_ssl_s;
	    if (sproto & proto_base) proto |= proto_base_s;
	    if ((dproto & proto_command) == command_proxy) {
		proto &= ~proto_command;
		proto |= command_proxy;
#ifdef USE_POP
	    } else if ((dproto & proto_command) == command_pop) {
		proto &= ~proto_command;
		proto |= command_pop;
#endif
	    } else if (dproto & proto_ohttp) {
		proto |= proto_ohttp_d;
		goto extra_arg;
	    } else if ((dproto & proto_command) == command_ihead) {
		proto &= ~proto_command;
		proto |= command_ihead;
	      extra_arg:
		p = argv[k++];
		j--;
		if (k > argc || j < 0) help(argv[0]);
	    }
	    if (dproto & proto_ssl) proto |= proto_ssl_d;
	    if (dproto & proto_base) proto |= proto_base_d;
	}
	stone = mkstone(host,port,shost,sport,j,&argv[k],proto);
	if (proto & proto_ohttp_d) {
	    stone->p = argstr(p);
	} else if ((proto & proto_command) == command_ihead) {
	    stone->p = argstr(p);
	}
	stone->next = stones;
	stones = stone;
    }
    for (stone=stones; stone != NULL; stone=stone->next) {
	FD_SET(stone->sd,&rin);
	FD_SET(stone->sd,&ein);
    }
}

void message_pairs() {	/* dump for debug */
    Pair *pair;
    for (pair=pairs.next; pair != NULL; pair=pair->next) message_pair(pair);
}

void message_origins() {	/* dump for debug */
    Origin *origin;
    for (origin=origins.next; origin != NULL; origin=origin->next)
	message_origin(origin);
}

void message_conns() {	/* dump for debug */
    Conn *conn;
    for (conn=conns.next; conn != NULL; conn=conn->next)
	message_conn(conn);
}

#ifndef WINDOWS
static void handler(sig,code)
int sig, code;
{
    static unsigned int g = 0;
    static int cnt = 0;
    int i;
    switch(sig) {
#ifndef NO_ALRM
      case SIGALRM:
	if (Debug > 8) message(LOG_DEBUG,"SIGALRM. (cnt,g,G)=(%d,%d,%d)",
			       cnt,g,Generation);
	utimer(0);
	signal(SIGALRM,handler);
	if (Generation == g && Recursion < RECURS_MAX) {
	    if (cnt < RECURS_CNT) {
		cnt++;
		if (cnt == RECURS_CNT) {
		    message(LOG_NOTICE,"recursion");
		    message_pairs();
		    message_origins();
		    message_conns();
		}
	    }
	    if (cnt >= RECURS_CNT) {
		{
		    sigset_t set;
		    sigemptyset(&set);
		    sigaddset(&set,SIGALRM);
		    sigprocmask(SIG_UNBLOCK,&set,NULL);
		}
		i = cnt;
		cnt = 0;
		Recursion++;
		repeater();	/* call recursively */
		Recursion--;
		cnt = i;
	    }
	} else {
	    cnt = 0;
	}
	g = Generation;
	utimer(TICK_TIMER);
	break;
#endif
      case SIGHUP:
	if (Debug > 4) message(LOG_DEBUG,"SIGHUP.");
#ifndef NO_FORK
	if (NForks) {	/* mother process */
	    if (ConfigFile && !oldstones) {
	        oldstones = stones;
		stones = NULL;
		OldConfigArgc = ConfigArgc;
		OldConfigArgv = ConfigArgv;
		Debug = 0;
		getconfig();	/* reconfigure */
		i = doopts(ConfigArgc,ConfigArgv);
		doargs(ConfigArgc,i,ConfigArgv);
		for (i=0; i < NForks; i++) {
		    kill(Pid[i],SIGHUP);
		    kill(Pid[i],SIGINT);
		}
	    }
	} else {	/* child process */
#endif
	    message_pairs();
	    message_origins();
	    message_conns();
#ifndef NO_FORK
	}
#endif
	signal(SIGHUP,handler);
	break;
      case SIGTERM:
#ifdef IGN_SIGTERM
	Debug = 0;
	message(LOG_INFO,"SIGTERM. clear Debug level");
	signal(SIGTERM,handler);
	break;
#endif
      case SIGINT:
#ifndef NO_FORK
	if (NForks) {	/* mother process */
	    message(LOG_INFO,"SIGTERM/INT. killing children and exiting...");
	    for (i=0; i < NForks; i++) kill(Pid[i],sig);
	} else
#endif
	    message(LOG_INFO,"SIGTERM/INT. exiting...");  /* child process */
	exit(1);
      case SIGUSR1:
	Debug++;
	message(LOG_INFO,"SIGUSR1. increase Debug level to %d",Debug);
	signal(SIGUSR1,handler);
	break;
      case SIGUSR2:
	if (Debug > 0) Debug--;
	message(LOG_INFO,"SIGUSR2. decrease Debug level to %d",Debug);
	signal(SIGUSR2,handler);
	break;
      case SIGPIPE:
	message(LOG_INFO,"SIGPIPE.");
	signal(SIGPIPE,handler);
	break;
      default:
	message(LOG_INFO,"signal %d. Debug level: %d",sig,Debug);
    }
}
#endif

void initialize(argc,argv)
int argc;
char *argv[];
{
    int i, j;
    char display[256], *p;
    int proto;
#ifdef WINDOWS
    WSADATA WSAData;
#endif
    LogFp = stderr;
    setbuf(stderr,NULL);
    DispHost = NULL;
    p = getenv("DISPLAY");
    if (p) {
	if (*p == ':') {
	    sprintf(display,"localhost%s",p);
	} else {
	    strcpy(display,p);
	}
	i = 0;
	for (p=display; *p; p++) {
	    if (*p == ':') i = 1;
	    else if (i && *p == '.') {
		*p = '\0';
		break;
	    }
	}
	if (getdist(display,&DispPort,&proto) > 0) {
	    DispHost = display;
	    DispPort += XPORT;
	} else {
	    message(LOG_ERR,"Illegal DISPLAY: %s",p);
	}
    }
#ifdef USE_SSL
    SSLeay_add_ssl_algorithms();
    SSL_load_error_strings();
    sprintf(ssl_file_path,"%s/stone.pem",	/* default */
	    X509_get_default_cert_dir());
    keyfile = certfile = ssl_file_path;
#endif
    i = doopts(argc,argv);
    if (ConfigFile) {
	getconfig();
	j = doopts(ConfigArgc,ConfigArgv);
    }
#ifndef NO_SYSLOG
    if (Syslog) {
	sprintf(SyslogName,"%s[%d]", MyName ? MyName : "stone",getpid());
	openlog(SyslogName,0,LOG_DAEMON);
    }
#endif
    message(LOG_INFO,"start (%s) [%d]",VERSION,getpid());
    if (Debug > 0) {
	message(LOG_DEBUG,"Debug level: %d",Debug);
    }
#ifdef WINDOWS
    if (WSAStartup(MAKEWORD(1,1),&WSAData)) {
	message(LOG_ERR,"Can't find winsock.");
	exit(1);
    }
    atexit((void(*)(void))WSACleanup);
#endif
#ifdef USE_SSL
    ssl_ctx_server = SSL_CTX_new(SSLv23_server_method());
    ssl_ctx_client = SSL_CTX_new(SSLv23_client_method());
    if (!cipher_list) cipher_list = getenv("SSL_CIPHER");
#endif
    pairs.next = NULL;
    conns.next = NULL;
    origins.next = NULL;
    FD_ZERO(&rin);
    FD_ZERO(&win);
    FD_ZERO(&ein);
    if (ConfigFile && ConfigArgc > j) {
	if (argc > i) doargs(argc,i,argv);
	doargs(ConfigArgc,j,ConfigArgv);
    } else {
	doargs(argc,i,argv);
    }
    if (!(pkt_buf=malloc(pkt_len_max=PKT_LEN_INI))) {
	message(LOG_ERR,"Out of memory.");
	exit(1);
    }
#ifndef WINDOWS
#ifndef NO_ALRM
    signal(SIGALRM,handler);
#endif
    signal(SIGHUP,handler);
    signal(SIGTERM,handler);
    signal(SIGINT,handler);
    signal(SIGPIPE,handler);
    signal(SIGUSR1,handler);
    signal(SIGUSR2,handler);
#endif
#ifndef NO_FORK
    if (NForks) {
	Pid = malloc(sizeof(pid_t) * NForks);
	if (!Pid) {
	    message(LOG_ERR,"Out of memory.");
	    exit(1);
	}
	for (i=0; i < NForks; i++) {
	    Pid[i] = fork();
	    if (!Pid[i]) break;
	}
	if (i >= NForks) {	/* the mother process */
	    pid_t id;
	    for (;;) {
		id = wait(NULL);
		if (id < 0) continue;
		message(LOG_WARNING,"Process died pid=%d",id);
		for (i=0; i < NForks; i++) {
		    if (Pid[i] == id) break;
		}
		if (i < NForks) {
		    id = fork();
		    if (!id) break;	/* respawned child */
		    else Pid[i] = id;
		} else {
		    message(LOG_ERR,"This can't happen pid=%d",id);
		}
	    }
	}
	free(Pid);	/* child process */
	Pid = NULL;
	NForks = 0;
#ifndef NO_SYSLOG
	if (Syslog) {
	    closelog();
	    sprintf(SyslogName,"%s[%d]", MyName ? MyName : "stone",getpid());
	    openlog(SyslogName,0,LOG_DAEMON);
	}
#endif
	message(LOG_INFO,"child start (%s) [%d]",VERSION,getpid());
    }
#endif
#ifdef WINDOWS
    PairMutex = ConnMutex = OrigMutex = AsyncMutex = NULL;
    if (!(PairMutex=CreateMutex(NULL,FALSE,NULL)) ||
	!(ConnMutex=CreateMutex(NULL,FALSE,NULL)) ||
	!(OrigMutex=CreateMutex(NULL,FALSE,NULL)) ||
	!(AsyncMutex=CreateMutex(NULL,FALSE,NULL)) ||
	!(FdRinMutex=CreateMutex(NULL,FALSE,NULL)) ||
	!(FdWinMutex=CreateMutex(NULL,FALSE,NULL)) ||
	!(FdEinMutex=CreateMutex(NULL,FALSE,NULL))) {
	message(LOG_ERR,"Can't create Mutex err=%d",GetLastError());
    }
#endif
#ifdef OS2
    PairMutex = ConnMutex = OrigMutex = AsyncMutex = NULLHANDLE;
    if ((j=DosCreateMutexSem(NULL,&PairMutex,0,FALSE)) ||
	(j=DosCreateMutexSem(NULL,&ConnMutex,0,FALSE)) ||
	(j=DosCreateMutexSem(NULL,&OrigMutex,0,FALSE)) ||
	(j=DosCreateMutexSem(NULL,&AsyncMutex,0,FALSE)) ||
	(j=DosCreateMutexSem(NULL,&FdRinMutex,0,FALSE)) ||
	(j=DosCreateMutexSem(NULL,&FdWinMutex,0,FALSE)) ||
	(j=DosCreateMutexSem(NULL,&FdEinMutex,0,FALSE))) {
	message(LOG_ERR,"Can't create Mutex err=%d",j);
    }
#endif
#ifndef NO_CHROOT
    if (RootDir) {
	if (chroot(RootDir) < 0) {
	    message(LOG_WARNING,"Can't change root directory to %s",RootDir);
	}
    }
#endif
#ifndef NO_SETUID
    if (SetGID) if (setgid(SetGID) < 0) {
	message(LOG_WARNING,"Can't set gid err=%d.",errno);
    }
    if (SetUID) if (setuid(SetUID) < 0) {
	message(LOG_WARNING,"Can't set uid err=%d.",errno);
    }
#endif
}

#ifdef NT_SERVICE
/* Main thread - runs until event becomes signalled */
DWORD WINAPI ThreadProc(LPVOID lpParms) {
    HANDLE hStopEvent;
    char *lpArgs[3];
    char *p;
    AddToMessageLog("Starting worker thread...");
    hStopEvent = (HANDLE)lpParms;
    if (NULL == hStopEvent) {
	AddToMessageLog("Invalid event handle.");
	ExitThread(1);
    }
    lpArgs[0] = "stone";
    lpArgs[1] = "-C";
    lpArgs[2] = (char*)malloc(MAX_PATH * 2);
    if (! lpArgs[2]) {
	AddToMessageLog("Can't allocate args buffers.");
	ExitThread(2);
    }
    if (0 == GetModuleFileName(NULL, lpArgs[2], MAX_PATH * 2)) {
	AddToMessageLog("Can't get module filename.");
	ExitThread(3);		
    }
    p = strrchr(lpArgs[2], '.');
    if (NULL == p) strcat(lpArgs[2], ".cfg");
    else strcpy(p, ".cfg");
    initialize(3, lpArgs);
    free(lpArgs[2]);
    do {
	repeater();    	
    } while (WAIT_TIMEOUT == WaitForSingleObject(hStopEvent, 1));
    AddToMessageLog("Exiting worker thread");
    ExitThread(0);
    return 0;
}

long svc_main(HANDLE hStopEvent) {	/* Entry point for the service call */
    long thread_id;
    HANDLE hThread;
    hThread = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)ThreadProc,
			   (LPVOID)hStopEvent, 0, &thread_id);
    return hThread != NULL;
}
#else
static void clear_args(argc,argv)
int argc;
char *argv[];
{
    char *argend = argv[argc-1] + strlen(argv[argc-1]);
    char *p;
    for (p=argv[1]; p < argend; p++) *p = '\0';	/* clear args */
}

int main(argc,argv)
int argc;
char *argv[];
{
    initialize(argc,argv);
    clear_args(argc,argv);
    for (;;) repeater();
    return 0;
}
#endif
