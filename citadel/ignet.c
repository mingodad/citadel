/*
 * $Id$
 *
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>
#include <pwd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <string.h>
#include <limits.h>
#include "citadel.h"
#include "server.h"
#include <time.h>
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "tools.h"
#include "msgbase.h"
#include "internet_addressing.h"
#include "ignet.h"
#include "user_ops.h"
#include "room_ops.h"
#include "parsedate.h"



/*
 * Returns nonzero if the specified nodename is on the Citadel network
 */

int is_ignet(char *node) {
	char filename[256];

	/* First, check to see if the specified node is a neighbor */
	sprintf(filename, "./network/systems/%s", node);
	if (!access(filename, F_OK)) return(1);

	/* If not, see if it's a non-neighbor node */
	/* FIXME add this */

	/* Nope */
	return 0;

}


int ignet_spool_to(char *node, long msgnum) {

	return 1;		/* FIXME */

}





