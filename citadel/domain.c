/*
 * $Id$
 *
 * DNS lookup for SMTP sender
 *
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <stdio.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include "sysdep_decls.h"
#include "citadel.h"
#include "domain.h"
#include "server.h"
#include "tools.h"
#include "internet_addressing.h"


/*
 * get_smarthosts() checks the Internet configuration for "smarthost"
 * entries and returns them in the same format as getmx() does -- fill the
 * buffer with a delimited list of hosts and return the number of hosts.
 */
int get_smarthosts(char *mxbuf) {
	int config_lines;
	int i;
	char buf[SIZ];
	char host[SIZ], type[SIZ];
	int total_smarthosts = 0;

	if (inetcfg == NULL) return(0);
	strcpy(mxbuf, "");

	config_lines = num_tokens(inetcfg, '\n');
	for (i=0; i<config_lines; ++i) {
		extract_token(buf, inetcfg, i, '\n');
		extract_token(host, buf, 0, '|');
		extract_token(type, buf, 1, '|');

		if (!strcasecmp(type, "smarthost")) {
			strcat(mxbuf, host);
			strcat(mxbuf, "|");
			++total_smarthosts;
		}
	}

	return(total_smarthosts);
}


/*
 * Compare the preference of two MX records.  First check by the actual
 * number listed in the MX record.  If they're identical, randomize the
 * result.
 */
inline int mx_compare_pref(int pref1, int pref2) {
	if (pref1 > pref2) {
		return(1);
	}
	else if (pref1 < pref2) {
		return(0);
	}
	else {
		return(rand() % 2);
	}
}


/*
 * sort_mxrecs()
 *
 * Sort a pile of MX records (struct mx, definted in domain.h) by preference
 *
 */
void sort_mxrecs(struct mx *mxrecs, int num_mxrecs) {
	int a, b;
	struct mx hold1, hold2;

	if (num_mxrecs < 2) return;

	/* do the sort */
	for (a = num_mxrecs - 2; a >= 0; --a) {
		for (b = 0; b <= a; ++b) {
			if (mx_compare_pref(mxrecs[b].pref,mxrecs[b+1].pref)) {
				memcpy(&hold1, &mxrecs[b], sizeof(struct mx));
				memcpy(&hold2, &mxrecs[b+1], sizeof(struct mx));
				memcpy(&mxrecs[b], &hold2, sizeof(struct mx));
				memcpy(&mxrecs[b+1], &hold1, sizeof(struct mx));
			}
		}
	}
}



/* 
 * getmx()
 *
 * Return one or more MX's for a mail destination.
 *
 * Upon success, it fills 'mxbuf' with one or more MX hosts, separated by
 * vertical bar characters, and returns the number of hosts as its return
 * value.  If no MX's are found, it returns 0.
 *
 */
int getmx(char *mxbuf, char *dest) {
	union {
			u_char bytes[1024];
			HEADER header;
    } answer;
	int ret;
	unsigned char *startptr, *endptr, *ptr;
	char expanded_buf[1024];
	unsigned short pref, type;
	int n = 0;
	int qdcount;

	struct mx *mxrecs = NULL;
	int num_mxrecs = 0;
	
	/* If we're configured to send all mail to a smart-host, then our
	 * job here is really easy.
	 */
	n = get_smarthosts(mxbuf);
	if (n > 0) return(n);

	/*
	 * No smart-host?  Look up the best MX for a site.
	 */
	ret = res_query(
		dest,
		C_IN, T_MX, (unsigned char *)answer.bytes, sizeof(answer)  );

	if (ret < 0) {
		mxrecs = mallok(sizeof(struct mx));
		mxrecs[0].pref = 0;
		strcpy(mxrecs[0].host, dest);
		num_mxrecs = 1;
	}
	else {

		/* If we had to truncate, shrink the number to avoid fireworks */
		if (ret > sizeof(answer))
			ret = sizeof(answer);
	
		startptr = &answer.bytes[0];		/* start and end of buffer */
		endptr = &answer.bytes[ret];
		ptr = startptr + HFIXEDSZ;	/* advance past header */
	
		for (qdcount = ntohs(answer.header.qdcount); qdcount--; ptr += ret + QFIXEDSZ) {
			if ((ret = dn_skipname(ptr, endptr)) < 0) {
				lprintf(9, "dn_skipname error\n");
				return(0);
			}
		}
	
		while(1) {
			memset(expanded_buf, 0, sizeof(expanded_buf));
			ret = dn_expand(startptr,
					endptr,
					ptr,
					expanded_buf,
					sizeof(expanded_buf)
					);
			if (ret < 0) break;
			ptr += ret;
	
			GETSHORT(type, ptr);
			ptr += INT16SZ + INT32SZ;
			GETSHORT(n, ptr);
	
			if (type != T_MX) {
				ptr += n;
			}
	
			else {
				GETSHORT(pref, ptr);
				ret = dn_expand(startptr,
						endptr,
						ptr,
						expanded_buf,
						sizeof(expanded_buf)
						);
				ptr += ret;
	
				++num_mxrecs;
				if (mxrecs == NULL) {
					mxrecs = mallok(sizeof(struct mx));
				}
				else {
					mxrecs = reallok(mxrecs,
					    (sizeof(struct mx) * num_mxrecs) );
				}
	
				mxrecs[num_mxrecs - 1].pref = pref;
				strcpy(mxrecs[num_mxrecs - 1].host,
				       expanded_buf);
			}
		}
	}

	sort_mxrecs(mxrecs, num_mxrecs);

	strcpy(mxbuf, "");
	for (n=0; n<num_mxrecs; ++n) {
		strcat(mxbuf, mxrecs[n].host);
		strcat(mxbuf, "|");
	}
	phree(mxrecs);
	return(num_mxrecs);
}
