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
#include "control.h"
#include "dynloader.h"
#include "room_ops.h"
#include "user_ops.h"
#include "policy.h"
#include "database.h"
#include "msgbase.h"
#include "tools.h"



/*
 * display who's online
 */
void cmd_rwho(char *argbuf) {
	struct CitContext *cptr;
	int spoofed = 0;
	int aide;
	char un[40];
	char real_room[ROOMNAMELEN], room[ROOMNAMELEN];
	char host[40], flags[5];
	
	aide = CC->usersupp.axlevel >= 6;
	cprintf("%d%c \n", LISTING_FOLLOWS, CtdlCheckExpress() );
	
	for (cptr = ContextList; cptr != NULL; cptr = cptr->next) 
	{
		flags[0] = '\0';
		spoofed = 0;
		
		if (cptr->cs_flags & CS_POSTING)
		   strcat(flags, "*");
		else
		   strcat(flags, ".");
		   
		if (cptr->fake_username[0])
		{
		   strcpy(un, cptr->fake_username);
		   spoofed = 1;
		}
		else
		   strcpy(un, cptr->curr_user);
		   
		if (cptr->fake_hostname[0])
		{
		   strcpy(host, cptr->fake_hostname);
		   spoofed = 1;
		}
		else
		   strcpy(host, cptr->cs_host);

		GenerateRoomDisplay(real_room, cptr, CC);

		if (cptr->fake_roomname[0]) {
			strcpy(room, cptr->fake_roomname);
			spoofed = 1;
		}
		else {
			strcpy(room, real_room);
		}
		
                if ((aide) && (spoofed))
                   strcat(flags, "+");
		
		if ((cptr->cs_flags & CS_STEALTH) && (aide))
		   strcat(flags, "-");
		
		if (((cptr->cs_flags&CS_STEALTH)==0) || (aide))
		{
			cprintf("%d|%s|%s|%s|%s|%ld|%s|%s\n",
				cptr->cs_pid, un, room,
				host, cptr->cs_clientname,
				(long)(cptr->lastidle),
				cptr->lastcmdname, flags);
		}
		if ((spoofed) && (aide))
		{
			cprintf("%d|%s|%s|%s|%s|%ld|%s|%s\n",
				cptr->cs_pid, cptr->curr_user,
				real_room,
				cptr->cs_host, cptr->cs_clientname,
				(long)(cptr->lastidle),
				cptr->lastcmdname, flags);
		
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
	char newroomname[256];

	extract(newroomname, argbuf, 0);
	newroomname[ROOMNAMELEN-1] = 0;
	if (strlen(newroomname) > 0) {
		safestrncpy(CC->fake_roomname, newroomname,
			sizeof(CC->fake_roomname) );
		}
	else {
		strcpy(CC->fake_roomname, "");
		}
	cprintf("%d OK\n", OK);
}

/*
 * Masquerade hostname 
 */
void cmd_hchg(char *argbuf)
{
	char newhostname[256];

	extract(newhostname, argbuf, 0);
	if (strlen(newhostname) > 0) {
		safestrncpy(CC->fake_hostname, newhostname,
			sizeof(CC->fake_hostname) );
		}
	else {
		strcpy(CC->fake_hostname, "");
		}
	cprintf("%d OK\n", OK);
}


/*
 * Masquerade username (aides only)
 */
void cmd_uchg(char *argbuf)
{

	char newusername[256];

	extract(newusername, argbuf, 0);

	if (CtdlAccessCheck(ac_aide)) return;

	if (strlen(newusername) > 0) {
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
	cprintf("%d\n",OK);
}




/*
 * enter or exit "stealth mode"
 */
void cmd_stel(char *cmdbuf)
{
	int requested_mode;

	requested_mode = extract_int(cmdbuf,0);
	if (requested_mode !=0) requested_mode = 1;

	if (CtdlAccessCheck(ac_aide)) return;

	if (CC->cs_flags & CS_STEALTH) {
		if (requested_mode == 0)
			CC->cs_flags = CC->cs_flags-CS_STEALTH;
		}
	else {
		if (requested_mode == 1)
			CC->cs_flags = CC->cs_flags|CS_STEALTH;
		}

	cprintf("%d Ok\n",OK);
	}







char *Dynamic_Module_Init(void)
{
        CtdlRegisterProtoHook(cmd_rwho, "RWHO", "Display who is online");
        CtdlRegisterProtoHook(cmd_hchg, "HCHG", "Masquerade hostname");
        CtdlRegisterProtoHook(cmd_rchg, "RCHG", "Masquerade roomname");
        CtdlRegisterProtoHook(cmd_uchg, "UCHG", "Masquerade username");
        CtdlRegisterProtoHook(cmd_stel, "STEL", "Enter/exit stealth mode");
        return "$Id$";
}
