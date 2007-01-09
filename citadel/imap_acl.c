/*
 * $Id:  $
 *
 * Functions which implement RFC 2086 (IMAP ACL extension)
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
 * Implements the SETACL command.
 */
void imap_setacl(int num_parms, char *parms[]) {

	cprintf("%s BAD not yet implemented FIXME\r\n", parms[0]);
	return;
}


/*
 * Implements the DELETEACL command.
 */
void imap_deleteacl(int num_parms, char *parms[]) {

	cprintf("%s BAD not yet implemented FIXME\r\n", parms[0]);
	return;
}


/*
 * Implements the GETACL command.
 */
void imap_getacl(int num_parms, char *parms[]) {

	cprintf("%s BAD not yet implemented FIXME\r\n", parms[0]);
	return;
}


/*
 * Implements the LISTRIGHTS command.
 */
void imap_listrights(int num_parms, char *parms[]) {

	cprintf("%s BAD not yet implemented FIXME\r\n", parms[0]);
	return;
}


/*
 * Implements the MYRIGHTS command.
 */
void imap_myrights(int num_parms, char *parms[]) {

	cprintf("%s BAD not yet implemented FIXME\r\n", parms[0]);
	return;
}


