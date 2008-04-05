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
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <limits.h>
#include <netdb.h>
#include <string.h>
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "locate_host.h"
#include "sysdep_decls.h"
#include "config.h"
#include "domain.h"
#include "ctdl_module.h"

#ifdef HAVE_RESOLV_H
#include <arpa/nameser.h>
#ifdef HAVE_ARPA_NAMESER_COMPAT_H
#include <arpa/nameser_compat.h>
#endif
#include <resolv.h>
#endif


/* Hacks to work around nameser.h declarations missing on OpenBSD
 * see also: http://search.cpan.org/src/MIKER/Net-DNS-ToolKit-0.30/ToolKit.h
 */

#ifndef NS_INT16SZ
# ifdef INT16SZ
#  define NS_INT16SZ INT16SZ
# endif
#endif

#ifndef NS_INT32SZ
# ifdef INT32SZ
#  define NS_INT32SZ INT32SZ
# endif
#endif

#ifndef NS_GET16
# ifdef GETSHORT
#  define NS_GET16 GETSHORT
# endif
#endif


/***************************************************************************/


void locate_host(char *tbuf, size_t n,
		char *abuf, size_t na,
		const struct in_addr *addr)
{
	struct hostent *ch;
	const char *i;
	char *j;
	int a1, a2, a3, a4;
	char address_string[SIZ];


#ifdef HAVE_NONREENTRANT_NETDB
	begin_critical_section(S_NETDB);
#endif

	i = (const char *) addr;
	a1 = ((*i++) & 0xff);
	a2 = ((*i++) & 0xff);
	a3 = ((*i++) & 0xff);
	a4 = ((*i++) & 0xff);
	sprintf(address_string, "%d.%d.%d.%d", a1, a2, a3, a4);

	if (abuf != NULL) {
		safestrncpy(abuf, address_string, na);
	}

	if ((ch = gethostbyaddr((const char *) addr,
	   sizeof(*addr), AF_INET)) == NULL) {
bad_dns:
		safestrncpy(tbuf, address_string, n);
		goto end;	/* because we might need to end the critical
				   section */
	}
	/* check if the forward DNS agrees; if not, they're spoofing */
	j = strdup(ch->h_name);
	ch = gethostbyname(j);
	free(j);
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
}


/*
 * RBL check written by Edward S. Marshall [http://rblcheck.sourceforge.net]
 */
#define RESULT_SIZE 4096 /* What is the longest result text we support? */
int rblcheck_backend(char *domain, char *txtbuf, int txtbufsize) {
	int a, b, c;
	char *result = NULL;
	u_char fixedans[ PACKETSZ ];
	u_char *answer;
	int need_to_free_answer = 0;
	const u_char *cp;
	u_char *rp;
	const u_char *cend;
	const u_char *rend;
	int len;
	char *p = NULL;

	/* Make our DNS query. */
	//res_init();
	answer = fixedans;
	if (CtdlThreadCheckStop())
	{
		if (txtbuf != NULL)
			snprintf(txtbuf, txtbufsize, "System shutting down");
		return (1);
	}
	len = res_query( domain, C_IN, T_A, answer, PACKETSZ );

	/* Was there a problem? If so, the domain doesn't exist. */
	if( len == -1 ) {
		if (txtbuf != NULL) {
			strcpy(txtbuf, "");
		}
		return(0);
	}

	if( len > PACKETSZ )
	{
		answer = malloc( len );
		need_to_free_answer = 1;
		len = res_query( domain, C_IN, T_A, answer, len );
		if( len == -1 ) {
			if (txtbuf != NULL) {
				snprintf(txtbuf, txtbufsize,
					"Message rejected due to known spammer source IP address");
			}
			if (need_to_free_answer) free(answer);
			return(1);
		}
	}
	if (CtdlThreadCheckStop())
	{
		if (txtbuf != NULL)
			snprintf(txtbuf, txtbufsize, "System shutting down");
		if (need_to_free_answer) free(answer);
		return (1);
	}

	result = ( char * )malloc( RESULT_SIZE );
	result[ 0 ] = '\0';


	/* Make another DNS query for textual data; this shouldn't
	   be a performance hit, since it'll now be cached at the
	   nameserver we're using. */
	res_init();
	len = res_query( domain, C_IN, T_TXT, answer, PACKETSZ );
	if (CtdlThreadCheckStop())
	{
		if (txtbuf != NULL)
			snprintf(txtbuf, txtbufsize, "System shutting down");
		if (need_to_free_answer) free(answer);
		free(result);
		return (1);
	}

	/* Just in case there's no TXT record... */
	if( len == -1 )
	{
		if (txtbuf != NULL) {
			snprintf(txtbuf, txtbufsize,
				"Message rejected due to known spammer source IP address");
		}
		if (need_to_free_answer) free(answer);
		free(result);
		return(1);
	}

	/* Skip the header and the address we queried. */
	cp = answer + sizeof( HEADER );
	while( *cp != '\0' )
	{
		a = *cp++;
		while( a-- )
			cp++;
	}

	/* This seems to be a bit of magic data that we need to
	   skip. I wish there were good online documentation
	   for programming for libresolv, so I'd know what I'm
	   skipping here. Anyone reading this, feel free to
	   enlighten me. */
	cp += 1 + NS_INT16SZ + NS_INT32SZ;

	/* Skip the type, class and ttl. */
	cp += ( NS_INT16SZ * 2 ) + NS_INT32SZ;

	/* Get the length and end of the buffer. */
	NS_GET16( c, cp );
	cend = cp + c;

	/* Iterate over any multiple answers we might have. In
	   this context, it's unlikely, but anyway. */
	rp = (u_char *) result;
	rend = (u_char *) result + RESULT_SIZE - 1;
	while( cp < cend && rp < rend )
	{
		a = *cp++;
		if( a != 0 )
			for( b = a; b > 0 && cp < cend && rp < rend;
			  b-- )
			{
				if( *cp == '\n' || *cp == '"' ||
				  *cp == '\\' )
				{
					*rp++ = '\\';
				}
				*rp++ = *cp++;
			}
	}
	*rp = '\0';
	if (txtbuf != NULL) {
		snprintf(txtbuf, txtbufsize, "%s", result);
	}
	/* Remove nonprintable characters */
	for (p=txtbuf; *p; ++p) {
		if (!isprint(*p)) strcpy(p, p+1);
	}
	if (need_to_free_answer) free(answer);
	free(result);
	return(1);
}


/*
 * Check to see if a host is on some sort of spam list (RBL)
 * If spammer, returns nonzero and places reason in 'message_to_spammer'
 */
int rbl_check_addr(struct in_addr *addr, char *message_to_spammer)
{
	const char *i;
	int a1, a2, a3, a4;
	char tbuf[256];
	int rbl;
	int num_rbl;
	char rbl_domains[SIZ];
	char txt_answer[1024];

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
                extract_token(&tbuf[strlen(tbuf)], rbl_domains, rbl, '|', (sizeof tbuf - strlen(tbuf)));

		if (rblcheck_backend(tbuf, txt_answer, sizeof txt_answer)) {
			sprintf(message_to_spammer, "5.7.1 %s", txt_answer);
			CtdlLogPrintf(CTDL_INFO, "RBL: %s\n", txt_answer);
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
	struct timeval tv1, tv2;
	suseconds_t total_time = 0;

	gettimeofday(&tv1, NULL);
	len = 0;
	memset (&sin, 0, sizeof (struct sockaddr_in));
	if (!getpeername(CC->client_socket, (struct sockaddr *) &sin, (socklen_t *)&len)) {
		return(rbl_check_addr(&sin.sin_addr, message_to_spammer));
	}
	
	gettimeofday(&tv2, NULL);
	total_time = (tv2.tv_usec + (tv2.tv_sec * 1000000)) - (tv1.tv_usec + (tv1.tv_sec * 1000000));
	CtdlLogPrintf(CTDL_DEBUG, "RBL check completed in %ld.%ld seconds\n",
		(total_time / 1000000),
		(total_time % 1000000)
	);
	return(0);
}

/*
 * Convert a host name to a dotted quad address. 
 * Returns zero on success or nonzero on failure.
 */
int hostname_to_dotted_quad(char *addr, char *host) {
	struct hostent *ch;
	const char *i;
	int a1, a2, a3, a4;

	ch = gethostbyname(host);
	if (ch == NULL) {
		strcpy(addr, "0.0.0.0");
		return(1);
	}

	i = (const char *) ch->h_addr_list[0];
	a1 = ((*i++) & 0xff);
	a2 = ((*i++) & 0xff);
	a3 = ((*i++) & 0xff);
	a4 = ((*i++) & 0xff);
	sprintf(addr, "%d.%d.%d.%d", a1, a2, a3, a4);
	return(0);
}
