/*
 * $Id$
 *
 * This module implementsserver commands related to the display and
 * manipulation of user "bio" files.
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
#include "citadel_dirs.h"

#include "ctdl_module.h"

/*
 * enter user bio
 */
void cmd_ebio(char *cmdbuf) {
	char buf[SIZ];
	FILE *fp;

	unbuffer_output();

	if (!(CC->logged_in)) {
		cprintf("%d Not logged in.\n",ERROR + NOT_LOGGED_IN);
		return;
	}

	snprintf(buf, sizeof buf, "%s%ld",ctdl_bio_dir,CC->user.usernum);
	fp = fopen(buf,"w");
	if (fp == NULL) {
		cprintf("%d Cannot create file: %s\n", ERROR + INTERNAL_ERROR,
				strerror(errno));
		return;
	}
	cprintf("%d  \n",SEND_LISTING);
	while(client_getln(buf, sizeof buf), strcmp(buf,"000")) {
		if (ftell(fp) < config.c_maxmsglen) {
			fprintf(fp,"%s\n",buf);
		}
	}
	fclose(fp);
}

/*
 * read user bio
 */
void cmd_rbio(char *cmdbuf)
{
	struct ctdluser ruser;
	char buf[256];
	FILE *fp;

	extract_token(buf, cmdbuf, 0, '|', sizeof buf);
	if (getuser(&ruser, buf) != 0) {
		cprintf("%d No such user.\n",ERROR + NO_SUCH_USER);
		return;
	}
	snprintf(buf, sizeof buf, "%s%ld",ctdl_bio_dir,ruser.usernum);
	
	cprintf("%d OK|%s|%ld|%d|%ld|%ld|%ld\n", LISTING_FOLLOWS,
		ruser.fullname, ruser.usernum, ruser.axlevel,
		(long)ruser.lastcall, ruser.timescalled, ruser.posted);
	fp = fopen(buf,"r");
	if (fp == NULL)
		cprintf("%s has no bio on file.\n", ruser.fullname);
	else {
		while (fgets(buf, sizeof buf, fp) != NULL) cprintf("%s",buf);
		fclose(fp);
	}
	cprintf("000\n");
}

/*
 * list of users who have entered bios
 */
void cmd_lbio(char *cmdbuf) {
	char buf[256];
	FILE *ls;
	struct ctdluser usbuf;
	char listbios[256];

	snprintf(listbios, sizeof(listbios),"cd %s; ls",ctdl_bio_dir);
	ls = popen(listbios, "r");
	if (ls == NULL) {
		cprintf("%d Cannot open listing.\n", ERROR + FILE_NOT_FOUND);
		return;
	}

	cprintf("%d\n", LISTING_FOLLOWS);
	while (fgets(buf, sizeof buf, ls)!=NULL)
		if (getuserbynumber(&usbuf,atol(buf))==0)
			cprintf("%s\n", usbuf.fullname);
	pclose(ls);
	cprintf("000\n");
}




CTDL_MODULE_INIT(bio)
{
        CtdlRegisterProtoHook(cmd_ebio, "EBIO", "Enter your bio");
        CtdlRegisterProtoHook(cmd_rbio, "RBIO", "Read a user's bio");
        CtdlRegisterProtoHook(cmd_lbio, "LBIO", "List users with bios");

	/* return our Subversion id for the Log */
        return "$Id$";
}


