/*
 * $Id$
 *
 * locate the originating host
 *
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
#include "citadel.h"
#include "server.h"
#include "serv_extensions.h"
#include "locate_host.h"
#include "sysdep_decls.h"
#include "config.h"
#include "tools.h"
#include "domain.h"

void locate_host(char *tbuf, size_t n, const struct in_addr *addr)
{
	struct hostent *ch;
	const char *i;
	char *j;
	int a1, a2, a3, a4;

	lprintf(9, "locate_host() called\n");

#ifdef HAVE_NONREENTRANT_NETDB
	begin_critical_section(S_NETDB);
#endif

	if ((ch = gethostbyaddr((const char *) addr, sizeof(*addr), AF_INET)) ==
	    NULL) {
	      bad_dns:
		i = (const char *) addr;
		a1 = ((*i++) & 0xff);
		a2 = ((*i++) & 0xff);
		a3 = ((*i++) & 0xff);
		a4 = ((*i++) & 0xff);
		snprintf(tbuf, n, "%d.%d.%d.%d", a1, a2, a3, a4);
		goto end;	/* because we might need to end the critical
				   section */
	}
	/* check if the forward DNS agrees; if not, they're spoofing */
	j = strdoop(ch->h_name);
	ch = gethostbyname(j);
	phree(j);
	if (ch == NULL)
		goto bad_dns;

	/* check address for consistency */
	for (; *ch->h_addr_list; ch->h_addr_list++)
		if (!memcmp(*ch->h_addr_list, addr,
			    sizeof *addr)) {
			safestrncpy(tbuf, ch->h_name, 63);
			goto end;
		}
	goto bad_dns;		/* they were spoofing. report a numeric IP
				   address. */

      end:

#ifdef HAVE_NONREENTRANT_NETDB
	end_critical_section(S_NETDB);
#endif

	tbuf[63] = 0;
	lprintf(9, "locate_host() exiting\n");
}


/*
 * Check to see if a host is on some sort of spam list (RBL)
 * If spammer, returns nonzero and places reason in 'message_to_spammer'
 */
int rbl_check_addr(struct in_addr *addr, char *message_to_spammer)
{
	const char *i;
	int a1, a2, a3, a4;
	char tbuf[SIZ];
	int rbl;
	int num_rbl;
	char rbl_domains[SIZ];

	strcpy(message_to_spammer, "ok");

	i = (const char *) addr;
	a1 = ((*i++) & 0xff);
	a2 = ((*i++) & 0xff);
	a3 = ((*i++) & 0xff);
	a4 = ((*i++) & 0xff);

	/* See if we have any RBL domains configured */
	num_rbl = get_hosts(rbl_domains, "rbl");
	if (num_rbl < 1) return(0);

	/* Try all configured RBL's */
        for (rbl=0; rbl<num_rbl; ++rbl) {
		snprintf(tbuf, sizeof tbuf,
			"%d.%d.%d.%d.",
			a4, a3, a2, a1);
                extract(&tbuf[strlen(tbuf)], rbl_domains, rbl);

		if (gethostbyname(tbuf) != NULL) {
			strcpy(message_to_spammer,
		    		"5.7.1 Message rejected due to "
				"known spammer source IP address"
			);
			return(1);
		}
	}

	return(0);
}
			

/*
 * Check to see if the client host is on some sort of spam list (RBL)
 * If spammer, returns nonzero and places reason in 'message_to_spammer'
 */
int rbl_check(char *message_to_spammer) {
	struct sockaddr_in sin;
	int len;	/* should be socklen_t but doesn't work on Macintosh */

	if (!getpeername(CC->client_socket, (struct sockaddr *) &sin, &len)) {
		return(rbl_check_addr(&sin.sin_addr, message_to_spammer));
	}
	return(0);
}
