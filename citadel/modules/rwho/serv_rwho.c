/*
 * $Id$
 *
 * This module implementsserver commands related to the display and
 * manipulation of the "Who's online" list.
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
#include <string.h>
#include <limits.h>
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "control.h"
#include "room_ops.h"
#include "user_ops.h"
#include "policy.h"
#include "database.h"
#include "msgbase.h"


#include "ctdl_module.h"


/*
 * display who's online
 */
void cmd_rwho(char *argbuf) {
	struct CitContext *cptr;
	int spoofed = 0;
	int user_spoofed = 0;
	int room_spoofed = 0;
	int host_spoofed = 0;
	int aide;
	char un[40];
	char real_room[ROOMNAMELEN], room[ROOMNAMELEN];
	char host[64], flags[5];
	
	aide = CC->user.axlevel >= 6;
	cprintf("%d%c \n", LISTING_FOLLOWS, CtdlCheckExpress() );
	
	for (cptr = ContextList; cptr != NULL; cptr = cptr->next) 
	{
		flags[0] = '\0';
		spoofed = 0;
		user_spoofed = 0;
		room_spoofed = 0;
		host_spoofed = 0;
		
		if (cptr->cs_flags & CS_POSTING)
		   strcat(flags, "*");
		else
		   strcat(flags, ".");
		   
		if (cptr->fake_username[0])
		{
		   strcpy(un, cptr->fake_username);
		   spoofed = 1;
		   user_spoofed = 1;
		}
		else
		   strcpy(un, cptr->curr_user);
		   
		if (cptr->fake_hostname[0])
		{
		   strcpy(host, cptr->fake_hostname);
		   spoofed = 1;
		   host_spoofed = 1;
		}
		else
		   strcpy(host, cptr->cs_host);

		GenerateRoomDisplay(real_room, cptr, CC);

		if (cptr->fake_roomname[0]) {
			strcpy(room, cptr->fake_roomname);
			spoofed = 1;
			room_spoofed = 1;
		}
		else {
			strcpy(room, real_room);
		}
		
                if ((aide) && (spoofed)) {
                	strcat(flags, "+");
		}
		
		if ((cptr->cs_flags & CS_STEALTH) && (aide)) {
			strcat(flags, "-");
		}
		
		if (((cptr->cs_flags&CS_STEALTH)==0) || (aide))
		{
			cprintf("%d|%s|%s|%s|%s|%ld|%s|%s|",
				cptr->cs_pid, un, room,
				host, cptr->cs_clientname,
				(long)(cptr->lastidle),
				cptr->lastcmdname, flags
			);

			if ((user_spoofed) && (aide)) {
				cprintf("%s|", cptr->curr_user);
			}
			else {
				cprintf("|");
			}
	
			if ((room_spoofed) && (aide)) {
				cprintf("%s|", real_room);
			}
			else {
				cprintf("|");
			}
	
			if ((host_spoofed) && (aide)) {
				cprintf("%s|", cptr->cs_host);
			}
			else {
				cprintf("|");
			}
	
			cprintf("%d\n", cptr->logged_in);
		}
	}

	/* Now it's magic time.  Before we finish, call any EVT_RWHO hooks
	 * so that external paging modules such as serv_icq can add more
	 * content to the Wholist.
	 */
	PerformSessionHooks(EVT_RWHO);
	cprintf("000\n");
}


/*
 * Masquerade roomname
 */
void cmd_rchg(char *argbuf)
{
	char newroomname[ROOMNAMELEN];

	extract_token(newroomname, argbuf, 0, '|', sizeof newroomname);
	newroomname[ROOMNAMELEN-1] = 0;
	if (!IsEmptyStr(newroomname)) {
		safestrncpy(CC->fake_roomname, newroomname,
			sizeof(CC->fake_roomname) );
	}
	else {
		safestrncpy(CC->fake_roomname, "", sizeof CC->fake_roomname);
	}
	cprintf("%d OK\n", CIT_OK);
}

/*
 * Masquerade hostname 
 */
void cmd_hchg(char *argbuf)
{
	char newhostname[64];

	extract_token(newhostname, argbuf, 0, '|', sizeof newhostname);
	if (!IsEmptyStr(newhostname)) {
		safestrncpy(CC->fake_hostname, newhostname,
			sizeof(CC->fake_hostname) );
	}
	else {
		safestrncpy(CC->fake_hostname, "", sizeof CC->fake_hostname);
	}
	cprintf("%d OK\n", CIT_OK);
}


/*
 * Masquerade username (aides only)
 */
void cmd_uchg(char *argbuf)
{

	char newusername[USERNAME_SIZE];

	extract_token(newusername, argbuf, 0, '|', sizeof newusername);

	if (CtdlAccessCheck(ac_aide)) return;

	if (!IsEmptyStr(newusername)) {
		CC->cs_flags &= ~CS_STEALTH;
		memset(CC->fake_username, 0, 32);
		if (strncasecmp(newusername, CC->curr_user,
				strlen(CC->curr_user)))
			safestrncpy(CC->fake_username, newusername,
				sizeof(CC->fake_username));
	}
	else {
		CC->fake_username[0] = '\0';
		CC->cs_flags |= CS_STEALTH;
	}
	cprintf("%d\n",CIT_OK);
}




/*
 * enter or exit "stealth mode"
 */
void cmd_stel(char *cmdbuf)
{
	int requested_mode;

	requested_mode = extract_int(cmdbuf,0);

	if (CtdlAccessCheck(ac_logged_in)) return;

	if (requested_mode == 1) {
		CC->cs_flags = CC->cs_flags | CS_STEALTH;
		PerformSessionHooks(EVT_STEALTH);
	}
	if (requested_mode == 0) {
		CC->cs_flags = CC->cs_flags & ~CS_STEALTH;
		PerformSessionHooks(EVT_UNSTEALTH);
	}

	cprintf("%d %d\n", CIT_OK,
		((CC->cs_flags & CS_STEALTH) ? 1 : 0) );
}


CTDL_MODULE_INIT(rwho)
{
	if(!threading)
	{
	        CtdlRegisterProtoHook(cmd_rwho, "RWHO", "Display who is online");
        	CtdlRegisterProtoHook(cmd_hchg, "HCHG", "Masquerade hostname");
	        CtdlRegisterProtoHook(cmd_rchg, "RCHG", "Masquerade roomname");
        	CtdlRegisterProtoHook(cmd_uchg, "UCHG", "Masquerade username");
	        CtdlRegisterProtoHook(cmd_stel, "STEL", "Enter/exit stealth mode");
	}
	
	/* return our Subversion id for the Log */
        return "$Id$";
}
