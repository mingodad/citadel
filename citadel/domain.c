#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include "sysdep_decls.h"
#include "domain.h"

#define SMART_HOST	"gatekeeper.wdcs.com"

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
			if (mxrecs[b].pref > mxrecs[b+1].pref) {

				memcpy(hold1, mxrefs[b], sizeof(struct mx));
				memcpy(hold2, mxrefs[b+1], sizeof(struct mx));
				memcpy(mxrefs[b], hold2, sizeof(struct mx));
				memcpy(mxrefs[b+1], hold1, sizeof(struct mx));
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
	char answer[1024];
	int ret;
	unsigned char *startptr, *endptr, *ptr;
	int expanded_size;
	char expanded_buf[1024];
	unsigned short pref, type;
	int n;
	HEADER *hp;
	int qdcount;

	struct mx *mxrecs = NULL;
	int num_mxrecs = 0;

	/* If we're configured to send all mail to a smart-host, then our
	 * job here is really easy.
	 */
	if (0) {	/* FIX */
		strcpy(mxbuf, SMART_HOST);
		return(1);
	}

	/* No smart-host?  Look up the best MX for a site.
	 */
	ret = res_query(
		dest,
		C_IN, T_MX, (unsigned char *)answer, sizeof(answer)  );

	if (ret < 0) {
		lprintf(5, "No MX found\n");
		return(0);
	}

	/* If we had to truncate, shrink the number to avoid fireworks */
	if (ret > sizeof(answer))
		ret = sizeof(answer);

	hp = (HEADER *)&answer[0];
	startptr = &answer[0];		/* start and end of buffer */
	endptr = &answer[ret];
	ptr = startptr + HFIXEDSZ;	/* advance past header */

	for (qdcount = ntohs(hp->qdcount); qdcount--; ptr += ret + QFIXEDSZ) {
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
			strcpy(mxrecs[num_mxrecs - 1].host, expanded_buf);
		}
	}

	lprintf(9, "unsorted...\n");
	for (n=0; n<num_mxrecs; ++n)
		lprintf(9, "%10d %s\n", mxrecs[n].pref, mxrecs[n].host);
	sort_mxrecs(mxrecs, num_mxrecs);
	lprintf(9, "sorted...\n");
	for (n=0; n<num_mxrecs; ++n)
		lprintf(9, "%10d %s\n", mxrecs[n].pref, mxrecs[n].host);

	strcpy(mxbuf, "");
	for (n=0; n<num_mxrecs; ++n) {
		strcat(mxbuf, mxrecs[n].host);
		strcat(mxbuf, "|");
	}
	phree(mxrecs);
	return(num_msrecs);
}
