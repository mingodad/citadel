/*
 * Functions which handle hostname/address lookups and resolution
 *
 * Copyright (c) 1987-2011 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
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
#include <arpa/inet.h>
#include <limits.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "locate_host.h"
#include "sysdep_decls.h"
#include "config.h"
#include "domain.h"
#include "context.h"
#include "ctdl_module.h"

#ifdef HAVE_RESOLV_H
#include <arpa/nameser.h>
#ifdef HAVE_ARPA_NAMESER_COMPAT_H
#include <arpa/nameser_compat.h>
#endif
#include <resolv.h>
#endif

/** START:some missing macros on OpenBSD 3.9 */
#ifndef NS_CMPRSFLGS
#define NS_CMPRSFLGS   0xc0
#endif
#if !defined(NS_MAXCDNAME) && defined (MAXCDNAME)
#define NS_MAXCDNAME MAXCDNAME
#endif
#if !defined(NS_INT16SZ) && defined(INT16SZ)
#define NS_INT16SZ INT16SZ
#define NS_INT32SZ INT32SZ
#endif
/** END:some missing macros on OpenBSD 3.9 */

/*
 * Given an open client socket, return the host name and IP address at the other end.
 * (IPv4 and IPv6 compatible)
 */
void locate_host(char *tbuf, size_t n, char *abuf, size_t na, int client_socket)
{
	struct sockaddr_in6 clientaddr;
	unsigned int addrlen = sizeof(clientaddr);

	tbuf[0] = 0;
	abuf[0] = 0;

	getpeername(client_socket, (struct sockaddr *)&clientaddr, &addrlen);
	getnameinfo((struct sockaddr *)&clientaddr, addrlen, tbuf, n, NULL, 0, 0);
	getnameinfo((struct sockaddr *)&clientaddr, addrlen, abuf, na, NULL, 0, NI_NUMERICHOST);

	/* Convert IPv6-mapped IPv4 addresses back to traditional dotted quad.
	 *
	 * Other code here, such as the RBL check, will expect IPv4 addresses to be represented
	 * as dotted-quad, even if they come in over a hybrid IPv6/IPv4 socket.
	 */
	if ( (strlen(abuf) > 7) && (!strncasecmp(abuf, "::ffff:", 7)) ) {
		if (!strcmp(abuf, tbuf)) strcpy(tbuf, &tbuf[7]);
		strcpy(abuf, &abuf[7]);
	}
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
	static int res_initted = 0;

	if (!res_initted) {		/* only have to do this once */
		res_init();
		res_initted = 1;
	}

	/* Make our DNS query. */
	answer = fixedans;
	if (server_shutting_down)
	{
		if (txtbuf != NULL) {
			snprintf(txtbuf, txtbufsize, "System shutting down");
		}
		return (1);
	}
	len = res_query(domain, C_IN, T_A, answer, PACKETSZ);

	/* Was there a problem? If so, the domain doesn't exist. */
	if (len == -1) {
		if (txtbuf != NULL) {
			strcpy(txtbuf, "");
		}
		return(0);
	}

	if( len > PACKETSZ )
	{
		answer = malloc(len);
		need_to_free_answer = 1;
		len = res_query(domain, C_IN, T_A, answer, len);
		if( len == -1 ) {
			if (txtbuf != NULL) {
				snprintf(txtbuf, txtbufsize,
					"Message rejected due to known spammer source IP address");
			}
			if (need_to_free_answer) free(answer);
			return(1);
		}
	}
	if (server_shutting_down)
	{
		if (txtbuf != NULL)
			snprintf(txtbuf, txtbufsize, "System shutting down");
		if (need_to_free_answer) free(answer);
		return (1);
	}

	result = (char *) malloc(RESULT_SIZE);
	result[ 0 ] = '\0';


	/* Make another DNS query for textual data; this shouldn't
	 * be a performance hit, since it'll now be cached at the
	 * nameserver we're using.
	 */
	len = res_query(domain, C_IN, T_TXT, answer, PACKETSZ);
	if (server_shutting_down)
	{
		if (txtbuf != NULL) {
			snprintf(txtbuf, txtbufsize, "System shutting down");
		}
		if (need_to_free_answer) free(answer);
		free(result);
		return (1);
	}

	/* Just in case there's no TXT record... */
	if (len ==(-1))
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
	 * skip. I wish there were good online documentation
	 * for programming for libresolv, so I'd know what I'm
	 * skipping here. Anyone reading this, feel free to
	 * enlighten me.
	 */
	cp += 1 + NS_INT16SZ + NS_INT32SZ;

	/* Skip the type, class and ttl. */
	cp += (NS_INT16SZ * 2) + NS_INT32SZ;

	/* Get the length and end of the buffer. */
	NS_GET16(c, cp);
	cend = cp + c;

	/* Iterate over any multiple answers we might have. In
	 * this context, it's unlikely, but anyway.
	 */
	rp = (u_char *) result;
	rend = (u_char *) result + RESULT_SIZE - 1;
	while (cp < cend && rp < rend)
	{
		a = *cp++;
		if( a != 0 )
			for (b = a; b > 0 && cp < cend && rp < rend; b--)
			{
				if (*cp == '\n' || *cp == '"' || *cp == '\\')
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
 * Check to see if the client host is on some sort of spam list (RBL)
 * If spammer, returns nonzero and places reason in 'message_to_spammer'
 */
int rbl_check(char *message_to_spammer)
{
	char tbuf[256] = "";
	int suffix_pos = 0;
	int rbl;
	int num_rbl;
	char rbl_domains[SIZ];
	char txt_answer[1024];

	strcpy(message_to_spammer, "ok");

	if ((strchr(CC->cs_addr, '.')) && (!strchr(CC->cs_addr, ':'))) {
		int a1, a2, a3, a4;

		sscanf(CC->cs_addr, "%d.%d.%d.%d", &a1, &a2, &a3, &a4);
		snprintf(tbuf, sizeof tbuf, "%d.%d.%d.%d.", a4, a3, a2, a1);
		suffix_pos = strlen(tbuf);
	}
	else if ((!strchr(CC->cs_addr, '.')) && (strchr(CC->cs_addr, ':'))) {
		int num_colons = 0;
		int i = 0;
		char workbuf[sizeof tbuf];
		char *ptr;

		/* tedious code to expand and reverse an IPv6 address */
		safestrncpy(tbuf, CC->cs_addr, sizeof tbuf);
		num_colons = haschar(tbuf, ':');
		if ((num_colons < 2) || (num_colons > 7)) return(0);	/* badly formed address */

		/* expand the "::" shorthand */
		while (num_colons < 7) {
			ptr = strstr(tbuf, "::");
			if (!ptr) return(0);				/* badly formed address */
			++ptr;
			strcpy(workbuf, ptr);
			strcpy(ptr, ":");
			strcat(ptr, workbuf);
			++num_colons;
		}

		/* expand to 32 hex characters with no colons */
		strcpy(workbuf, tbuf);
		strcpy(tbuf, "00000000000000000000000000000000");
		for (i=0; i<8; ++i) {
			char tokbuf[5];
			extract_token(tokbuf, workbuf, i, ':', sizeof tokbuf);

			memcpy(&tbuf[ (i*4) + (4-strlen(tokbuf)) ], tokbuf, strlen(tokbuf) );
		}
		if (strlen(tbuf) != 32) return(0);

		/* now reverse it and add dots */
		strcpy(workbuf, tbuf);
		for (i=0; i<32; ++i) {
			tbuf[i*2] = workbuf[31-i];
			tbuf[(i*2)+1] = '.';
		}
		tbuf[64] = 0;
		suffix_pos = 64;
	}
	else {
		return(0);	/* unknown address format */
	}

	/* See if we have any RBL domains configured */
	num_rbl = get_hosts(rbl_domains, "rbl");
	if (num_rbl < 1) return(0);

	/* Try all configured RBL's */
        for (rbl=0; rbl<num_rbl; ++rbl) {
                extract_token(&tbuf[suffix_pos], rbl_domains, rbl, '|', (sizeof tbuf - suffix_pos));

		if (rblcheck_backend(tbuf, txt_answer, sizeof txt_answer)) {
			strcpy(message_to_spammer, txt_answer);
			syslog(LOG_INFO, "RBL: %s\n", txt_answer);
			return(1);
		}
	}

	return(0);
}
			

/*
 * Convert a host name to a dotted quad address. 
 * Returns zero on success or nonzero on failure.
 *
 * FIXME this is obviously not IPv6 compatible.
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
