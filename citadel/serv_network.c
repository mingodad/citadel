/*
 * $Id$ 
 *
 * This module will eventually replace netproc and some of its utilities.  In
 * the meantime, it serves as a mailing list manager.
 *
 * Copyright (C) 2000-2001 by Art Cancro and others.
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
#include "citadel.h"
#include "server.h"
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
	char filename[SIZ];
	char buf[SIZ];
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
	char tempfilename[SIZ];
	char filename[SIZ];
	char buf[SIZ];
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

	/* Now copy the temp file to its permanent location
	 * (We use /bin/mv instead of link() because they may be on
	 * different filesystems)
	 */
	unlink(filename);
	snprintf(buf, sizeof buf, "/bin/mv %s %s", tempfilename, filename);
	system(buf);
}



/*
 * Spools out one message from the list.
 */
void network_spool_msg(long msgnum, void *userdata) {
	struct SpoolControl *sc;
	struct namelist *nptr;
	int err;
	char *instr = NULL;
	size_t instr_len = SIZ;
	struct CtdlMessage *imsg;

	sc = (struct SpoolControl *)userdata;

	/* If no recipients, bail out now.
	 * (May need to tweak this when we add other types of targets)
	 */
	if (sc->listrecps == NULL) return;
	
	/* First, copy it to the spoolout room */
	err = CtdlSaveMsgPointerInRoom(SMTP_SPOOLOUT_ROOM, msgnum, 0);
	if (err != 0) return;

	/* 
	 * Figure out how big a buffer we need to allocate
	 */
	for (nptr = sc->listrecps; nptr != NULL; nptr = nptr->next) {
		instr_len = instr_len + strlen(nptr->name);
	}

	/*
	 * allocate...
	 */
	lprintf(9, "Generating delivery instructions\n");
	instr = mallok(instr_len);
	if (instr == NULL) {
		lprintf(1, "Cannot allocate %d bytes for instr...\n",
			instr_len);
		abort();
	}
	sprintf(instr,
		"Content-type: %s\n\nmsgid|%ld\nsubmitted|%ld\n"
		"bounceto|postmaster@%s\n" ,
		SPOOLMIME, msgnum, time(NULL), config.c_fqdn );

	/* Generate delivery instructions for each recipient */
	for (nptr = sc->listrecps; nptr != NULL; nptr = nptr->next) {
		sprintf(&instr[strlen(instr)], "remote|%s|0||\n",
			nptr->name);
	}

	/*
	 * Generate a message from the instructions
	 */
       	imsg = mallok(sizeof(struct CtdlMessage));
	memset(imsg, 0, sizeof(struct CtdlMessage));
	imsg->cm_magic = CTDLMESSAGE_MAGIC;
	imsg->cm_anon_type = MES_NORMAL;
	imsg->cm_format_type = FMT_RFC822;
	imsg->cm_fields['A'] = strdoop("Citadel");
	imsg->cm_fields['M'] = instr;

	/* Save delivery instructions in spoolout room */
	CtdlSaveMsg(imsg, "", SMTP_SPOOLOUT_ROOM, MES_LOCAL);
	CtdlFreeMessage(imsg);

	/* update lastsent */
	sc->lastsent = msgnum;
}




/*
 * Batch up and send all outbound traffic from the current room
 */
void network_spoolout_room(struct quickroom *qrbuf, void *data) {
	char filename[SIZ];
	char buf[SIZ];
	char instr[SIZ];
	FILE *fp;
	struct SpoolControl sc;
	/* struct namelist *digestrecps = NULL; */
	struct namelist *nptr;

	memcpy(&CC->quickroom, qrbuf, sizeof(struct quickroom));

	memset(&sc, 0, sizeof(struct SpoolControl));
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
			sc.lastsent = extract_long(buf, 1);
		}
		else if (!strcasecmp(instr, "listrecp")) {
			nptr = (struct namelist *)
				mallok(sizeof(struct namelist));
			nptr->next = sc.listrecps;
			extract(nptr->name, buf, 1);
			sc.listrecps = nptr;
		}


	}
	fclose(fp);


	/* Do something useful */
	CtdlForEachMessage(MSGS_GT, sc.lastsent, (-63), NULL, NULL,
		network_spool_msg, &sc);


	/* Now rewrite the config file */
	fp = fopen(filename, "w");
	if (fp == NULL) {
		lprintf(1, "ERROR: cannot open %s: %s\n",
			filename, strerror(errno));
	}
	else {
		fprintf(fp, "lastsent|%ld\n", sc.lastsent);

		/* Write out the listrecps while freeing from memory at the
		 * same time.  Am I clever or what?  :)
		 */
		while (sc.listrecps != NULL) {
			fprintf(fp, "listrecp|%s\n", sc.listrecps->name);
			nptr = sc.listrecps->next;
			phree(sc.listrecps);
			sc.listrecps = nptr;
		}

		fclose(fp);
	}

	lprintf(5, "Outbound batch processing finished for <%s>\n",
		CC->quickroom.QRname);
}


/*
 * network_do_queue()
 * 
 * Run through the rooms doing various types of network stuff.
 */
void network_do_queue(void) {
	static int doing_queue = 0;
	static time_t last_run = 0L;

#define NETWORK_QUEUE_FREQUENCY 3600	/* one hour ... FIXME put in config */
	/*
	 * Run no more frequently than once every n seconds
	 */
	if ( (time(NULL) - last_run) < NETWORK_QUEUE_FREQUENCY ) return;

	/*
	 * This is a simple concurrency check to make sure only one queue run
	 * is done at a time.  We could do this with a mutex, but since we
	 * don't really require extremely fine granularity here, we'll do it
	 * with a static variable instead.
	 */
	if (doing_queue) return;
	doing_queue = 1;
	last_run = time(NULL);

	/* 
	 * Go ahead and run the queue
	 */
	lprintf(7, "network: processing outbound queue\n");
	ForEachRoom(network_spoolout_room, NULL);
	lprintf(7, "network: queue run completed\n");
	doing_queue = 0;
}


char *Dynamic_Module_Init(void)
{
	CtdlRegisterProtoHook(cmd_gnet, "GNET", "Get network config");
	CtdlRegisterProtoHook(cmd_snet, "SNET", "Get network config");
	CtdlRegisterSessionHook(network_do_queue, EVT_TIMER);
	return "$Id$";
}
