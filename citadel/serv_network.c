/*
 * $Id$ 
 *
 * This module will eventually replace netproc and some of its utilities.
 * Copyright (C) 2000 by Art Cancro and others.
 * This code is released under the terms of the GNU General Public License.
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
#include "dynloader.h"
#include "room_ops.h"
#include "user_ops.h"
#include "policy.h"
#include "database.h"
#include "msgbase.h"
#include "tools.h"
#include "internet_addressing.h"
#include "serv_network.h"


void cmd_gnet(char *argbuf) {
	char filename[256];
	char buf[256];
	FILE *fp;

	if (CtdlAccessCheck(ac_room_aide)) return;
	assoc_file_name(filename, &CC->quickroom, "netconfigs");
	cprintf("%d Network settings for room #%ld <%s>\n",
		LISTING_FOLLOWS,
		CC->quickroom.QRnumber, CC->quickroom.QRname);

	fp = fopen(filename, "r");
	if (fp != NULL) {
		while (fgets(buf, sizeof buf, fp) != NULL) {
			buf[strlen(buf)-1] = 0;
			cprintf("%s\n", buf);
		}
		fclose(fp);
	}

	cprintf("000\n");
}


void cmd_snet(char *argbuf) {
	char tempfilename[256];
	char filename[256];
	char buf[256];
	FILE *fp;

	if (CtdlAccessCheck(ac_room_aide)) return;
	safestrncpy(tempfilename, tmpnam(NULL), sizeof tempfilename);
	assoc_file_name(filename, &CC->quickroom, "netconfigs");

	fp = fopen(tempfilename, "w");
	if (fp == NULL) {
		cprintf("%d Cannot open %s: %s\n",
			ERROR+INTERNAL_ERROR,
			tempfilename,
			strerror(errno));
	}

	cprintf("%d %s\n", SEND_LISTING, tempfilename);
	while (client_gets(buf), strcmp(buf, "000")) {
		fprintf(fp, "%s\n", buf);
	}
	fclose(fp);

	/* Now that we've got the whole file, put it in place */
	unlink(filename);
	link(tempfilename, filename);
	unlink(tempfilename);
}


char *Dynamic_Module_Init(void)
{
	CtdlRegisterProtoHook(cmd_gnet, "GNET", "Get network config");
	CtdlRegisterProtoHook(cmd_snet, "SNET", "Get network config");
	return "$Id$";
}
