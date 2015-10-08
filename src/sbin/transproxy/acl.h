/*
 * acl.h:
 *
 * Functions to provide access control lists.
 */

#include <sys/types.h>
#include <netinet/in.h>

void reset_acl(void);
int add_acl(char *acl);
int check_acl(struct sockaddr_in *addr);
