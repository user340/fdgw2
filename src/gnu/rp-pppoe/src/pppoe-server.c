/***********************************************************************
*
* pppoe.h
*
* Implementation of a user-space PPPoE server
*
* Copyright (C) 2000 Roaring Penguin Software Inc.
*
* This program may be distributed according to the terms of the GNU
* General Public License, version 2 or (at your option) any later version.
*
* $Id: pppoe-server.c,v 1.14 2001/01/29 18:50:36 dfs Exp $
*
***********************************************************************/

static char const RCSID[] =
"$Id: pppoe-server.c,v 1.14 2001/01/29 18:50:36 dfs Exp $";

#include "config.h"

#if defined(HAVE_NETPACKET_PACKET_H) || defined(HAVE_LINUX_IF_PACKET_H)
#define _POSIX_SOURCE 1 /* For sigaction defines */
#endif

#define _BSD_SOURCE 1 /* for gethostname */

#include "pppoe.h"
#include "md5.h"

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif

#include <errno.h>
#include <string.h>
#include <stdlib.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#include <signal.h>

/* Hack for daemonizing */
#define CLOSEFD 64

/* Max. 64 sessions by default */
#define DEFAULT_MAX_SESSIONS 64

/* A list of client sessions */
struct ClientSession *Sessions = NULL;

/* The number of session slots */
size_t NumSessionSlots;

/* Offset of first session */
size_t SessOffset = 0;

/* Socket for client's discovery phases */
int Socket = -1;

/* Pipe written on reception of SIGCHLD */
int Pipe[2] = {-1, -1};
int ReapPending = 0;

/* Synchronous mode */
int Synchronous = 0;

/* Random seed for cookie generation */
#define SEED_LEN 16
#define MD5_LEN 16
#define COOKIE_LEN (MD5_LEN + sizeof(pid_t)) /* Cookie is 16-byte MD5 + PID of server */

unsigned char CookieSeed[SEED_LEN];

/* Default interface if no -I option given */
#define DEFAULT_IF "eth0"
char *IfName = NULL;

/* Access concentrator name */
char *ACName = NULL;

/* Options to pass to pppoe process */
char PppoeOptions[SMALLBUF] = "";

/* Our local IP address */
unsigned char LocalIP[IPV4ALEN] = {10, 0, 0, 1};
unsigned char RemoteIP[IPV4ALEN] = {10, 67, 15, 1}; /* Counter STARTS here */

PPPoETag hostUniq;
PPPoETag relayId;
PPPoETag receivedCookie;
PPPoETag requestedService;

#define HOSTNAMELEN 256

static void startPPPD(struct ClientSession *sess);
static void sendErrorPADS(int sock, unsigned char *source, unsigned char *dest,
			  int errorTag, char *errorMsg);

#define CHECK_ROOM(cursor, start, len) \
do {\
    if (((cursor)-(start))+(len) > MAX_PPPOE_PAYLOAD) { \
        syslog(LOG_ERR, "Would create too-long packet"); \
        return; \
    } \
} while(0)

/* Use Linux kernel-mode PPPoE? */
int UseLinuxKernelModePPPoE = 0;

/**********************************************************************
*%FUNCTION: parsePADITags
*%ARGUMENTS:
* type -- tag type
* len -- tag length
* data -- tag data
* extra -- extra user data.
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Picks interesting tags out of a PADI packet
***********************************************************************/
void
parsePADITags(UINT16_t type, UINT16_t len, unsigned char *data,
	      void *extra)
{
    switch(type) {
    case TAG_SERVICE_NAME:
	/* Should do something -- currently ignored */
	break;
    case TAG_RELAY_SESSION_ID:
	relayId.type = htons(type);
	relayId.length = htons(len);
	memcpy(relayId.payload, data, len);
	break;
    case TAG_HOST_UNIQ:
	hostUniq.type = htons(type);
	hostUniq.length = htons(len);
	memcpy(hostUniq.payload, data, len);
	break;
    }
}

/**********************************************************************
*%FUNCTION: parsePADRTags
*%ARGUMENTS:
* type -- tag type
* len -- tag length
* data -- tag data
* extra -- extra user data.
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Picks interesting tags out of a PADR packet
***********************************************************************/
void
parsePADRTags(UINT16_t type, UINT16_t len, unsigned char *data,
	      void *extra)
{
    switch(type) {
    case TAG_RELAY_SESSION_ID:
	relayId.type = htons(type);
	relayId.length = htons(len);
	memcpy(relayId.payload, data, len);
	break;
    case TAG_HOST_UNIQ:
	hostUniq.type = htons(type);
	hostUniq.length = htons(len);
	memcpy(hostUniq.payload, data, len);
	break;
    case TAG_AC_COOKIE:
	receivedCookie.type = htons(type);
	receivedCookie.length = htons(len);
	memcpy(receivedCookie.payload, data, len);
	break;
    case TAG_SERVICE_NAME:
	requestedService.type = htons(type);
	requestedService.length = htons(len);
	memcpy(requestedService.payload, data, len);
	break;
    }
}

/**********************************************************************
*%FUNCTION: findSession
*%ARGUMENTS:
* pid -- PID of child which owns session.  If PID is 0, searches for
* empty session slots.
*%RETURNS:
* A pointer to the session, or NULL if no such session found.
*%DESCRIPTION:
* Searches for specified session.
**********************************************************************/
struct ClientSession *
findSession(pid_t pid)
{
    size_t i;
    for (i=0; i<NumSessionSlots; i++) {
	if (Sessions[i].pid == pid) {
	    return &Sessions[i];
	}
    }
    return NULL;
}

/**********************************************************************
*%FUNCTION: reapSessions
*%ARGUMENTS:
* None
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Reaps children which have exited and removes their sessions
**********************************************************************/
void
reapSessions(void)
{
    int status;
    pid_t pid;
    struct ClientSession *session;

    while((pid = waitpid(-1, &status, WNOHANG)) > 0) {
	session = findSession(pid);
	if (!session) {
	    syslog(LOG_ERR, "Child %d died but couldn't find session!",
		   (int) pid);
	} else {
	    syslog(LOG_INFO,
		   "Session %d closed for client %02x:%02x:%02x:%02x:%02x:%02x (%d.%d.%d.%d)",
		   ntohs(session->sess),
		   session->eth[0], session->eth[1], session->eth[2],
		   session->eth[3], session->eth[4], session->eth[5],
		   (int) session->ip[0], (int) session->ip[1],
		   (int) session->ip[2], (int) session->ip[3]);
	    session->pid = 0;
	}
    }
}

/**********************************************************************
*%FUNCTION: fatalSys
*%ARGUMENTS:
* str -- error message
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Prints a message plus the errno value to stderr and syslog and exits.
***********************************************************************/
void
fatalSys(char const *str)
{
    char buf[SMALLBUF];
    snprintf(buf, SMALLBUF, "%s: %s", str, strerror(errno));
    printErr(buf);
    exit(EXIT_FAILURE);
}

/**********************************************************************
*%FUNCTION: sysErr
*%ARGUMENTS:
* str -- error message
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Prints a message plus the errno value to syslog.
***********************************************************************/
void
sysErr(char const *str)
{
    char buf[1024];
    sprintf(buf, "%.256s: %.256s", str, strerror(errno));
    printErr(buf);
}

/**********************************************************************
*%FUNCTION: rp_fatal
*%ARGUMENTS:
* str -- error message
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Prints a message to stderr and syslog and exits.
***********************************************************************/
void
rp_fatal(char const *str)
{
    printErr(str);
    exit(EXIT_FAILURE);
}

/**********************************************************************
*%FUNCTION: genCookie
*%ARGUMENTS:
* peerEthAddr -- peer Ethernet address (6 bytes)
* myEthAddr -- my Ethernet address (6 bytes)
* seed -- random cookie seed to make things tasty (16 bytes)
* cookie -- buffer which is filled with server PID and 
*           md5 sum of previous items
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Forms the md5 sum of peer MAC address, our MAC address and seed, useful
* in a PPPoE Cookie tag.
***********************************************************************/
void
genCookie(unsigned char const *peerEthAddr,
	  unsigned char const *myEthAddr,
	  unsigned char const *seed,
	  unsigned char *cookie)
{
    struct MD5Context ctx;
    pid_t pid = getpid();

    MD5Init(&ctx);
    MD5Update(&ctx, peerEthAddr, ETH_ALEN);
    MD5Update(&ctx, myEthAddr, ETH_ALEN);
    MD5Update(&ctx, seed, SEED_LEN);
    MD5Final(cookie, &ctx);
    memcpy(cookie+MD5_LEN, &pid, sizeof(pid));
}

/**********************************************************************
*%FUNCTION: processPADI
*%ARGUMENTS:
* sock -- Ethernet socket
* myAddr -- my Ethernet address
* packet -- PPPoE PADI packet
* len -- length of received packet
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Sends a PADO packet back to client
***********************************************************************/
void
processPADI(int sock, unsigned char *myAddr,
	    PPPoEPacket *packet, int len)
{
    PPPoEPacket pado;
    PPPoETag acname;
    PPPoETag servname;
    PPPoETag cookie;
    size_t acname_len;
    unsigned char *cursor = pado.payload;
    UINT16_t plen;

    /* Ignore PADI's which don't come from a unicast address */
    if (NOT_UNICAST(packet->ethHdr.h_source)) {
	syslog(LOG_ERR, "PADI packet from non-unicast source address");
	return;
    }

    acname.type = htons(TAG_AC_NAME);
    acname_len = strlen(ACName);
    acname.length = htons(acname_len);
    memcpy(acname.payload, ACName, acname_len);

    servname.type = htons(TAG_SERVICE_NAME);
    servname.length = 0;

    relayId.type = 0;
    hostUniq.type = 0;
    parsePacket(packet, parsePADITags, NULL);

    /* Generate a cookie */
    cookie.type = htons(TAG_AC_COOKIE);
    cookie.length = htons(COOKIE_LEN);
    genCookie(packet->ethHdr.h_source, myAddr, CookieSeed, cookie.payload);

    /* Construct a PADO packet */
    memcpy(pado.ethHdr.h_dest, packet->ethHdr.h_source, ETH_ALEN);
    memcpy(pado.ethHdr.h_source, myAddr, ETH_ALEN);
    pado.ethHdr.h_proto = htons(Eth_PPPOE_Discovery);
    pado.ver = 1;
    pado.type = 1;
    pado.code = CODE_PADO;
    pado.session = 0;
    plen = TAG_HDR_SIZE + acname_len;

    CHECK_ROOM(cursor, pado.payload, acname_len+TAG_HDR_SIZE);
    memcpy(cursor, &acname, acname_len + TAG_HDR_SIZE);
    cursor += acname_len + TAG_HDR_SIZE;

    CHECK_ROOM(cursor, pado.payload, TAG_HDR_SIZE);
    memcpy(cursor, &servname, TAG_HDR_SIZE);
    cursor += TAG_HDR_SIZE;
    plen += TAG_HDR_SIZE;

    CHECK_ROOM(cursor, pado.payload, TAG_HDR_SIZE + COOKIE_LEN);
    memcpy(cursor, &cookie, TAG_HDR_SIZE + COOKIE_LEN);
    cursor += TAG_HDR_SIZE + COOKIE_LEN;
    plen += TAG_HDR_SIZE + COOKIE_LEN;

    if (relayId.type) {
	CHECK_ROOM(cursor, pado.payload, ntohs(relayId.length) + TAG_HDR_SIZE);
	memcpy(cursor, &relayId, ntohs(relayId.length) + TAG_HDR_SIZE);
	cursor += ntohs(relayId.length) + TAG_HDR_SIZE;
	plen += ntohs(relayId.length) + TAG_HDR_SIZE;
    }
    if (hostUniq.type) {
	CHECK_ROOM(cursor, pado.payload, ntohs(hostUniq.length)+TAG_HDR_SIZE);
	memcpy(cursor, &hostUniq, ntohs(hostUniq.length) + TAG_HDR_SIZE);
	cursor += ntohs(hostUniq.length) + TAG_HDR_SIZE;
	plen += ntohs(hostUniq.length) + TAG_HDR_SIZE;
    }
    pado.length = htons(plen);
    sendPacket(sock, &pado, (int) (plen + HDR_SIZE));
}

/**********************************************************************
*%FUNCTION: processPADT
*%ARGUMENTS:
* sock -- Ethernet socket
* myAddr -- my Ethernet address
* packet -- PPPoE PADT packet
* len -- length of received packet
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Kills session whose session-ID is in PADT packet.
***********************************************************************/
void
processPADT(int sock, unsigned char *myAddr,
	    PPPoEPacket *packet, int len)
{
    size_t i;

    /* Ignore PADT's not directed at us */
    if (memcmp(packet->ethHdr.h_dest, myAddr, ETH_ALEN)) return;

    /* Get session's index */
    i = ntohs(packet->session) - 1 - SessOffset;
    if (i >= NumSessionSlots) return;
    if (Sessions[i].sess != packet->session) {
	syslog(LOG_ERR, "Session index %u doesn't match session number %u",
	       (unsigned int) i, (unsigned int) ntohs(packet->session));
	return;
    }
    if (Sessions[i].pid) {
	parsePacket(packet, parseLogErrs, NULL);
	kill(Sessions[i].pid, SIGTERM);
    }
}

/**********************************************************************
*%FUNCTION: processPADR
*%ARGUMENTS:
* sock -- Ethernet socket
* myAddr -- my Ethernet address
* packet -- PPPoE PADR packet
* len -- length of received packet
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Sends a PADS packet back to client and starts a PPP session if PADR
* packet is OK.
***********************************************************************/
void
processPADR(int sock, unsigned char *myAddr,
	    PPPoEPacket *packet, int len)
{
    unsigned char cookieBuffer[COOKIE_LEN];
    struct ClientSession *cliSession;
    pid_t child;
    PPPoEPacket pads;
    unsigned char *cursor = pads.payload;
    UINT16_t plen;
    PPPoETag servname;

    /* Initialize some globals */
    relayId.type = 0;
    hostUniq.type = 0;
    receivedCookie.type = 0;
    requestedService.type = 0;

    /* Ignore PADR's not directed at us */
    if (memcmp(packet->ethHdr.h_dest, myAddr, ETH_ALEN)) return;

    /* Ignore PADR's from non-unicast addresses */
    if (NOT_UNICAST(packet->ethHdr.h_source)) {
	syslog(LOG_ERR, "PADR packet from non-unicast source address");
	return;
    }

    parsePacket(packet, parsePADRTags, NULL);

    /* Check that everything's cool */
    if (!receivedCookie.type) {
	/* Drop it -- do not send error PADS */
	return;
    }

    /* Is cookie kosher? */
    if (receivedCookie.length != htons(COOKIE_LEN)) {
	/* Drop it -- do not send error PADS */
	return;
    }

    genCookie(packet->ethHdr.h_source, myAddr, CookieSeed, cookieBuffer);
    if (memcmp(receivedCookie.payload, cookieBuffer, COOKIE_LEN)) {
	/* Drop it -- do not send error PADS */
	return;
    }

    /* Check service name -- we only offer service "" */
    if (!requestedService.type) {
	syslog(LOG_ERR, "Received PADR packet with no SERVICE_NAME tag");
	sendErrorPADS(sock, myAddr, packet->ethHdr.h_source,
		      TAG_SERVICE_NAME_ERROR, "RP-PPPoE: Server: No service name tag");
	return;
    }

    if (requestedService.length) {
	syslog(LOG_ERR, "Received PADR packet asking for unsupported service %.*s", (int) ntohs(requestedService.length), requestedService.payload);
	sendErrorPADS(sock, myAddr, packet->ethHdr.h_source,
		      TAG_SERVICE_NAME_ERROR, "RP-PPPoE: Server: Invalid service name tag");
	return;
    }

    /* Looks cool... find a slot for the session */
    cliSession = findSession(0);
    if (!cliSession) {
	syslog(LOG_ERR, "No client slots available (%02x:%02x:%02x:%02x:%02x:%02x)",
	       (unsigned int) packet->ethHdr.h_source[0],
	       (unsigned int) packet->ethHdr.h_source[1],
	       (unsigned int) packet->ethHdr.h_source[2],
	       (unsigned int) packet->ethHdr.h_source[3],
	       (unsigned int) packet->ethHdr.h_source[4],
	       (unsigned int) packet->ethHdr.h_source[5]);
	sendErrorPADS(sock, myAddr, packet->ethHdr.h_source,
		      TAG_AC_SYSTEM_ERROR, "RP-PPPoE: Server: No client slots available");
	return;
    }

    /* Set up client session peer Ethernet address */
    memcpy(cliSession->eth, packet->ethHdr.h_source, ETH_ALEN);

    /* Create child process, send PADS packet back */
    child = fork();
    if (child < 0) {
	sendErrorPADS(sock, myAddr, packet->ethHdr.h_source,
		      TAG_AC_SYSTEM_ERROR, "RP-PPPoE: Server: Unable to start session process");
	return;
    }
    if (child != 0) {
	/* In the parent process.  Mark pid in session slot */
	cliSession->pid = child;
	return;
    }

    /* In the child process.  */

    /* pppd has a nasty habit of killing all processes in its process group.
       Start a new session to stop pppd from killing us! */
    setsid();

    /* Send PADS and Start pppd */
    memcpy(pads.ethHdr.h_dest, packet->ethHdr.h_source, ETH_ALEN);
    memcpy(pads.ethHdr.h_source, myAddr, ETH_ALEN);
    pads.ethHdr.h_proto = htons(Eth_PPPOE_Discovery);
    pads.ver = 1;
    pads.type = 1;
    pads.code = CODE_PADS;
    
    pads.session = cliSession->sess;
    plen = 0;
    
    servname.type = htons(TAG_SERVICE_NAME);
    servname.length = 0;
    
    memcpy(cursor, &servname, TAG_HDR_SIZE);
    cursor += TAG_HDR_SIZE;
    plen += TAG_HDR_SIZE;
    
    if (relayId.type) {
	memcpy(cursor, &relayId, ntohs(relayId.length) + TAG_HDR_SIZE);
	cursor += ntohs(relayId.length) + TAG_HDR_SIZE;
	plen += ntohs(relayId.length) + TAG_HDR_SIZE;
    }
    if (hostUniq.type) {
	memcpy(cursor, &hostUniq, ntohs(hostUniq.length) + TAG_HDR_SIZE);
	cursor += ntohs(hostUniq.length) + TAG_HDR_SIZE;
	plen += ntohs(hostUniq.length) + TAG_HDR_SIZE;
    }
    pads.length = htons(plen);
    sendPacket(sock, &pads, (int) (plen + HDR_SIZE));
    startPPPD(cliSession);
}

/**********************************************************************
*%FUNCTION: childHandler
*%ARGUMENTS:
* sig -- signal number
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Called by SIGCHLD.  Writes one byte to Pipe to wake up the select
* loop and cause reaping of dead sessions
***********************************************************************/
void
childHandler(int sig)
{
    if (!ReapPending) {
	ReapPending = 1;
	write(Pipe[1], &ReapPending, 1);
    }
}

/**********************************************************************
*%FUNCTION: usage
*%ARGUMENTS:
* argv0 -- argv[0] from main
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Prints usage instructions
***********************************************************************/
void
usage(char const *argv0)
{
    fprintf(stderr, "Usage: %s [options]\n", argv0);
    fprintf(stderr, "Options:\n");
#ifdef USE_BPF
    fprintf(stderr, "   -I if_name     -- Specify interface (REQUIRED)\n");
#else
    fprintf(stderr, "   -I if_name     -- Specify interface (default %s.)\n",
	    DEFAULT_IF);
#endif
    fprintf(stderr, "   -T timeout     -- Specify inactivity timeout in seconds.\n");
    fprintf(stderr, "   -C name        -- Set access concentrator name.\n");
    fprintf(stderr, "   -m MSS         -- Clamp incoming and outgoing MSS options.\n");
    fprintf(stderr, "   -L ip          -- Set local IP address.\n");
    fprintf(stderr, "   -R ip          -- Set start address of remote IP pool.\n");
    fprintf(stderr, "   -N num         -- Allow 'num' concurrent sessions.\n");
    fprintf(stderr, "   -o offset      -- Assign session numbers starting at offset+1.\n");
    fprintf(stderr, "   -f disc:sess   -- Set Ethernet frame types (hex).\n");
    fprintf(stderr, "   -s             -- Use synchronous PPP mode.\n");
#ifdef HAVE_LINUX_KERNEL_PPPOE
    fprintf(stderr, "   -k             -- Use kernel-mode PPPoE.\n");
#endif
    fprintf(stderr, "   -h             -- Print usage information.\n\n");
    fprintf(stderr, "PPPoE-Server Version %s, Copyright (C) 2001 Roaring Penguin Software Inc.\n", VERSION);
    fprintf(stderr, "PPPoE-Server comes with ABSOLUTELY NO WARRANTY.\n");
    fprintf(stderr, "This is free software, and you are welcome to redistribute it\n");
    fprintf(stderr, "under the terms of the GNU General Public License, version 2\n");
    fprintf(stderr, "or (at your option) any later version.\n");
    fprintf(stderr, "http://www.roaringpenguin.com\n");
}

/**********************************************************************
*%FUNCTION: main
*%ARGUMENTS:
* argc, argv -- usual suspects
*%RETURNS:
* Exit status
*%DESCRIPTION:
* Main program of PPPoE server
***********************************************************************/
int
main(int argc, char **argv)
{

    FILE *fp;
    int i;
    int opt;
    unsigned char myAddr[ETH_ALEN];
    PPPoEPacket packet;
    int len;
    int sock;
    int d[IPV4ALEN];
    int beDaemon = 1;
    struct sigaction act;
    int maxFD;
    unsigned int discoveryType, sessionType;

#ifndef HAVE_LINUX_KERNEL_PPPOE
    char *options = "hI:C:L:R:T:m:FN:f:o:s";
#else
    char *options = "hI:C:L:R:T:m:FN:f:o:sk";
#endif

    /* Initialize syslog */
    openlog("pppoe-server", LOG_PID, LOG_DAEMON);

    /* Default number of session slots */
    NumSessionSlots = DEFAULT_MAX_SESSIONS;

    /* Parse command-line options */
    while((opt = getopt(argc, argv, options)) != -1) {
	switch(opt) {
#ifdef HAVE_LINUX_KERNEL_PPPOE
	case 'k':
	    UseLinuxKernelModePPPoE = 1;
	    break;
#endif
	case 's':
	    Synchronous = 1;
	    /* Pass the Synchronous option on to pppoe */
	    snprintf(PppoeOptions + strlen(PppoeOptions),
		     SMALLBUF-strlen(PppoeOptions),
		     " -s");
	    break;
	case 'f':
	    if (sscanf(optarg, "%x:%x", &discoveryType, &sessionType) != 2) {
		fprintf(stderr, "Illegal argument to -f: Should be disc:sess in hex\n");
		exit(EXIT_FAILURE);
	    }
	    Eth_PPPOE_Discovery = (UINT16_t) discoveryType;
	    Eth_PPPOE_Session   = (UINT16_t) sessionType;
	    /* This option gets passed to pppoe */
	    snprintf(PppoeOptions + strlen(PppoeOptions),
		     SMALLBUF-strlen(PppoeOptions),
		     " -%c %s", opt, optarg);
	    break;
	case 'F':
	    beDaemon = 0;
	    break;
	case 'N':
	    if (sscanf(optarg, "%d", &opt) != 1) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	    }
	    if (opt <= 0) {
		fprintf(stderr, "-N: Value must be positive\n");
		exit(EXIT_FAILURE);
	    }
	    NumSessionSlots = opt;
	    break;
	case 'o':
	    if (sscanf(optarg, "%d", &opt) != 1) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	    }
	    if (opt < 0) {
		fprintf(stderr, "-o: Value must be non-negative\n");
		exit(EXIT_FAILURE);
	    }
	    SessOffset = (size_t) opt;
	    break;

	case 'I':
	    SET_STRING(IfName, optarg);
	    break;
	case 'C':
	    SET_STRING(ACName, optarg);
	    break;
	case 'L':
	case 'R':
	    /* Get local/remote IP address */
	    if (sscanf(optarg, "%d.%d.%d.%d", &d[0], &d[1], &d[2], &d[3]) != 4) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	    }
	    for (i=0; i<IPV4ALEN; i++) {
		if (d[i] < 0 || d[i] > 255) {
		    usage(argv[0]);
		    exit(EXIT_FAILURE);
		}
		if (opt == 'L') {
		    LocalIP[i] = (unsigned char) d[i];
		} else {
		    RemoteIP[i] = (unsigned char) d[i];
		}
	    }
	    break;
	case 'T':
	case 'm':
	    /* These just get passed to pppoe */
	    snprintf(PppoeOptions + strlen(PppoeOptions),
		     SMALLBUF-strlen(PppoeOptions),
		     " -%c %s", opt, optarg);
	    break;
	case 'h':
	    usage(argv[0]);
	    exit(EXIT_SUCCESS);
	}
    }
    
    if (!IfName) {
	IfName = DEFAULT_IF;
    }
    
    if (!ACName) {
	ACName = malloc(HOSTNAMELEN);
	if (gethostname(ACName, HOSTNAMELEN) < 0) {
	    fatalSys("gethostname");
	}
    }

    /* Max 65534 - SessOffset sessions */
    if (NumSessionSlots + SessOffset > 65534) {
	fprintf(stderr, "-N and -o options must add up to at most 65534\n");
	exit(EXIT_FAILURE);
    }

    /* Allocate memory for sessions */
    Sessions = calloc(NumSessionSlots, sizeof(struct ClientSession));
    if (!Sessions) {
	rp_fatal("Cannot allocate memory for session slots");
    }

    /* Daemonize -- UNIX Network Programming, Vol. 1, Stevens */
    if (beDaemon) {
	i = fork();
	if (i < 0) {
	    fatalSys("fork");
	} else if (i != 0) {
	    /* parent */
	    exit(EXIT_SUCCESS);
	}
	setsid();
	signal(SIGHUP, SIG_IGN);
	i = fork();
	if (i < 0) {
	    fatalSys("fork");
	} else if (i != 0) {
	    exit(EXIT_SUCCESS);
	}

	chdir("/");
	closelog();
	for (i=0; i<CLOSEFD; i++) close(i);
	/* We nuked our syslog descriptor... */
	openlog("pppoe-server", LOG_PID, LOG_DAEMON);
    }

    for(i=0; i<NumSessionSlots; i++) {
	Sessions[i].pid = 0;
	memcpy(Sessions[i].ip, RemoteIP, sizeof(RemoteIP));
	Sessions[i].sess = htons(i+1+SessOffset);

	/* Increment IP */
	RemoteIP[3]++;
	if (!RemoteIP[3] || RemoteIP[3] == 255) {
	    RemoteIP[3] = 1;
	    RemoteIP[2]++;
	    if (!RemoteIP[2]) {
		RemoteIP[1]++;
		if (!RemoteIP[1]) {
		    RemoteIP[0]++;
		}
	    }
	}
    }

    /* Initialize our random cookie.  Try /dev/urandom; if that fails,
       use PID and rand() */
    fp = fopen("/dev/urandom", "r");
    if (fp) {
	fread(&CookieSeed, 1, SEED_LEN, fp);
	fclose(fp);
    } else {
	CookieSeed[0] = getpid() & 0xFF;
	CookieSeed[1] = (getpid() >> 8) & 0xFF;
	for (i=2; i<SEED_LEN; i++) {
	    CookieSeed[i] = (rand() >> (i % 9)) & 0xFF;
	}
    }
    
    sock = openInterface(IfName, Eth_PPPOE_Discovery, myAddr);

    /* Set signal handler for SIGCHLD */
    act.sa_handler = childHandler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_NOCLDSTOP | SA_RESTART;
    if (sigaction(SIGCHLD, &act, NULL) < 0) {
	fatalSys("sigaction");
    }

    /* Set up pipe for signal handler */
    if (pipe(Pipe) < 0) {
	fatalSys("pipe");
    }

    /* Main server loop */
    maxFD = sock;
    if (Pipe[0] > maxFD) maxFD = Pipe[0];
    maxFD++;

    for(;;) {
	fd_set readable;
	FD_ZERO(&readable);
	FD_SET(sock, &readable);
	FD_SET(Pipe[0], &readable);

	while(1) {
	    i = select(maxFD, &readable, NULL, NULL, NULL);
	    if (i >= 0 || errno != EINTR) break;
	}
	if (i < 0) {
	    fatalSys("select");
	}

	if (FD_ISSET(Pipe[0], &readable)) {
	    /* Clear pipe */
	    char buf[SMALLBUF];
	    read(Pipe[0], buf, SMALLBUF);
	}

	if (ReapPending) {
	    ReapPending = 0;
	    reapSessions();
	}
	if (!FD_ISSET(sock, &readable)) {
	    continue;
	}

	if (receivePacket(sock, &packet, &len) < 0) {
	    continue;
	}
	
	/* Check length */
	if (ntohs(packet.length) + HDR_SIZE > len) {
	    syslog(LOG_ERR, "Bogus PPPoE length field (%u)",
		   (unsigned int) ntohs(packet.length));
	    continue;
	}

	/* Sanity check on packet */
	if (packet.ver != 1 || packet.type != 1) {
	    /* Syslog an error */
	    continue;
	}
	switch(packet.code) {
	case CODE_PADI:
	    processPADI(sock, myAddr, &packet, len);
	    break;
	case CODE_PADR:
	    processPADR(sock, myAddr, &packet, len);
	    break;
	case CODE_PADT:
	    /* Kill the child */
	    processPADT(sock, myAddr, &packet, len);
	    break;
	case CODE_SESS:
	    /* Ignore SESS -- children will handle them */
	    break;
	case CODE_PADO:
	case CODE_PADS:
  	    /* Ignore PADO and PADS totally */
	    break;
	default:
	    /* Syslog an error */
	    break;
	}
    }
    return 0;
}

/**********************************************************************
*%FUNCTION: sendErrorPADS
*%ARGUMENTS:
* sock -- socket to write to
* source -- source Ethernet address
* dest -- destination Ethernet address
* errorTag -- error tag
* errorMsg -- error message
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Sends a PADS packet with an error message
***********************************************************************/
void
sendErrorPADS(int sock,
	      unsigned char *source,
	      unsigned char *dest,
	      int errorTag,
	      char *errorMsg)
{
    PPPoEPacket pads;
    unsigned char *cursor = pads.payload;
    UINT16_t plen;
    PPPoETag err;
    int elen = strlen(errorMsg);

    memcpy(pads.ethHdr.h_dest, dest, ETH_ALEN);
    memcpy(pads.ethHdr.h_source, source, ETH_ALEN);
    pads.ethHdr.h_proto = htons(Eth_PPPOE_Discovery);
    pads.ver = 1;
    pads.type = 1;
    pads.code = CODE_PADS;
    
    pads.session = htons(0);
    plen = 0;
    
    err.type = htons(errorTag);
    err.length = htons(elen);
    
    memcpy(err.payload, errorMsg, elen);
    memcpy(cursor, &err, TAG_HDR_SIZE+elen);
    cursor += TAG_HDR_SIZE + elen;
    plen += TAG_HDR_SIZE + elen;
    
    if (relayId.type) {
	memcpy(cursor, &relayId, ntohs(relayId.length) + TAG_HDR_SIZE);
	cursor += ntohs(relayId.length) + TAG_HDR_SIZE;
	plen += ntohs(relayId.length) + TAG_HDR_SIZE;
    }
    if (hostUniq.type) {
	memcpy(cursor, &hostUniq, ntohs(hostUniq.length) + TAG_HDR_SIZE);
	cursor += ntohs(hostUniq.length) + TAG_HDR_SIZE;
	plen += ntohs(hostUniq.length) + TAG_HDR_SIZE;
    }
    pads.length = htons(plen);
    sendPacket(sock, &pads, (int) (plen + HDR_SIZE));
}
	      

/**********************************************************************
*%FUNCTION: startPPPDUserMode
*%ARGUMENTS:
* session -- client session record
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Starts PPPD for user-mode PPPoE
***********************************************************************/
void
startPPPDUserMode(struct ClientSession *session)
{
    /* Leave some room */
    char *argv[20];

    char buffer[SMALLBUF];

    argv[0] = "pppd";
    argv[1] = "pty";

    snprintf(buffer, SMALLBUF, "%s -I %s -e %d:%02x:%02x:%02x:%02x:%02x:%02x%s",
	     PPPOE_PATH, IfName,
	     ntohs(session->sess),
	     session->eth[0], session->eth[1], session->eth[2],
	     session->eth[3], session->eth[4], session->eth[5],
	     PppoeOptions);
    argv[2] = strdup(buffer);
    if (!argv[2]) {
	/* TODO: Send a PADT */
	exit(EXIT_FAILURE);
    }

    argv[3] = "file";
    argv[4] = PPPOE_SERVER_OPTIONS;

    snprintf(buffer, SMALLBUF, "%d.%d.%d.%d:%d.%d.%d.%d",
	    (int) LocalIP[0], (int) LocalIP[1],
	    (int) LocalIP[2], (int) LocalIP[3],
	    (int) session->ip[0], (int) session->ip[1],
	    (int) session->ip[2], (int) session->ip[3]);
    syslog(LOG_INFO,
	   "Session %d created for client %02x:%02x:%02x:%02x:%02x:%02x (%d.%d.%d.%d)",
	   ntohs(session->sess),
	   session->eth[0], session->eth[1], session->eth[2],
	   session->eth[3], session->eth[4], session->eth[5],
	   (int) session->ip[0], (int) session->ip[1],
	   (int) session->ip[2], (int) session->ip[3]);
    argv[5] = buffer; /* No need for strdup -- about to execv! */
    argv[6] = "nodetach";
    argv[7] = "noaccomp";
    argv[8] = "nobsdcomp";
    argv[9] = "nodeflate";
    argv[10] = "nopcomp";
    argv[11] = "novj";
    argv[12] = "novjccomp";
    argv[13] = "default-asyncmap";
    if (Synchronous) {
	argv[14] = "sync";
	argv[15] = NULL;
    } else {
	argv[14] = NULL;
    }

    execv(PPPD_PATH, argv);
    exit(EXIT_FAILURE);
}

/**********************************************************************
*%FUNCTION: startPPPDLinuxKernelMode
*%ARGUMENTS:
* session -- client session record
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Starts PPPD for kernel-mode PPPoE on Linux
***********************************************************************/
void
startPPPDLinuxKernelMode(struct ClientSession *session)
{
    /* Leave some room */
    char *argv[20];

    char buffer[SMALLBUF];

    argv[0] = "pppd";
    argv[1] = "plugin";
    argv[2] = PLUGIN_PATH;
    argv[3] = IfName;
    snprintf(buffer, SMALLBUF, "%d:%02x:%02x:%02x:%02x:%02x:%02x",
	     ntohs(session->sess),
	     session->eth[0], session->eth[1], session->eth[2],
	     session->eth[3], session->eth[4], session->eth[5]);
    argv[4] = "rp_pppoe_sess";
    argv[5] = strdup(buffer);
    if (!argv[5]) {
	/* TODO: Send a PADT */
	exit(EXIT_FAILURE);
    }
    argv[6] = "file";
    argv[7] = PPPOE_SERVER_OPTIONS;

    snprintf(buffer, SMALLBUF, "%d.%d.%d.%d:%d.%d.%d.%d",
	    (int) LocalIP[0], (int) LocalIP[1],
	    (int) LocalIP[2], (int) LocalIP[3],
	    (int) session->ip[0], (int) session->ip[1],
	    (int) session->ip[2], (int) session->ip[3]);
    syslog(LOG_INFO,
	   "Session %d created for client %02x:%02x:%02x:%02x:%02x:%02x (%d.%d.%d.%d)",
	   ntohs(session->sess),
	   session->eth[0], session->eth[1], session->eth[2],
	   session->eth[3], session->eth[4], session->eth[5],
	   (int) session->ip[0], (int) session->ip[1],
	   (int) session->ip[2], (int) session->ip[3]);
    argv[8] = buffer;
    argv[9] = "nodetach";
    argv[10] = "noaccomp";
    argv[11] = "nobsdcomp";
    argv[12] = "nodeflate";
    argv[13] = "nopcomp";
    argv[14] = "novj";
    argv[15] = "novjccomp";
    argv[16] = "default-asyncmap";
    argv[17] = NULL;
    execv(PPPD_PATH, argv);
    exit(EXIT_FAILURE);
}

/**********************************************************************
*%FUNCTION: startPPPD
*%ARGUMENTS:
* session -- client session record
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Starts PPPD
***********************************************************************/
void
startPPPD(struct ClientSession *session)
{
    if (UseLinuxKernelModePPPoE) startPPPDLinuxKernelMode(session);
    else startPPPDUserMode(session);
}

