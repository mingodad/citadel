/*
 * locate the originating host
 * $Id$
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <limits.h>
#include <netdb.h>
#include <string.h>
#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif
#include "citadel.h"
#include "server.h"
#include "locate_host.h"
#include "config.h"

void locate_host(char *tbuf)
{
	struct sockaddr_in cs;
	struct hostent *ch, *ch2;
	int len;
	char *i;
	int a1, a2, a3, a4;

	len = sizeof(cs);
	if (getpeername(CC->client_socket, (struct sockaddr *) &cs, &len) < 0) {
		strcpy(tbuf, config.c_fqdn);
		return;
	}
#ifdef HAVE_NONREENTRANT_NETDB
	begin_critical_section(S_NETDB);
#endif

	if ((ch = gethostbyaddr((char *) &cs.sin_addr, sizeof(cs.sin_addr),
				AF_INET)) == NULL) {
	      bad_dns:
		i = (char *) &cs.sin_addr;
		a1 = ((*i++) & 0xff);
		a2 = ((*i++) & 0xff);
		a3 = ((*i++) & 0xff);
		a4 = ((*i++) & 0xff);
		sprintf(tbuf, "%d.%d.%d.%d", a1, a2, a3, a4);
		goto end;	/* because we might need to end the critical
				   section */
	}
	/* check if the forward DNS agrees; if not, they're spoofing */
	if ((ch2 = gethostbyname(ch->h_name)) == NULL)
		goto bad_dns;

	/* check address for consistency */
	for (; *ch2->h_addr_list; ch2->h_addr_list++)
		if (!memcmp(*ch2->h_addr_list, &cs.sin_addr,
			    sizeof cs.sin_addr)) {
			strncpy(tbuf, ch->h_name, 24);
			goto end;
		}
	goto bad_dns;		/* they were spoofing. report a numeric IP
				   address. */

      end:

#ifdef HAVE_NONREENTRANT_NETDB
	end_critical_section(S_NETDB);
#endif

	tbuf[24] = 0;
}
