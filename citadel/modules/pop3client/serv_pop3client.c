/*
 * Aggregate remote POP3 accounts
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <ctype.h>
#include <string.h>
#include <errno.h>
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "support.h"
#include "user_ops.h"
#include "md5.h"
#include "tools.h"


#include "ctdl_module.h"


/*
void pop3client_scan(void) {
	lprintf(CTDL_DEBUG, "pop3client started\n");
	lprintf(CTDL_DEBUG, "pop3client ended\n");
}
*/


CTDL_MODULE_INIT(pop3client)
{
	/* CtdlRegisterSessionHook(pop3client_scan, EVT_TIMER); */

	/* return our Subversion id for the Log */
        return "$Id:  $";
}
