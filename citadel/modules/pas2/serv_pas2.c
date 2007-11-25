/*
 * cmd_pas2 - MD5 APOP style auth keyed off of the hash of the password
 *            plus a nonce displayed at the login banner.
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
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "support.h"
#include "user_ops.h"
#include "md5.h"


#include "ctdl_module.h"


void cmd_pas2(char *argbuf)
{
	char pw[256];
	char hexstring[MD5_HEXSTRING_SIZE];
	

	if (!strcmp(CC->curr_user, NLI))
	{
		cprintf("%d You must enter a user with the USER command first.\n", ERROR + USERNAME_REQUIRED);
		return;
	}
	
	if (CC->logged_in)
	{
		cprintf("%d Already logged in.\n", ERROR + ALREADY_LOGGED_IN);
		return;
	}
	
	extract_token(pw, argbuf, 0, '|', sizeof pw);
	
	if (getuser(&CC->user, CC->curr_user))
	{
		cprintf("%d Unable to find user record for %s.\n", ERROR + NO_SUCH_USER, CC->curr_user);
		return;
	}
	
	strproc(pw);
	strproc(CC->user.password);
	
	if (strlen(pw) != (MD5_HEXSTRING_SIZE-1))
	{
		cprintf("%d Auth string of length %ld is the wrong length (should be %d).\n", ERROR + ILLEGAL_VALUE, (long)strlen(pw), MD5_HEXSTRING_SIZE-1);
		return;
	}
	
	make_apop_string(CC->user.password, CC->cs_nonce, hexstring, sizeof hexstring);
	
	if (!strcmp(hexstring, pw))
	{
		do_login();
		return;
	}
	else
	{
		cprintf("%d Wrong password.\n", ERROR + PASSWORD_REQUIRED);
		return;
	}
}





CTDL_MODULE_INIT(pas2)
{
	if (!threading)
	{
	        CtdlRegisterProtoHook(cmd_pas2, "PAS2", "APOP-based login");
	}
	
	/* return our Subversion id for the Log */
        return "$Id$";
}
