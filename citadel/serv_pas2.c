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
#include "sysdep_decls.h"
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "support.h"
#include "dynloader.h"
#include "user_ops.h"
#include "md5.h"
#include "tools.h"


void cmd_pas2(char *argbuf)
{
	char pw[SIZ];
	char hexstring[MD5_HEXSTRING_SIZE];
	

	if (!strcmp(CC->curr_user, NLI))
	{
		cprintf("%d You must enter a user with the USER command first.\n", ERROR);
		return;
	}
	
	if (CC->logged_in)
	{
		cprintf("%d Already logged in.\n", ERROR);
		return;
	}
	
	extract(pw, argbuf, 0);
	
	if (getuser(&CC->usersupp, CC->curr_user))
	{
		cprintf("%d Unable to find user record for %s.\n", ERROR, CC->curr_user);
		return;
	}
	
	strproc(pw);
	strproc(CC->usersupp.password);
	
	if (strlen(pw) != (MD5_HEXSTRING_SIZE-1))
	{
		cprintf("%d Auth string of length %ld is the wrong length (should be %d).\n", ERROR, (long)strlen(pw), MD5_HEXSTRING_SIZE-1);
		return;
	}
	
	make_apop_string(CC->usersupp.password, CC->cs_nonce, hexstring, sizeof hexstring);
	
	if (!strcmp(hexstring, pw))
	{
		do_login();
		return;
	}
	else
	{
		cprintf("%d Wrong password.\n", ERROR);
		return;
	}
}





char *serv_pas2_init(void)
{
        CtdlRegisterProtoHook(cmd_pas2, "PAS2", "APOP-based login");
        return "$Id$";
}
