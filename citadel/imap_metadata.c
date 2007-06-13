/*
 * $Id:  $
 *
 * IMAP METADATA extension (yet another ugly disaster)
 *
 */


#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <pwd.h>
#include <errno.h>
#include <sys/types.h>

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

#include <sys/wait.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include "citadel.h"
#include "server.h"
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "serv_extensions.h"
#include "room_ops.h"
#include "user_ops.h"
#include "policy.h"
#include "database.h"
#include "msgbase.h"
#include "tools.h"
#include "internet_addressing.h"
#include "serv_imap.h"
#include "imap_tools.h"
#include "imap_fetch.h"
#include "imap_misc.h"
#include "genstamp.h"



/*
 * Implements the GETMETADATA command.
 (
 ( This is currently a stub which returns no data, because we are not yet
 * using any server annotations.
 */
void imap_getmetadata(int num_parms, char *parms[]) {

	cprintf("%s OK GETMETADATA complete\r\n", parms[0]);
	return;
}


/*
 * Implements the SETMETADATA command.
 *
 * This is currently a stub which fools the client into thinking that there
 * is no remaining space available to store annotations.
 */
void imap_setmetadata(int num_parms, char *parms[]) {

	cprintf("%s NO [METADATA TOOMANY] SETMETADATA failed\r\n", parms[0]);
	return;
}


