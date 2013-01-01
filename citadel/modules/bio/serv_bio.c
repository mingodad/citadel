/*
 * This module implementsserver commands related to the display and
 * manipulation of user "bio" files.
 *
 * Copyright (c) 1987-2012 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "ctdl_module.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>


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
void cmd_lbio(char *cmdbuf)
{
	DIR *filedir = NULL;
	struct dirent *filedir_entry;
	struct dirent *d;
	int dont_resolve_uids;
	size_t d_namelen;
	struct ctdluser usbuf;
	int d_type = 0;


	d = (struct dirent *)malloc(offsetof(struct dirent, d_name) + PATH_MAX + 2);
	if (d == NULL) {
		cprintf("%d Cannot open listing.\n", ERROR + FILE_NOT_FOUND);
		return;
	}

	filedir = opendir (ctdl_bio_dir);
	if (filedir == NULL) {
		free(d);
		cprintf("%d Cannot open listing.\n", ERROR + FILE_NOT_FOUND);
		return;
	}
	dont_resolve_uids = *cmdbuf == '1';
        cprintf("%d\n", LISTING_FOLLOWS);
	while ((readdir_r(filedir, d, &filedir_entry) == 0) &&
	       (filedir_entry != NULL))
	{
#ifdef _DIRENT_HAVE_D_NAMLEN
		d_namelen = filedir_entry->d_namelen;

#else
		d_namelen = strlen(filedir_entry->d_name);
#endif

#ifdef _DIRENT_HAVE_D_TYPE
		d_type = filedir_entry->d_type;
#else

#ifndef DT_UNKNOWN
#define DT_UNKNOWN     0
#define DT_DIR         4
#define DT_REG         8
#define DT_LNK         10

#define IFTODT(mode)   (((mode) & 0170000) >> 12)
#define DTTOIF(dirtype)        ((dirtype) << 12)
#endif
		d_type = DT_UNKNOWN;
#endif
		if ((d_namelen == 1) && 
		    (filedir_entry->d_name[0] == '.'))
			continue;

		if ((d_namelen == 2) && 
		    (filedir_entry->d_name[0] == '.') &&
		    (filedir_entry->d_name[1] == '.'))
			continue;

		if (d_type == DT_UNKNOWN) {
			struct stat s;
			char path[PATH_MAX];
			snprintf(path, PATH_MAX, "%s/%s", 
				 ctdl_bio_dir, filedir_entry->d_name);
			if (lstat(path, &s) == 0) {
				d_type = IFTODT(s.st_mode);
			}
		}
		switch (d_type)
		{
		case DT_DIR:
			break;
		case DT_LNK:
		case DT_REG:
			if (dont_resolve_uids) {
				filedir_entry->d_name[d_namelen++] = '\n';
				filedir_entry->d_name[d_namelen] = '\0';
				client_write(filedir_entry->d_name, d_namelen);
			}
			else if (CtdlGetUserByNumber(&usbuf,atol(filedir_entry->d_name))==0)
				cprintf("%s\n", usbuf.fullname);
		}
	}
	free(d);
	closedir(filedir);
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
	/* return our module name for the log */
        return "bio";
}


