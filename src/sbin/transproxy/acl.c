/* vim:ai ts=4 sw=4
 *
 * acl.c:
 *
 * Functions to provide access control lists.
 */

#include <sys/types.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "acl.h"

#ifndef MAX_ACL
# define MAX_ACL	100
#endif

typedef struct
{
	unsigned long	ip;
	unsigned long	mask;
}	acl_t;

static unsigned long getclass(unsigned long ip);

static acl_t	acl[MAX_ACL];
static int		acl_length = 0;

void reset_acl(void)
{
	acl_length = 0;
}

/*
 * Add an ACL. Supported formats are:
 *  192.168.1.7		# host ACL
 *  192.168.1.0		# network ACL
 *  10.1.1.0/24		# subnet ACL (CIDR notation)
 */
int add_acl(char *acl_str)
{
	char			*cidr;
	int				bits;
	char			*value;
	char			*ptr;
	unsigned long	ip;
	unsigned long	mask;
	int				byte;
	int				shift;

	/*
	 * Check for CIDR notation.
	 */
	if ((cidr = strchr(acl_str, '/')) != NULL)
	{
		*cidr++ = '\0';
	}

	ip = 0L;
	shift = 24;
	value = acl_str;
	ptr = strchr(value, '.');
	while (ptr || (value && *value != '\0'))
	{
		if (ptr)
			*ptr++ = '\0';

		if (shift < 0)
			return (0);

		byte = atoi(value);
		ip |= (byte << shift);
		shift -= 8;

		value = ptr;
		if (value)
			ptr = strchr(value, '.');
	}

	if (cidr != NULL)
	{
		bits = atoi(cidr);
		if (bits >= 0 && bits <= 32)
			mask = 0xffffffffL << (32 - bits);
		else
			return (0);
	}
	else
	{
		mask = getclass(ip);

		if ((ip & mask) != ip)
			mask = 0xffffffffL;
	}

	acl[acl_length].ip = htonl(ip) & htonl(mask);
	acl[acl_length].mask = htonl(mask);
	++acl_length;

#ifdef DEBUG
	fprintf(stderr, "Added ACL 0x%08lx/0x%08lx\n", ip, mask);
#endif

	return (1);
}

/*
 * Check if an IP address matches our ACLs.
 */
int check_acl(struct sockaddr_in *addr)
{
	int				index;
	unsigned long	check_ip;

	/*
	 * First check if any ACLs have been defined.
	 */
	if (acl_length > 0)
	{
		/*
		 * Get the IP address to check.
		 */
		check_ip = addr->sin_addr.s_addr;

		/*
		 * Check the IP address against all ACLs and return 1 for
		 * the first correct match.
		 */
		for (index = 0; index < acl_length; ++index)
		{
			if ((check_ip & acl[index].mask) == acl[index].ip)
			{
#ifdef DEBUG
				fprintf(stderr, "0x%08lx matched 0x%08lx/0x%08lx\n",
					ntohl(check_ip), ntohl(acl[index].ip), ntohl(acl[index].mask));
#endif
				return (1);
			}
		}

#ifdef DEBUG
		fprintf(stderr, "0x%08lx not matched\n", ntohl(check_ip));
#endif

		/*
		 * If any ACLs have been defined, no match means that
		 * the connection should be refused.
		 */
		return (0);
	}

#ifdef DEBUG
	fprintf(stderr, "ACL always matched\n");
#endif

	/*
	 * If no ACLs have been defined, the connection should be allowed.
	 */
	return (1);
}

static unsigned long getclass(unsigned long ip)
{
	if ((ip & 0x80000000L) == 0)
		return (0xff000000L);
	else if ((ip & 0x40000000L) == 0)
		return (0xffff0000L);
	else
		return (0xffffff00L);
}
