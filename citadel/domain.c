#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include "sysdep_decls.h"
#include "domain.h"

#define SMART_HOST	"gatekeeper.wdcs.com"

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


	/* If we're configured to send all mail to a smart-host, then our
	 * job here is really easy.
	 */
	if (1) {	/* FIX */
		strcpy(mxbuf, SMART_HOST);
		return(1);
	}

	/* No smart-host?  Look up the best MX for a site.
	 */
	ret = res_query(
		dest,
		C_IN, T_MX, answer, sizeof(answer)  );

	lprintf(9, "res_query() returned %d\n", ret);
	return(0); /* FIX not yet working!! */
}
