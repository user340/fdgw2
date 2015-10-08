/***********************************************************************
*
* pppoe.c 
*
* Implementation of user-space PPPoE redirector for Linux.
*
* Copyright (C) 2000-2001 by Roaring Penguin Software Inc.
*
* This program may be distributed according to the terms of the GNU
* General Public License, version 2 or (at your option) any later version.
*
***********************************************************************/

static char const RCSID[] =
"$Id: pppoe.c,v 1.12 2001/01/29 17:37:20 dfs Exp $";

#include "pppoe.h"

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef USE_LINUX_PACKET
#include <sys/ioctl.h>
#include <fcntl.h>
#endif

#include <signal.h>

#ifdef HAVE_N_HDLC
#ifndef N_HDLC
#include <linux/termios.h>
#endif
#endif

/* Default interface if no -I option given */
#define DEFAULT_IF "eth0"

/* Global variables -- options */
int optInactivityTimeout = 0;	/* Inactivity timeout */
int optClampMSS          = 0;	/* Clamp MSS to this value */
int optSkipSession       = 0;	/* Perform discovery, print session info
				   and exit */

PPPoEConnection *Connection = NULL; /* Must be global -- used
				       in signal handler */
/***********************************************************************
*%FUNCTION: sendSessionPacket
*%ARGUMENTS:
* conn -- PPPoE connection
* packet -- the packet to send
* len -- length of data to send
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Transmits a session packet to the peer.
***********************************************************************/
void
sendSessionPacket(PPPoEConnection *conn, PPPoEPacket *packet, int len)
{
    packet->length = htons(len);
    if (optClampMSS) {
	clampMSS(packet, "outgoing", optClampMSS);
    }
    sendPacket(conn->sessionSocket, packet, len + HDR_SIZE);
    if (conn->debugFile) {
	fprintf(conn->debugFile, "SENT ");
	dumpPacket(conn->debugFile, packet);
	fprintf(conn->debugFile, "\n");
	fflush(conn->debugFile);
    }
}

#ifdef USE_BPF
/**********************************************************************
*%FUNCTION: sessionDiscoveryPacket
*%ARGUMENTS:
* packet -- the discovery packet that was received
*%RETURNS:
* Nothing
*%DESCRIPTION:
* We got a discovery packet during the session stage.  This most likely
* means a PADT.
*
* The BSD version uses a single socket for both discovery and session
* packets.  When a packet comes in over the wire once we are in
* session mode, either syncReadFromEth() or asyncReadFromEth() will
* have already read the packet and determined it to be a discovery
* packet before passing it here.
***********************************************************************/
void
sessionDiscoveryPacket(PPPoEConnection *conn, PPPoEPacket *packet)
{
    /* Sanity check */
    if (packet->code != CODE_PADT) {
	return;
    }

    /* It's a PADT, all right.  Is it for us? */
    if (packet->session != conn->session) {
	/* Nope, ignore it */
	return;
    }

    syslog(LOG_INFO,
	   "Session terminated -- received PADT from access concentrator");
    parsePacket(packet, parseLogErrs, NULL);
    exit(EXIT_SUCCESS);
}
#else
/**********************************************************************
*%FUNCTION: sessionDiscoveryPacket
*%ARGUMENTS:
* conn -- PPPoE connection
*%RETURNS:
* Nothing
*%DESCRIPTION:
* We got a discovery packet during the session stage.  This most likely
* means a PADT.
***********************************************************************/
void
sessionDiscoveryPacket(PPPoEConnection *conn)
{
    PPPoEPacket packet;
    int len;

    if (receivePacket(conn->discoverySocket, &packet, &len) < 0) {
	return;
    }

    /* Check length */
    if (ntohs(packet.length) + HDR_SIZE > len) {
	syslog(LOG_ERR, "Bogus PPPoE length field (%u)",
	       (unsigned int) ntohs(packet.length));
	return;
    }

    if (conn->debugFile) {
	fprintf(conn->debugFile, "RCVD ");
	dumpPacket(conn->debugFile, &packet);
	fprintf(conn->debugFile, "\n");
	fflush(conn->debugFile);
    }

    if (packet.code != CODE_PADT) {
	/* Not PADT; ignore it */
	return;
    }

    /* It's a PADT, all right.  Is it for us? */
    if (packet.session != conn->session) {
	/* Nope, ignore it */
	return;
    }

    syslog(LOG_INFO,
	   "Session terminated -- received PADT from peer");
    parsePacket(&packet, parseLogErrs, NULL);
    exit(EXIT_SUCCESS);
}
#endif /* USE_BPF */

/**********************************************************************
*%FUNCTION: session
*%ARGUMENTS:
* conn -- PPPoE connection info
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Handles the "session" phase of PPPoE
***********************************************************************/
void
session(PPPoEConnection *conn)
{
    fd_set readable;
    PPPoEPacket packet;
    struct timeval tv;
    struct timeval *tvp = NULL;
    int maxFD = 0;
    int r;

    /* Open a session socket */
    conn->sessionSocket = openInterface(conn->ifName, Eth_PPPOE_Session, conn->myEth);

    /* Prepare for select() */
    if (conn->sessionSocket > maxFD)   maxFD = conn->sessionSocket;
    if (conn->discoverySocket > maxFD) maxFD = conn->discoverySocket;
    maxFD++;

    /* Fill in the constant fields of the packet to save time */
    memcpy(packet.ethHdr.h_dest, conn->peerEth, ETH_ALEN);
    memcpy(packet.ethHdr.h_source, conn->myEth, ETH_ALEN);
    packet.ethHdr.h_proto = htons(Eth_PPPOE_Session);
    packet.ver = 1;
    packet.type = 1;
    packet.code = CODE_SESS;
    packet.session = conn->session;

    initPPP();

#ifdef USE_BPF
    /* check for buffered session data */
    while (BPF_BUFFER_HAS_DATA) {
	if (conn->synchronous) {
	    syncReadFromEth(conn, conn->sessionSocket, optClampMSS);
	} else {
	    asyncReadFromEth(conn, conn->sessionSocket, optClampMSS);
	}
    }
#endif

    for (;;) {
	if (optInactivityTimeout > 0) {
	    tv.tv_sec = optInactivityTimeout;
	    tv.tv_usec = 0;
	    tvp = &tv;
	}
	FD_ZERO(&readable);
	FD_SET(0, &readable);     /* ppp packets come from stdin */
	FD_SET(conn->discoverySocket, &readable);
	FD_SET(conn->sessionSocket, &readable);
	while(1) {
	    r = select(maxFD, &readable, NULL, NULL, tvp);
	    if (r >= 0 || errno != EINTR) break;
	}
	if (r < 0) {
	    fatalSys("select (session)");
	}
	if (r == 0) { /* Inactivity timeout */
	    syslog(LOG_ERR, "Inactivity timeout... something wicked happened");
	    sendPADT(conn, "RP-PPPoE: Inactivity timeout");
	    exit(EXIT_FAILURE);
	}

	/* Handle ready sockets */
	if (FD_ISSET(0, &readable)) {
	    if (conn->synchronous) {
		syncReadFromPPP(conn, &packet);
	    } else {
		asyncReadFromPPP(conn, &packet);
	    }
	}

	if (FD_ISSET(conn->sessionSocket, &readable)) {
	    do {
		if (conn->synchronous) {
		    syncReadFromEth(conn, conn->sessionSocket, optClampMSS);
		} else {
		    asyncReadFromEth(conn, conn->sessionSocket, optClampMSS);
		}
	    } while (BPF_BUFFER_HAS_DATA);
	}

#ifndef USE_BPF	
	/* BSD uses a single socket, see *syncReadFromEth() */
	/* for calls to sessionDiscoveryPacket() */
	if (FD_ISSET(conn->discoverySocket, &readable)) {
	    sessionDiscoveryPacket(conn);
	}
#endif

    }
}


/***********************************************************************
*%FUNCTION: sigPADT
*%ARGUMENTS:
* src -- signal received
*%RETURNS:
* Nothing
*%DESCRIPTION:
* If an established session exists send PADT to terminate from session
*  from our end
***********************************************************************/
void
sigPADT(int src)
{
  syslog(LOG_DEBUG,"Received signal %d.",(int)src);
  sendPADT(Connection, "RP-PPPoE: Received signal");
  exit(EXIT_SUCCESS);
}

/**********************************************************************
*%FUNCTION: usage
*%ARGUMENTS:
* argv0 -- program name
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Prints usage information and exits.
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
    fprintf(stderr, "   -D filename    -- Log debugging information in filename.\n");
    fprintf(stderr, "   -V             -- Print version and exit.\n");
    fprintf(stderr, "   -A             -- Print access concentrator names and exit.\n");
    fprintf(stderr, "   -S name        -- Set desired service name.\n");
    fprintf(stderr, "   -C name        -- Set desired access concentrator name.\n");
    fprintf(stderr, "   -U             -- Use Host-Unique to allow multiple PPPoE sessions.\n");
    fprintf(stderr, "   -s             -- Use synchronous PPP encapsulation.\n");
    fprintf(stderr, "   -m MSS         -- Clamp incoming and outgoing MSS options.\n");
    fprintf(stderr, "   -p pidfile     -- Write process-ID to pidfile.\n");
    fprintf(stderr, "   -e sess:mac    -- Skip discovery phase; use existing session.\n");
    fprintf(stderr, "   -k             -- Kill a session with PADT (requires -e)\n");
    fprintf(stderr, "   -d             -- Perform discovery, print session info and exit.\n");
    fprintf(stderr, "   -f disc:sess   -- Set Ethernet frame types (hex).\n");
    fprintf(stderr, "   -h             -- Print usage information.\n\n");
    fprintf(stderr, "PPPoE Version %s, Copyright (C) 2001 Roaring Penguin Software Inc.\n", VERSION);
    fprintf(stderr, "PPPoE comes with ABSOLUTELY NO WARRANTY.\n");
    fprintf(stderr, "This is free software, and you are welcome to redistribute it under the terms\n");
    fprintf(stderr, "of the GNU General Public License, version 2 or any later version.\n");
    fprintf(stderr, "http://www.roaringpenguin.com\n");
    exit(EXIT_SUCCESS);
}

/**********************************************************************
*%FUNCTION: main
*%ARGUMENTS:
* argc, argv -- count and values of command-line arguments
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Main program
***********************************************************************/
int
main(int argc, char *argv[])
{
    int opt;
    int n;
    unsigned int m[6];		/* MAC address in -e option */
    unsigned int s;		/* Temporary to hold session */
    FILE *pidfile;
    unsigned int discoveryType, sessionType;

    PPPoEConnection conn;

#ifdef HAVE_N_HDLC
    int disc = N_HDLC;
    long flags;
#endif

    /* Initialize connection info */
    memset(&conn, 0, sizeof(conn));
    conn.discoverySocket = -1;
    conn.sessionSocket = -1;

    /* For signal handler */
    Connection = &conn;

    /* Initialize syslog */
    openlog("pppoe", LOG_PID, LOG_DAEMON);

    while((opt = getopt(argc, argv, "I:VAT:D:hS:C:Usm:p:e:kdf:")) != -1) {
	switch(opt) {
	case 'f':
	    if (sscanf(optarg, "%x:%x", &discoveryType, &sessionType) != 2) {
		fprintf(stderr, "Illegal argument to -f: Should be disc:sess in hex\n");
		exit(EXIT_FAILURE);
	    }
	    Eth_PPPOE_Discovery = (UINT16_t) discoveryType;
	    Eth_PPPOE_Session   = (UINT16_t) sessionType;
	    break;
	case 'd':
	    optSkipSession = 1;
	    break;

	case 'k':
	    conn.killSession = 1;
	    break;

	case 'e':
	    /* Existing session: "sess:xx:yy:zz:aa:bb:cc" where "sess" is
	       session-ID, and xx:yy:zz:aa:bb:cc is MAC-address of peer */
	    n = sscanf(optarg, "%u:%2x:%2x:%2x:%2x:%2x:%2x",
		       &s, &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]);
	    if (n != 7) {
		fprintf(stderr, "Illegal argument to -e: Should be sess:xx:yy:zz:aa:bb:cc\n");
		exit(EXIT_FAILURE);
	    }

	    /* Copy MAC address of peer */
	    for (n=0; n<6; n++) {
		conn.peerEth[n] = (unsigned char) m[n];
	    }

	    /* Convert session */
	    conn.session = htons(s);

	    /* Skip discovery phase! */
	    conn.skipDiscovery = 1;
	    break;
		       
	case 'p':
	    pidfile = fopen(optarg, "w");
	    if (pidfile) {
		fprintf(pidfile, "%lu\n", (unsigned long) getpid());
		fclose(pidfile);
	    }
	    break;
	case 'S':
	    SET_STRING(conn.serviceName, optarg);
	    break;
	case 'C':
	    SET_STRING(conn.acName, optarg);
	    break;
	case 's':
	    conn.synchronous = 1;
	    break;
	case 'U':
	    conn.useHostUniq = 1;
	    break;
	case 'D':
	    conn.debugFile = fopen(optarg, "w");
	    if (!conn.debugFile) {
		fprintf(stderr, "Could not open %s: %s\n",
			optarg, strerror(errno));
		exit(EXIT_FAILURE);
	    }
	    fprintf(conn.debugFile, "rp-pppoe-%s\n", VERSION);
	    fflush(conn.debugFile);
	    break;
	case 'T':
	    optInactivityTimeout = (int) strtol(optarg, NULL, 10);
	    if (optInactivityTimeout < 0) {
		optInactivityTimeout = 0;
	    }
	    break;
	case 'm':
	    optClampMSS = (int) strtol(optarg, NULL, 10);
	    if (optClampMSS < 536) {
		fprintf(stderr, "-m: %d is too low (min 536)\n", optClampMSS);
		exit(EXIT_FAILURE);
	    }
	    if (optClampMSS > 1452) {
		fprintf(stderr, "-m: %d is too high (max 1452)\n", optClampMSS);
		exit(EXIT_FAILURE);
	    }
	    break;
	case 'I':
	    SET_STRING(conn.ifName, optarg);
	    break;
	case 'V':
	    printf("Roaring Penguin PPPoE Version %s\n", VERSION);
	    exit(EXIT_SUCCESS);
	case 'A':
	    conn.printACNames = 1;
	    break;
	case 'h':
	    usage(argv[0]);
	    break;
	default:
	    usage(argv[0]);
	}
    }

    /* Pick a default interface name */
    if (!conn.ifName) {
#ifdef USE_BPF
	fprintf(stderr, "No interface specified (-I option)\n");
	exit(EXIT_FAILURE);
#else
	SET_STRING(conn.ifName, DEFAULT_IF);
#endif
    }

    /* Set signal handlers: send PADT on TERM, HUP and INT */
    if (!conn.printACNames) {
	signal(SIGTERM, sigPADT);
	signal(SIGHUP, sigPADT);
	signal(SIGINT, sigPADT);

#ifdef HAVE_N_HDLC
	if (conn.synchronous) {
	    if (ioctl(0, TIOCSETD, &disc) < 0) {
		printErr("Unable to set line discipline to N_HDLC -- synchronous mode probably will fail");
	    } else {
		syslog(LOG_INFO,
		       "Changed pty line discipline to N_HDLC for synchronous mode");
	    }
	    /* There is a bug in Linux's select which returns a descriptor
	     * as readable if N_HDLC line discipline is on, even if
	     * it isn't really readable.  This return happens only when
	     * select() times out.  To avoid blocking forever in read(),
	     * make descriptor 0 non-blocking */
	    flags = fcntl(0, F_GETFL);
	    if (flags < 0) fatalSys("fcntl(F_GETFL)");
	    if (fcntl(0, F_SETFL, (long) flags | O_NONBLOCK) < 0) {
		fatalSys("fcntl(F_SETFL)");
	    }
	}
#endif

    }

    discovery(&conn);
    if (optSkipSession) {
	printf("%u:%02x:%02x:%02x:%02x:%02x:%02x\n",
	       ntohs(conn.session),
	       conn.peerEth[0],
	       conn.peerEth[1],
	       conn.peerEth[2],
	       conn.peerEth[3],
	       conn.peerEth[4],
	       conn.peerEth[5]);
	exit(EXIT_SUCCESS);
    }
    session(&conn);
    return 0;
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
    char buf[1024];
    sprintf(buf, "%.256s: %.256s", str, strerror(errno));
    printErr(buf);
    sendPADT(Connection, "RP-PPPoE: System call error");
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
    sendPADT(Connection, str);
    exit(EXIT_FAILURE);
}

/**********************************************************************
*%FUNCTION: asyncReadFromEth
*%ARGUMENTS:
* conn -- PPPoE connection info
* sock -- Ethernet socket
* clampMss -- if non-zero, do MSS-clamping
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Reads a packet from the Ethernet interface and sends it to async PPP
* device.
***********************************************************************/
void
asyncReadFromEth(PPPoEConnection *conn, int sock, int clampMss)
{
    PPPoEPacket packet;
    int len;
    int plen;
    int i;
    unsigned char pppBuf[4096];
    unsigned char *ptr = pppBuf;
    unsigned char c;
    UINT16_t fcs;
    unsigned char header[2] = {FRAME_ADDR, FRAME_CTRL};
    unsigned char tail[2];
#ifdef USE_BPF
    int type;
#endif

    if (receivePacket(sock, &packet, &len) < 0) {
	return;
    }

    /* Check length */
    if (ntohs(packet.length) + HDR_SIZE > len) {
	syslog(LOG_ERR, "Bogus PPPoE length field (%u)",
	       (unsigned int) ntohs(packet.length));
	return;
    }
    if (conn->debugFile) {
	fprintf(conn->debugFile, "RCVD ");
	dumpPacket(conn->debugFile, &packet);
	fprintf(conn->debugFile, "\n");
	fflush(conn->debugFile);
    }

#ifdef USE_BPF
    /* Make sure this is a session packet before processing further */
    type = etherType(&packet);
    if (type == Eth_PPPOE_Discovery) {
	sessionDiscoveryPacket(conn, &packet);
    } else if (type != Eth_PPPOE_Session) {
	return;
    }
#endif

    /* Sanity check */
    if (packet.code != CODE_SESS) {
	syslog(LOG_ERR, "Unexpected packet code %d", (int) packet.code);
	return;
    }
    if (packet.ver != 1) {
	syslog(LOG_ERR, "Unexpected packet version %d", (int) packet.ver);
	return;
    }
    if (packet.type != 1) {
	syslog(LOG_ERR, "Unexpected packet type %d", (int) packet.type);
	return;
    }
    if (memcmp(packet.ethHdr.h_source, conn->peerEth, ETH_ALEN)) {
	/* Not for us -- must be another session.  This is not an error,
	   so don't log anything.  */
	return;
    }

    if (packet.session != conn->session) {
	/* Not for us -- must be another session.  This is not an error,
	   so don't log anything.  */
	return;
    }
    plen = ntohs(packet.length);
    if (plen + HDR_SIZE > len) {
	syslog(LOG_ERR, "Bogus length field in session packet %d (%d)",
	       (int) plen, (int) len);
	return;
    }

    /* Clamp MSS */
    if (clampMss) {
	clampMSS(&packet, "incoming", clampMss);
    }

    /* Compute FCS */
    fcs = pppFCS16(PPPINITFCS16, header, 2);
    fcs = pppFCS16(fcs, packet.payload, plen) ^ 0xffff;
    tail[0] = fcs & 0x00ff;
    tail[1] = (fcs >> 8) & 0x00ff;

    /* Build a buffer to send to PPP */
    *ptr++ = FRAME_FLAG;
    *ptr++ = FRAME_ADDR;
    *ptr++ = FRAME_ESC;
    *ptr++ = FRAME_CTRL ^ FRAME_ENC;

    for (i=0; i<plen; i++) {
	c = packet.payload[i];
	if (c == FRAME_FLAG || c == FRAME_ADDR || c == FRAME_ESC || c < 0x20) {
	    *ptr++ = FRAME_ESC;
	    *ptr++ = c ^ FRAME_ENC;
	} else {
	    *ptr++ = c;
	}
    }
    for (i=0; i<2; i++) {
	c = tail[i];
	if (c == FRAME_FLAG || c == FRAME_ADDR || c == FRAME_ESC || c < 0x20) {
	    *ptr++ = FRAME_ESC;
	    *ptr++ = c ^ FRAME_ENC;
	} else {
	    *ptr++ = c;
	}
    }
    *ptr++ = FRAME_FLAG;

    /* Ship it out */
    if (write(1, pppBuf, (ptr-pppBuf)) < 0) {
	fatalSys("asyncReadFromEth: write");
    }
}

/**********************************************************************
*%FUNCTION: syncReadFromEth
*%ARGUMENTS:
* conn -- PPPoE connection info
* sock -- Ethernet socket
* clampMss -- if true, clamp MSS.
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Reads a packet from the Ethernet interface and sends it to sync PPP
* device.
***********************************************************************/
void
syncReadFromEth(PPPoEConnection *conn, int sock, int clampMss)
{
    PPPoEPacket packet;
    int len;
    int plen;
    struct iovec vec[2];
    unsigned char dummy[2];
#ifdef USE_BPF
    int type;
#endif

    if (receivePacket(sock, &packet, &len) < 0) {
	return;
    }

    /* Check length */
    if (ntohs(packet.length) + HDR_SIZE > len) {
	syslog(LOG_ERR, "Bogus PPPoE length field (%u)",
	       (unsigned int) ntohs(packet.length));
	return;
    }
    if (conn->debugFile) {
	fprintf(conn->debugFile, "RCVD ");
	dumpPacket(conn->debugFile, &packet);
	fprintf(conn->debugFile, "\n");
	fflush(conn->debugFile);
    }

#ifdef USE_BPF
    /* Make sure this is a session packet before processing further */
    type = etherType(&packet);
    if (type == Eth_PPPOE_Discovery) {
	sessionDiscoveryPacket(conn, &packet);
    } else if (type != Eth_PPPOE_Session) {
	return;
    }
#endif

    /* Sanity check */
    if (packet.code != CODE_SESS) {
	syslog(LOG_ERR, "Unexpected packet code %d", (int) packet.code);
	return;
    }
    if (packet.ver != 1) {
	syslog(LOG_ERR, "Unexpected packet version %d", (int) packet.ver);
	return;
    }
    if (packet.type != 1) {
	syslog(LOG_ERR, "Unexpected packet type %d", (int) packet.type);
	return;
    }
    if (memcmp(packet.ethHdr.h_source, conn->peerEth, ETH_ALEN)) {
	/* Not for us -- must be another session.  This is not an error,
	   so don't log anything.  */
	return;
    }
    if (packet.session != conn->session) {
	/* Not for us -- must be another session.  This is not an error,
	   so don't log anything.  */
	return;
    }
    plen = ntohs(packet.length);
    if (plen + HDR_SIZE > len) {
	syslog(LOG_ERR, "Bogus length field in session packet %d (%d)",
	       (int) plen, (int) len);
	return;
    }

    /* Clamp MSS */
    if (clampMss) {
	clampMSS(&packet, "incoming", clampMss);
    }

    /* Ship it out */
    vec[0].iov_base = (void *) dummy;
    dummy[0] = FRAME_ADDR;
    dummy[1] = FRAME_CTRL;
    vec[0].iov_len = 2;
    vec[1].iov_base = (void *) packet.payload;
    vec[1].iov_len = plen;

    if (writev(1, vec, 2) < 0) {
	fatalSys("syncReadFromEth: write");
    }
}

