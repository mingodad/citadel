/*
 * $Id$ 
 *
 * This module will eventually replace netproc and some of its utilities.
 * Copyright (C) 2000 by Art Cancro and others.
 * This code is released under the terms of the GNU General Public License.
 *
 */


/* FIXME

there's stuff in here that makes the assumption that /tmp is on the same
filesystem as Citadel, and makes calls to link() on that basis.  fix this.

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




/*
 * Batch up and send all outbound traffic from the current room
 */
void network_spoolout_current_room(void) {
	char filename[256];
	char buf[256];
	char instr[256];
	FILE *fp;
	long lastsent = 0L;
	struct namelist *listrecps = NULL;
	/* struct namelist *digestrecps = NULL; */
	struct namelist *nptr;

	assoc_file_name(filename, &CC->quickroom, "netconfigs");

	fp = fopen(filename, "r");
	if (fp == NULL) {
		lprintf(7, "Outbound batch processing skipped for <%s>\n",
			CC->quickroom.QRname);
		return;
	}

	lprintf(5, "Outbound batch processing started for <%s>\n",
		CC->quickroom.QRname);

	while (fgets(buf, sizeof buf, fp) != NULL) {
		buf[strlen(buf)-1] = 0;

		extract(instr, buf, 0);
		if (!strcasecmp(instr, "lastsent")) {
			lastsent = extract_long(buf, 1);
		}
		else if (!strcasecmp(instr, "listrecp")) {
			nptr = (struct namelist *)
				mallok(sizeof(struct namelist));
			nptr->next = listrecps;
			extract(nptr->name, buf, 1);
			listrecps = nptr;
		}


	}
	fclose(fp);


	/* Do something useful */







	/* Now rewrite the config file */
	fp = fopen(filename, "w");
	if (fp == NULL) {
		lprintf(1, "ERROR: cannot open %s: %s\n",
			filename, strerror(errno));
	}
	else {
		fprintf(fp, "lastsent|%ld\n", lastsent);

		/* Write out the listrecps while freeing from memory at the
		 * same time.  Am I clever or what?  :)
		 */
		while (listrecps != NULL) {
			fprintf(fp, "listrecp|%s\n", listrecps->name);
			nptr = listrecps->next;
			phree(listrecps);
			listrecps = nptr;
		}

		fclose(fp);
	}

	lprintf(5, "Outbound batch processing finished for <%s>\n",
		CC->quickroom.QRname);
}



/* FIXME temporary server command for batch send */
void cmd_batc(char *argbuf) {
	if (CtdlAccessCheck(ac_aide)) return;

	network_spoolout_current_room();

	cprintf("%d ok\n", OK);
}



char *Dynamic_Module_Init(void)
{
	CtdlRegisterProtoHook(cmd_gnet, "GNET", "Get network config");
	CtdlRegisterProtoHook(cmd_snet, "SNET", "Get network config");

	/* FIXME
	   temporary server command for batch send
	 */
	CtdlRegisterProtoHook(cmd_batc, "BATC", "send out batch (temp)");

	return "$Id$";
}
