/*
 * This module implementsserver commands related to the display and
 * manipulation of user "bio" files.
 *
 *
 * Copyright (c) 1987-2009 by the citadel.org team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

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
	while(client_getln(buf, sizeof buf) >= 0 && strcmp(buf,"000")) {
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
	if (CtdlGetUser(&ruser, buf) != 0) {
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
		if (CtdlGetUserByNumber(&usbuf,atol(buf))==0)
			cprintf("%s\n", usbuf.fullname);
	pclose(ls);
	cprintf("000\n");
}




CTDL_MODULE_INIT(bio)
{
	if (!threading)
	{
	        CtdlRegisterProtoHook(cmd_ebio, "EBIO", "Enter your bio");
        	CtdlRegisterProtoHook(cmd_rbio, "RBIO", "Read a user's bio");
	        CtdlRegisterProtoHook(cmd_lbio, "LBIO", "List users with bios");
	}
	/* return our Subversion id for the Log */
        return "bio";
}


