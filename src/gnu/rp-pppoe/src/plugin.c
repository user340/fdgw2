/***********************************************************************
*
* plugin.c
*
* pppd plugin for kernel-mode PPPoE on Linux
*
* Copyright (C) 2001 by Roaring Penguin Software Inc., Michal Ostrowski
* and Jamal Hadi Salim.
* 
* Much code and many ideas derived from pppoe plugin by Michal
* Ostrowski and Jamal Hadi Salim, which carries this copyright:
*
* Copyright 2000 Michal Ostrowski <mostrows@styx.uwaterloo.ca>,
*                Jamal Hadi Salim <hadi@cyberus.ca>
* Borrows heavily from the PPPoATM plugin by Mitchell Blank Jr., 
* which is based in part on work from Jens Axboe and Paul Mackerras.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version
* 2 of the License, or (at your option) any later version.
***********************************************************************/

static char const RCSID[] =
"$Id: plugin.c,v 1.8 2001/01/29 17:37:20 dfs Exp $";

#define _GNU_SOURCE 1
#include "pppoe.h"

#include "pppd/pppd.h"
#include "pppd/fsm.h"
#include "pppd/lcp.h"
#include "pppd/ipcp.h"
#include "pppd/ccp.h"
#include "pppd/pathnames.h"

#include <linux/types.h>
#include <syslog.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <net/ethernet.h>
#include <net/if_arp.h>
#include <linux/if_pppox.h>

/* From sys-linux.c in pppd -- MUST FIX THIS! */
extern int new_style_driver;

static char *service = NULL;
static char *acName = NULL;
static char *existingSession = NULL;

static option_t Options[] = {
    { "rp_pppoe_service", o_string, &service,
      "Desired PPPoE service name" },
    { "rp_pppoe_ac",      o_string, &acName,
      "Desired PPPoE access concentrator name" },
    { "rp_pppoe_sess",    o_string, &existingSession,
      "Attach to existing session (sessid:macaddr)" },
    { NULL }
};
int (*OldDevnameHook)(const char *name) = NULL;
static PPPoEConnection *conn = NULL;

/**********************************************************************
 * %FUNCTION: PPPOEInitDevice
 * %ARGUMENTS:
 * None
 * %RETURNS:
 * 
 * %DESCRIPTION:
 * Initializes PPPoE device.
 ***********************************************************************/
static int
PPPOEInitDevice(void)
{
    conn = malloc(sizeof(PPPoEConnection));
    if (!conn) {
	fatal("Could not allocate memory for PPPoE session");
    }
    memset(conn, 0, sizeof(PPPoEConnection));
    if (acName) {
	SET_STRING(conn->acName, acName);
    }
    if (service) {
	SET_STRING(conn->serviceName, acName);
    }
    SET_STRING(conn->ifName, devnam);
    conn->discoverySocket = -1;
    conn->sessionSocket = -1;
    conn->useHostUniq = 1;
    return 1;
}

/**********************************************************************
 * %FUNCTION: PPPOEConnectDevice
 * %ARGUMENTS:
 * None
 * %RETURNS:
 * Non-negative if all goes well; -1 otherwise
 * %DESCRIPTION:
 * Connects PPPoE device.
 ***********************************************************************/
static int
PPPOEConnectDevice(void)
{
    struct sockaddr_pppox sp;

    strlcpy(ppp_devnam, devnam, sizeof(ppp_devnam));
    if (existingSession) {
	unsigned int mac[ETH_ALEN];
	int i, ses;
	if (sscanf(existingSession, "%d:%x:%x:%x:%x:%x:%x",
		   &ses, &mac[0], &mac[1], &mac[2],
		   &mac[3], &mac[4], &mac[5]) != 7) {
	    fatal("Illegal value for rp_pppoe_sess option");
	}
	conn->session = htons(ses);
	for (i=0; i<ETH_ALEN; i++) {
	    conn->peerEth[i] = (unsigned char) mac[i];
	}
    } else {
	discovery(conn);
	if (conn->discoveryState != STATE_SESSION) {
	    fatal("Unable to complete PPPoE Discovery");
	}
    }

    /* Make the session socket */
    conn->sessionSocket = socket(AF_PPPOX, SOCK_STREAM, PX_PROTO_OE);
    if (conn->sessionSocket < 0) {
	fatal("Failed to create PPPoE socket: %m");
    }
    sp.sa_family = AF_PPPOX;
    sp.sa_protocol = PX_PROTO_OE;
    sp.sa_addr.pppoe.sid = conn->session;
    memcpy(sp.sa_addr.pppoe.dev, conn->ifName, IFNAMSIZ);
    memcpy(sp.sa_addr.pppoe.remote, conn->peerEth, ETH_ALEN);
    if (connect(conn->sessionSocket, (struct sockaddr *) &sp,
		sizeof(struct sockaddr_pppox)) < 0) {
	fatal("Failed to connect PPPoE socket: %d %m", errno);
	return -1;
    }
    return conn->sessionSocket;
}

static void
PPPOESendConfig(int unit, 
		int mtu, 
		u_int32_t asyncmap, 
		int pcomp, 
		int accomp)
{
    int sock;
    struct ifreq ifr;
    
    if (mtu > MAX_PPPOE_MTU) {
	warn("Couldn't increase MTU to %d", mtu);
    }
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
	fatal("Couldn't create IP socket: %m");
    }
    strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
    ifr.ifr_mtu = mtu;
    if (ioctl(sock, SIOCSIFMTU, &ifr) < 0) {
	fatal("ioctl(SIOCSIFMTU): %m");
    }
    (void) close (sock);
}


static void
PPPOERecvConfig(int unit, 
		int mru, 
		u_int32_t asyncmap, 
		int pcomp, 
		int accomp)
{
    if (mru > MAX_PPPOE_MTU) {
	error("Couldn't increase MRU to %d", mru);
    }
}

static void
PPPOESetXaccm(int unit,
	      ext_accm accm)
{
    /* Do nothing */
}

/**********************************************************************
 * %FUNCTION: PPPOEDisconnectDevice
 * %ARGUMENTS:
 * None
 * %RETURNS:
 * Nothing
 * %DESCRIPTION:
 * Disconnects PPPoE device
 ***********************************************************************/
static void
PPPOEDisconnectDevice(void)
{
    struct sockaddr_pppox sp;

    sp.sa_family = AF_PPPOX;
    sp.sa_protocol = PX_PROTO_OE;
    sp.sa_addr.pppoe.sid = 0;
    memcpy(sp.sa_addr.pppoe.dev, conn->ifName, IFNAMSIZ);
    memcpy(sp.sa_addr.pppoe.remote, conn->peerEth, ETH_ALEN);
    if (connect(conn->sessionSocket, (struct sockaddr *) &sp,
		sizeof(struct sockaddr_pppox)) < 0) {
	fatal("Failed to disconnect PPPoE socket: %d %m", errno);
	return;
    }
    close(conn->sessionSocket);
}

static int
PPPOESetSpeed(const char *speed)
{
    return 0;
}

static void
PPPOEDeviceCheckHook(void)
{
    if (!options_for_dev(_PATH_ETHOPT, devnam)) {
	exit(EXIT_OPTION_ERROR);
    }
}

/**********************************************************************
 * %FUNCTION: PPPoEDevnameHook
 * %ARGUMENTS:
 * name -- name of device
 * %RETURNS:
 * 1 if we will handle this device; 0 otherwise.
 * %DESCRIPTION:
 * Checks if name is a valid interface name; if so, returns 1.  Also
 * sets up devnam (string representation of device) and sets devstat.st_mode
 * so S_ISCHR(devstat.st_mode) != 1 for internal pppd consumption.
 ***********************************************************************/
static int
PPPoEDevnameHook(const char *name)
{
    int r = 1;
    int fd;
    struct ifreq ifr;

    /* Open a socket */
    if ((fd = socket(PF_PACKET, SOCK_RAW, 0)) < 0) {
	r = 0;
    }

    /* Try getting interface index */
    if (r) {
	strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
	    r = 0;
	} else {
	    if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) {
		r = 0;
	    } else {
		if (ifr.ifr_hwaddr.sa_family != ARPHRD_ETHER) {
		    error("Interface %s not Ethernet", name);
		    r=0;
		}
	    }
	}
    }

    /* Close socket */
    close(fd);
    if (r) {
	strncpy(devnam, name, sizeof(devnam));
	if (device_check_hook != PPPOEDeviceCheckHook) {
	    devstat.st_mode = S_IFSOCK;
	    device_init_hook = PPPOEInitDevice;
	    setspeed_hook = PPPOESetSpeed;
	    device_check_hook = PPPOEDeviceCheckHook;
	    connect_device_hook = PPPOEConnectDevice;
	    disconnect_device_hook = PPPOEDisconnectDevice;
	    send_config_hook = PPPOESendConfig;
	    recv_config_hook = PPPOERecvConfig;
	    set_xaccm_hook = PPPOESetXaccm;
	    modem = 0;
	    
	    lcp_allowoptions[0].neg_accompression = 0;
	    lcp_wantoptions[0].neg_accompression = 0;
	    
	    lcp_allowoptions[0].neg_asyncmap = 0;
	    lcp_wantoptions[0].neg_asyncmap = 0;
	    
	    lcp_allowoptions[0].neg_pcompression = 0;
	    lcp_wantoptions[0].neg_pcompression = 0;
	    
	    ccp_allowoptions[0].deflate = 0 ;
	    ccp_wantoptions[0].deflate = 0 ;
	    
	    ipcp_allowoptions[0].neg_vj=0;
	    ipcp_wantoptions[0].neg_vj=0;
	    
	    ccp_allowoptions[0].bsd_compress = 0;
	    ccp_wantoptions[0].bsd_compress = 0;
	    
	    PPPOEInitDevice();
	}
	return 1;
    }

    if (OldDevnameHook) r = OldDevnameHook(name);
    return r;
}

/**********************************************************************
 * %FUNCTION: plugin_init
 * %ARGUMENTS:
 * None
 * %RETURNS:
 * Nothing
 * %DESCRIPTION:
 * Initializes hooks for pppd plugin
 ***********************************************************************/
void
plugin_init(void)
{
    if (!new_style_driver) {
	fatal("Linux kernel does not support PPPoE -- are you running 2.4.x?");
    }
    OldDevnameHook = setdevname_hook;
    setdevname_hook = PPPoEDevnameHook;
    add_options(Options);

    info("Roaring Penguin PPPoE Plugin Initialized");
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
    sendPADT(conn, str);
    exit(1);
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
    sendPADT(conn, str);
    exit(1);
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
    rp_fatal(str);
}
